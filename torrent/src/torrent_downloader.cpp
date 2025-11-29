#include "../include/torrent_downloader.hpp"
#include "../include/common.hpp"
#include "../include/protocol.hpp"

#include "../../cryptography/hashlib.hpp"

#include <iostream>
#include <print>

namespace Torrent {
    void TorrentDownloader::handleHave(const std::string &payload, PeerContext &ctx) {
        std::uint32_t pieceIdx;
        std::memcpy(&pieceIdx, payload.c_str(), 4);
        ctx.haves.insert(net::utils::bswap(pieceIdx));
    }

    void TorrentDownloader::handleBitfield(const std::string &payload, PeerContext &ctx) {
        if (payload.size() != (torrentFile.numPieces + 7) / 8)
            std::println(std::cerr, "Bitfield received from client "
                "has invalid length: {}", payload.size());

        std::uint32_t bitIdx {};
        for (char ch: payload) {
            std::uint8_t byte {static_cast<std::uint8_t>(ch)};
            for (std::uint8_t bit {8}; bit-- > 0; ++bitIdx) {
                if (byte & (1u << bit)) 
                    ctx.haves.insert(bitIdx);
            }
        }
    }

    void TorrentDownloader::handlePiece(const std::string &payload, PeerContext &ctx) {
        // Payload: {index: int32, begin: int32, block: char*}
        std::uint32_t pIndex, pBegin;

        // Check torrent.blockSize == payload's block size
        bool validBlock {payload.size() - 8 != blockSize};
        std::string buffer(blockSize, '\0');
        std::memcpy(&pIndex, payload.c_str() + 0, 4);
        std::memcpy(&pBegin, payload.c_str() + 4, 4);
        net::utils::inplace_bswap(pIndex, pBegin);

        // Convert the offset into a valid block index
        std::uint32_t bIndex {pBegin / blockSize};
        ctx.pending.erase({pIndex, bIndex}); --ctx.backlog;
        if (!validBlock)
            std::println(std::cerr, "Block# {} size received from {} for piece# {}, "
                "is invalid, will be dropped", bIndex, ctx.str(), pIndex);
        else {
            std::memcpy(buffer.data(), payload.c_str() + 8, blockSize);
            pending[pIndex][bIndex] = std::move(buffer);
        }

        // If all blocks of a piece are done, write to file and add to haves
        long processedCount {std::ranges::count_if(pending[pIndex], [bsize = blockSize]
            (auto &str) { return str.second.size() == bsize; })};
        if (static_cast<std::size_t>(processedCount) == numBlocks) {
            // Gather all the blocks into same buffer
            std::string piece(torrentFile.pieceSize, '\0');
            for (std::uint32_t blockIdx {}; blockIdx < numBlocks; ++blockIdx) {
                const std::string &block {pending[pIndex][blockIdx]};
                std::memcpy(piece.data() + (blockIdx * blockSize), block.c_str(), block.size());
            }

            // Validate that the piece is valid against the hash
            if (hashutil::sha1(piece) != torrentFile.getPieceHash(pIndex)) {
                std::println("Mismatching hash for piece# {} will be dropped", pIndex);
            } else {
                DownloadTempFile.seekp(pIndex * torrentFile.pieceSize, std::ios::beg);
                DownloadTempFile.write(piece.c_str(), torrentFile.pieceSize);
                haves.insert(pIndex);
                std::println("Piece# {} has completed downloading", pIndex);
            }

            pending.erase(pIndex);
        }
    }

    void TorrentDownloader::handleChoke(const std::string&, PeerContext &ctx) {
        // Reset any download pending from this peer
        for (const auto &pb: ctx.pending) {
            pending[pb.first].erase(pb.second);
            if (pending[pb.first].empty()) 
                pending.erase(pb.first);
        }
        // Reset the peer context
        ctx.choked = true; ctx.backlog = 0; 
        ctx.pending.clear();
    }

    void TorrentDownloader::handleUnchoke(const std::string&, PeerContext &ctx) {
        ctx.unchokeAttempts = 0; ctx.choked = false;
    }

    TorrentDownloader::TorrentDownloader(
        const TorrentFile &torrentFile, const std::string_view downloadDir, 
        const std::uint16_t bSize, const std::uint8_t backlog, 
        const std::uint8_t unchokeAttempts
    ): 
        torrentFile {torrentFile}, 
        peerID{generatePeerID()}, 
        blockSize {bSize}, 
        numBlocks {(torrentFile.pieceSize + blockSize - 1) / blockSize},
        MAX_BACKLOG {backlog}, MAX_UNCHOKE_ATTEMPTS {unchokeAttempts},
        DownloadDir {downloadDir}
    {
        // Create download directory if it doesn't already exist
        if (!std::filesystem::exists(DownloadDir))
            std::filesystem::create_directory(DownloadDir);
        if (!std::filesystem::is_directory(DownloadDir))
            throw std::runtime_error("Download directory provided is not a valid folder path");

        // Create a temp sparse file for saving the pieces
        DownloadTempFile = std::ofstream {DownloadDir / torrentFile.name, std::ios::binary};
        DownloadTempFile.seekp(static_cast<long long>(torrentFile.length - 1)); 
        DownloadTempFile.put('\0');
    }

