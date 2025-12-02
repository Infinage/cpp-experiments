#include "../include/torrent_downloader.hpp"
#include "../include/common.hpp"
#include "../include/protocol.hpp"

#include "../../networking/net.hpp"

#include <cstring>
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
        ctx.haves = readBitField(payload);
    }

    void TorrentDownloader::handlePiece(const std::string &payload, PeerContext &ctx) {
        // Payload: {index: int32, begin: int32, block: char*}
        std::uint32_t pIndex, pBegin;

        // Check torrent.blockSize == payload's block size
        std::memcpy(&pIndex, payload.c_str() + 0, 4);
        std::memcpy(&pBegin, payload.c_str() + 4, 4);
        net::utils::inplace_bswap(pIndex, pBegin);

        if (!ctx.pending.contains({pIndex, pBegin})) {
            std::println("Piece# {}, Block Offset {} was not requested yet or has already "
                "been downloaded, dropping.", pIndex, pBegin);
            return;
        }

        // Convert the offset into a valid block index
        ctx.pending.erase({pIndex, pBegin}); --ctx.backlog;
        bool validBlock {payload.size() - 8 == blockSize};
        std::println("{} Piece: {}, Block Offset: {} parsed from client {}", 
            validBlock? "Valid": "Invalid", pIndex, pBegin, ctx.str());

        // Notify piece manager that we have received a block
        if (validBlock) {
            auto [pieceDone, piece] {pieceManager.onBlockReceived(pIndex, pBegin, payload)};
            if (pieceDone) {
                DownloadTempFile.seekp(pIndex * torrentFile.pieceSize, std::ios::beg);
                DownloadTempFile.write(piece.c_str(), torrentFile.pieceSize);
                std::println("Piece# {} has completed downloading", pIndex);
            }
        }
    }

    void TorrentDownloader::handleChoke(const std::string&, PeerContext &ctx) {
        // Reset the peer context
        clearPendingFromPeer(ctx);
        ctx.choked = true; ctx.backlog = 0; 
    }

    void TorrentDownloader::handleUnchoke(const std::string&, PeerContext &ctx) {
        ctx.unchokeAttempts = 0; ctx.choked = false;
    }

    void TorrentDownloader::clearPendingFromPeer(PeerContext &ctx) {
        // Reset any download pending from this peer
        pieceManager.onPeerReset({ctx.pending.begin(), ctx.pending.end()});
        ctx.pending.clear();
    }

    TorrentDownloader::~TorrentDownloader() {
        const auto &haves {pieceManager.getHaves()};
        std::string bitField {writeBitField(haves)}; 
        std::ofstream ofs {StateSavePath, std::ios::binary};
        ofs.write(bitField.c_str(), static_cast<long>(bitField.size()));
        std::println("Download state saved to disk, {} pieces were completed", haves.size());
    }

    TorrentDownloader::TorrentDownloader(
        const TorrentFile &torrentFile, const std::string_view downloadDir, 
        const std::uint16_t bSize, const std::uint8_t backlog, 
        const std::uint8_t unchokeAttempts
    ): 
        torrentFile {torrentFile},
        peerID{generatePeerID()},
        blockSize {bSize},
        MAX_BACKLOG {backlog}, 
        MAX_UNCHOKE_ATTEMPTS {unchokeAttempts},
        pieceManager {torrentFile.length, torrentFile.pieceSize, bSize, torrentFile.pieceBlob},
        DownloadDir {downloadDir},
        StateSavePath {DownloadDir / ("." + torrentFile.name + ".ctorrent")}
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

        // If save found reload the state
        if (std::filesystem::exists(StateSavePath)) {
            if (!std::filesystem::is_regular_file(StateSavePath))
                throw std::runtime_error("Download state save path is invalid");

            std::ifstream saveFile {StateSavePath, std::ios::binary};
            std::string bitFieldString {std::istreambuf_iterator<char>(saveFile), 
                std::istreambuf_iterator<char>()};
            auto _haves {readBitField(bitFieldString)};
            if (*std::ranges::max_element(_haves) >= torrentFile.numPieces)
                std::println("Download state save is corrupted, will be overwritten");
            else {
                std::println("Download state reloaded, {} no of pieces have been completed", _haves.size());
                pieceManager.getHaves() = std::move(_haves);
            }
        }
    }

    void TorrentDownloader::download(std::vector<std::pair<std::string, std::uint16_t>> &peerList) {
        if (peerList.empty()) { std::println(std::cerr, "No peers available"); return; }
        std::println("Discovered {} peers.", peerList.size());

        // Connect to all available peers
        std::string handshake {buildHandshake(torrentFile.infoHash, peerID)};
        net::PollManager pollManager; std::unordered_map<int, PeerContext> states;
        for (const auto &[ip, port]: peerList) {
            std::optional<net::IP> ipType {net::utils::checkIPType(ip)};
            if (!ipType.has_value()) { std::println(std::cerr, "Invalid IP: {}", ip); continue; }
            auto peer {net::Socket{net::SOCKTYPE::TCP, ipType.value()}};
            peer.setNonBlocking(); peer.connect(ip, port);
            states.insert({peer.fd(), {.fd=peer.fd(), .ip=ip, .port=port, .sendBuffer=handshake}});
            pollManager.track(std::move(peer), net::PollEventType::Writable);
            if (pollManager.size() == 3) break; // TODO: debug easier
        }

        while (!pieceManager.finished() && !pollManager.empty()) {
            for (auto &[peer, event]: pollManager.poll(5)) {
                PeerContext &ctx {states.at(peer.fd())};
                ctx.lastActivity = std::chrono::steady_clock::now();

                if (!ctx.closed && event & net::PollEventType::Readable) {
                    std::string recvBytes;
                    try { recvBytes = peer.recvAll(); } 
                    catch (net::SocketError &err) { 
                        std::println("Recv failed for {}, reason: {}", ctx.str(), err.what()); 
                        ctx.closed = true;
                    }

                    if (!recvBytes.empty()) {
                        std::println("Recv {} bytes from client: {}", recvBytes.size(), ctx.str());
                        std::size_t iSize {ctx.recvBuffer.size()}; // Get existing buffer size to expand recvBuffer
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
                                ctx.sendBuffer = buildInterested();
                            }
                        }

                        if (ctx.handshaked) {
                            std::uint32_t msgLen;
                            while ((msgLen = IsCompleteMessage(ctx.recvBuffer)) > 0 && !ctx.closed) {
                                auto [msgType, message] {parseMessage(std::string_view{ctx.recvBuffer.data(), msgLen + 4})};
                                std::println("Parsed message of type: {} from client: {}", str(msgType), ctx.str());

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
                                    std::string unchokeMsg {buildUnchoke()};
                                    if (ctx.unchokeAttempts > MAX_UNCHOKE_ATTEMPTS) {
                                        std::println("Exceeded max unchoke attempts, disconnecting {}", ctx.str());
                                        ctx.closed = true;
                                    }

                                    // If buffer already has unchoke, we haven't had
                                    // a chance to send to client yet, don't increase attempts
                                    else if (ctx.sendBuffer != unchokeMsg) {
                                        std::println("Building unchoke for {}", ctx.str());
                                        ctx.sendBuffer = unchokeMsg;
                                        ++ctx.unchokeAttempts;
                                    }
                                }

                                // If unchoked & peer not throttled, try requesting for block(s) available from peer
                                else if (ctx.backlog < MAX_BACKLOG) {
                                    auto pendingBlocks {pieceManager.getPendingBlocks(ctx.haves, MAX_BACKLOG - ctx.backlog)};
                                    for (auto [pieceIdx, blockOffset, blockSize]: pendingBlocks) {
                                        std::println("Building request for piece# {}, block Offset {} from {}", 
                                                pieceIdx, blockOffset, ctx.str());
                                        ctx.sendBuffer += buildRequest(pieceIdx, blockOffset, blockSize);
                                        ctx.pending.emplace(pieceIdx, blockOffset);
                                        ++ctx.backlog;
                                    }
                                }

                                ctx.recvBuffer = ctx.recvBuffer.substr(msgLen + 4);
                            } // while IsCompleteMessage && !ctx.closed

                        } // if ctx.handshaked

                    } // if iSize

                } // if !ctx.closed && event is readable

                if (!ctx.closed && event & net::PollEventType::Writable) {
                    try {
                        long sentBytes {ctx.sendBuffer.empty()? 0: peer.sendAll(ctx.sendBuffer)};
                        if (sentBytes) {
                            std::println("Sent {} bytes to client: {}", sentBytes, ctx.str());
                            ctx.sendBuffer = ctx.sendBuffer.substr(static_cast<std::size_t>(sentBytes));
                        }
                    } catch (net::SocketError &err) { 
                        std::println("Send failed for {}, reason: {}", ctx.str(), err.what()); 
                        ctx.closed = true;
                    }
                }

                // Drop client on closed or err event
                if (event & net::PollEventType::Error || event & net::PollEventType::Closed) { 
                    std::println("Got an closed/err poll event from {}", ctx.str());
                    ctx.closed = true;
                }

                // Close the connection if already dropped, erase all pending states
                if (ctx.closed || peer.fd() == -1) {
                    std::println("Dropping client {}", ctx.str());
                    clearPendingFromPeer(ctx);
                    pollManager.untrack(ctx.fd);
                    states.erase(ctx.fd);
                }

                // Only track for events we are interested in
                else {
                    if (ctx.sendBuffer.empty())
                        pollManager.updateTracking(peer.fd(), net::PollEventType::Readable);
                    else if (ctx.handshaked)
                        pollManager.updateTracking(peer.fd(), net::PollEventType::Readable | net::PollEventType::Writable);
                }

            }
        }

        // Display status to user
        std::println("Download status: {}", (pieceManager.finished()? "Completed": "Failed"));
    }
};