    void TorrentDownloader::download(std::vector<std::pair<std::string, std::uint16_t>> &peerList) {
        if (peerList.empty()) { std::println(std::cerr, "No peers available"); return; }
        std::println("Discovered {} peers.", peerList.size());

        // Connect to all available peers
        std::string handshake {buildHandshake(torrentFile.infoHash, peerID)};
        net::PollManager manager; std::unordered_map<int, PeerContext> states;
        for (const auto &[ip, port]: peerList) {
            std::optional<net::IP> ipType {net::utils::checkIPType(ip)};
            if (!ipType.has_value()) { std::println(std::cerr, "Invalid IP: {}", ip); continue; }
            auto peer {net::Socket{net::SOCKTYPE::TCP, ipType.value()}};
            peer.setNonBlocking(); peer.connect(ip, port);
            states.insert({peer.fd(), {.fd=peer.fd(), .ip=ip, .port=port, .sendBuffer=handshake}});
            manager.track(std::move(peer));
        }

        while (haves.size() < torrentFile.numPieces) {
            for (auto &[peer, event]: manager.poll()) {
                PeerContext &ctx {states.at(peer.fd())};
                ctx.lastActivity = std::chrono::steady_clock::now();

                if (event & net::PollEventType::Readable) {
                    std::string recvBytes {peer.recvAll()}; std::size_t iSize {ctx.recvBuffer.size()};
                    ctx.recvBuffer.resize(iSize + recvBytes.size());
                    std::memmove(ctx.recvBuffer.data() + iSize, recvBytes.data(), recvBytes.size());

                    if (!ctx.handshaked && ctx.recvBuffer.size() >= 68) {
                        ctx.handshaked = std::memcmp(handshake.data(), ctx.recvBuffer.data(), 20) == 0 
                            && std::memcmp(handshake.data() + 28, ctx.recvBuffer.data() + 28, 20) == 0;
                        if (!ctx.handshaked) {
                            std::println("Failed handshake with {}, client will be dropped", ctx.str());
                            ctx.closed = true;
                        } else {
                            ctx.recvBuffer = ctx.recvBuffer.substr(68);
                            std::println("Handshake established with client: {}", ctx.str());
                            ctx.sendBuffer = buildUnchoke() + buildInterested();
                        }
                    }

                    else if (ctx.handshaked) {
                        while (std::uint32_t msgLen = IsCompleteMessage(ctx.recvBuffer)) {
                            auto [msgType, message] {parseMessage(std::string_view{ctx.recvBuffer.data(), msgLen})};
                            std::println("Received message of type: {} from client: {}", str(msgType), ctx.str());

                            // Process the message and update the internal context
                            switch (msgType) {
                                case MsgType::Choke:       handleChoke(message, ctx); break;
                                case MsgType::Unchoke:   handleUnchoke(message, ctx); break;
                                case MsgType::Have:         handleHave(message, ctx); break;
                                case MsgType::Piece:       handlePiece(message, ctx); break;
                                case MsgType::Bitfield: handleBitfield(message, ctx); break;
                            }

                            // If choked, request for gettin unchoked. We will wait for a 
                            // maximum of 3 turns before disconnecting from the peer
                            if (ctx.choked) {
                                if (ctx.unchokeAttempts > MAX_UNCHOKE_ATTEMPTS) ctx.closed = true;
                                else {
                                    ctx.sendBuffer = buildUnchoke();
                                    ++ctx.unchokeAttempts;
                                }
                            }

                            // If unchoked & peer not throttled, try requesting for block(s) available from peer
                            else if (ctx.backlog < MAX_BACKLOG) {
                                for (std::uint32_t pHave: ctx.haves) {
                                    if (ctx.backlog >= MAX_BACKLOG) break;
                                    if (haves.count(pHave) == 0) {
                                        for (std::uint32_t block {}; block < numBlocks; ++block) {
                                            if (pending[pHave].count(block) == 0) {
                                                ctx.sendBuffer += buildRequest(pHave, block, this->blockSize);
                                                ++ctx.backlog;
                                                ctx.pending.insert({pHave, block});
                                                pending[pHave].insert({block, ""});
                                                if (ctx.backlog >= MAX_BACKLOG) break;
                                            }
                                        }
                                    }
                                }
                            }

                            ctx.recvBuffer = ctx.recvBuffer.substr(msgLen);
                            if (ctx.closed) {
                                manager.untrack(ctx.fd), 
                                    states.erase(ctx.fd); 
                                break;
                            }
                        }
                    }
                }

                else if (event & net::PollEventType::Writable) {
                    long sentBytes {peer.sendAll(ctx.sendBuffer)};
                    std::println("Sent {} bytes to client: {}", sentBytes, ctx.str());
                    ctx.sendBuffer = ctx.sendBuffer.substr(static_cast<std::size_t>(sentBytes));
                }

                else {
                    manager.untrack(ctx.fd);
                    states.erase(ctx.fd);
                }
            }
        }
    }
};
