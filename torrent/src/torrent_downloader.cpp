#include "../include/torrent_downloader.hpp"
#include "../include/common.hpp"
#include "../include/protocol.hpp"

#include "../../networking/net.hpp"
#include "../../misc/logger.hpp"

#include <csignal>
#include <cstring>
#include <iostream>

namespace Torrent {
    void TorrentDownloader::handleHave(const std::string &payload, PeerContext &ctx) {
        std::uint32_t pieceIdx;
        std::memcpy(&pieceIdx, payload.c_str(), 4);
        pieceIdx = net::utils::bswap(pieceIdx);
        ctx.haves.insert(pieceIdx);
        Logging::Dynamic::Trace("[{}] Client has Piece #{}", ctx.ID, pieceIdx);
    }

    void TorrentDownloader::handleBitfield(const std::string &payload, PeerContext &ctx) {
        if (payload.size() != (torrentFile.numPieces + 7) / 8) {
            Logging::Dynamic::Debug("[{}] Bitfield received has invalid length: {}, "
                    "will be dropped", ctx.ID, payload.size());
            return;
        }
        ctx.haves = readBitField(payload);
        Logging::Dynamic::Trace("[{}] Client has Pieces #{}", ctx.ID, ctx.haves.size());
    }

    void TorrentDownloader::handlePiece(const std::string &payload, PeerContext &ctx) {
        // Payload: {index: int32, begin: int32, block: char*}
        std::uint32_t pIndex, pBegin;

        // Check torrent.blockSize == payload's block size
        std::memcpy(&pIndex, payload.c_str() + 0, 4);
        std::memcpy(&pBegin, payload.c_str() + 4, 4);
        net::utils::inplace_bswap(pIndex, pBegin);

        auto pendingIt {ctx.pending.find({pIndex, pBegin, 0})};
        if (pendingIt == ctx.pending.end()) {
            Logging::Dynamic::Debug("[{}] Piece# {}, Block Offset {} was not requested "
                "yet or has already been downloaded, dropping.", ctx.ID, pIndex, pBegin);
            return;
        }

        // Convert the offset into a valid block index
        bool validBlock {payload.size() - 8 == pendingIt->blockSize};
        ctx.pending.erase(pendingIt); --ctx.backlog;
        Logging::Dynamic::Debug("[{}] Piece: {}, Block Offset {} => {}", ctx.ID, 
            pIndex, pBegin, validBlock? "Valid": "Invalid");

        // Notify piece manager that we have received a block
        if (validBlock) {
            auto [pieceReady, piece] {pieceManager.onBlockReceived(pIndex, pBegin, payload)};
            if (pieceReady) {
                diskWriter.schedule(pIndex * torrentFile.pieceSize, std::move(piece));
            }
        }
    }

    // Reset the peer context
    void TorrentDownloader::handleChoke(const std::string&, PeerContext &ctx) {
        clearPendingFromPeer(ctx); ctx.choked = true;
    }

    void TorrentDownloader::handleUnchoke(const std::string&, PeerContext &ctx) {
        ctx.unchokeAttempts = 0; ctx.choked = false;
    }

    void TorrentDownloader::clearPendingFromPeer(PeerContext &ctx) {
        // Reset any download pending from this peer
        pieceManager.onPeerReset({ctx.pending.begin(), ctx.pending.end()});
        ctx.pending.clear(); ctx.backlog = 0;
    }

    TorrentDownloader::~TorrentDownloader() {
        const auto &haves {pieceManager.getHaves()};
        if (haves.empty()) return;
        std::string bitField {writeBitField(haves)}; 
        std::ofstream ofs {StateSavePath, std::ios::binary};
        ofs.write(bitField.c_str(), static_cast<long>(bitField.size()));
        Logging::Dynamic::Info("Download state saved to disk, {}/{} pieces "
            "were completed", haves.size(), torrentFile.numPieces);
    }

    TorrentDownloader::TorrentDownloader(
        TorrentTracker &tTracker, const std::filesystem::path downloadDir, 
        const std::uint16_t bSize, const std::uint8_t backlog, 
        const std::uint8_t maxUnchokeAttempts, 
        const std::uint8_t maxReconnectAttempts, 
        const std::uint16_t maxReqWaitTime,
        const std::uint16_t minReconWaitTime
    ): 
        torrentFile {tTracker.torrentFile},
        torrentTracker {tTracker},
        peerID{generatePeerID()},
        blockSize {bSize},
        MAX_REQ_WAIT_TIME {maxReqWaitTime},
        MIN_RECON_WAIT_TIME {minReconWaitTime},
        MAX_BACKLOG {backlog}, 
        MAX_UNCHOKE_ATTEMPTS {maxUnchokeAttempts},
        MAX_RECONNECT_ATTEMPTS {maxReconnectAttempts},
        StateSavePath {downloadDir / ("." + torrentFile.name + ".ctorrent")},
        coldStart {!std::filesystem::exists(StateSavePath)},
        pieceManager {torrentFile.length, torrentFile.pieceSize, bSize, torrentFile.pieceBlob},
        diskWriter {torrentFile.name, torrentFile.length, torrentFile.pieceSize, downloadDir, coldStart}
    {
        // If save found reload the state
        if (!coldStart) {
            if (!std::filesystem::is_regular_file(StateSavePath))
                throw std::runtime_error("Download state save path is invalid");

            std::ifstream saveFile {StateSavePath, std::ios::binary};
            std::string bitFieldString {std::istreambuf_iterator<char>(saveFile), 
                std::istreambuf_iterator<char>()};
            auto _haves {readBitField(bitFieldString)};
            if (_haves.empty() || *std::ranges::max_element(_haves) >= torrentFile.numPieces)
                Logging::Dynamic::Warn("Download state save is corrupted, will be overwritten");
            else {
                Logging::Dynamic::Info("Download state reloaded, {}/{} pieces "
                    "have been completed", _haves.size(), torrentFile.numPieces);
                pieceManager.getHaves() = std::move(_haves);
            }
        }
    }

    void TorrentDownloader::download(int timeout) {
        // Get the peers from the tracker object
        std::vector<std::pair<std::string, std::uint16_t>> peerList {torrentTracker.getPeers(timeout)};
        if (peerList.empty()) { Logging::Dynamic::Error("No peers available"); return; }

        Logging::Dynamic::Info("Discovered {} peers.", peerList.size());

        net::PollManager pollManager;
        std::unordered_map<std::string, PeerContext> states;
        std::unordered_map<int, std::string> fd2PeerID;

        // Connect to all available peers and fill send buffer with handshakes
        std::string handshake {buildHandshake(torrentFile.infoHash, peerID)};
        auto lastTick {std::chrono::steady_clock::now()};
        for (const auto &[ip, port]: peerList) {
            std::optional<net::IP> ipType {net::utils::checkIPType(ip)};
            if (!ipType.has_value()) { Logging::Dynamic::Debug("Invalid IP: {}", ip); continue; }

            // Create socket connection and the Peer Context
            auto peer {net::Socket{net::SOCKTYPE::TCP, ipType.value()}};
            peer.setNonBlocking(); peer.connect(ip, port);
            PeerContext peerCtx {.ip=ip, .port=port, .ipV4=(ipType.value() == net::IP::V4), 
                .ID=(ip + ':' + std::to_string(port)), .fd=peer.fd(), .sendBuffer=handshake, 
                .lastReadTimeStamp=lastTick};

            states.emplace(peerCtx.ID, std::move(peerCtx));
            fd2PeerID.emplace(peer.fd(), peerCtx.ID);

            pollManager.track(std::move(peer), net::PollEventType::Writable);
            Logging::Dynamic::Debug("Intiating connection with {}:{}", ip, port);
        }

        std::size_t pendingPieceCount {torrentFile.numPieces - pieceManager.getHaves().size()};
        double pendingSize {pendingPieceCount * torrentFile.pieceSize / (1024. * 1024.)};
        Logging::Dynamic::Info("Established connection with {} peers, "
            "Pending download: {:.2f} MB", pollManager.size(), pendingSize);

        static bool interrupted {false};
        std::signal(SIGINT, [](int) { interrupted = true; });
        while (!interrupted && !pieceManager.finished() && !pollManager.empty()) {
            auto lastTick {std::chrono::steady_clock::now()};
            for (auto &[peer, event]: pollManager.poll(5)) {
                PeerContext &ctx {states.at(fd2PeerID.at(peer.fd()))};

                if (!ctx.closed && event & net::PollEventType::Readable) {
                    std::string recvBytes;
                    try { recvBytes = peer.recvAll(); } 
                    catch (net::SocketError &err) {
                        Logging::Dynamic::Debug("[{}] Recv from client failed: {}", ctx.ID, err.what());
                        ctx.closed = true;
                    }

                    if (!recvBytes.empty()) {
                        Logging::Dynamic::Debug("[{}] Recv {} bytes from client", ctx.ID, recvBytes.size());
                        ctx.lastReadTimeStamp = lastTick;

                        // Get existing buffer size to expand recvBuffer
                        std::size_t iSize {ctx.recvBuffer.size()};
                        ctx.recvBuffer.resize(iSize + recvBytes.size());
                        std::memmove(ctx.recvBuffer.data() + iSize, recvBytes.data(), recvBytes.size());

                        Logging::Dynamic::Trace("[{}] Current recv buffer size: {}", ctx.ID, ctx.recvBuffer.size());

                        if (!ctx.handshaked && ctx.recvBuffer.size() >= 68) {
                            ctx.handshaked = std::memcmp(handshake.data(), ctx.recvBuffer.data(), 20) == 0 
                                && std::memcmp(handshake.data() + 28, ctx.recvBuffer.data() + 28, 20) == 0;
                            if (!ctx.handshaked) {
                                Logging::Dynamic::Debug("[{}] Handshake failed, will be dropped", ctx.ID);
                                ctx.closed = true;
                            } else {
                                ctx.recvBuffer = ctx.recvBuffer.substr(68);
                                Logging::Dynamic::Debug("[{}] Handshake established", ctx.ID);
                                ctx.sendBuffer = buildInterested();
                            }
                        }

                        if (ctx.handshaked) {
                            std::uint32_t msgLen;
                            while ((msgLen = IsCompleteMessage(ctx.recvBuffer)) > 0 && !ctx.closed) {
                                auto [msgType, message] {parseMessage(std::string_view{ctx.recvBuffer.data(), msgLen + 4})};
                                Logging::Dynamic::Debug("[{}] Received {} from client", ctx.ID, str(msgType));

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
                                    if (ctx.unchokeAttempts > MAX_UNCHOKE_ATTEMPTS - 1) {
                                        Logging::Dynamic::Debug("[{}] Exceeded max unchoke attempts, disconnecting", ctx.ID);
                                        ctx.closed = true;
                                    }

                                    // If buffer already has unchoke, we haven't had
                                    // a chance to send to client yet, don't increase attempts
                                    else if (ctx.sendBuffer != unchokeMsg) {
                                        ++ctx.unchokeAttempts;
                                        Logging::Dynamic::Debug("[{}] Building unchoke for client, attempt: {}/{}", 
                                                ctx.ID, ctx.unchokeAttempts, MAX_UNCHOKE_ATTEMPTS);
                                        ctx.sendBuffer = unchokeMsg;
                                    }
                                }

                                // If unchoked & peer not throttled, try requesting for block(s) available from peer
                                else if (ctx.backlog < MAX_BACKLOG) {
                                    auto pendingBlocks {pieceManager.getPendingBlocks(ctx.haves, MAX_BACKLOG - ctx.backlog)};
                                    Logging::Dynamic::Debug("[{}] Building request for {} blocks", ctx.ID, pendingBlocks.size());
                                    for (auto [pieceIdx, blockOffset, blockSize]: pendingBlocks) {
                                        Logging::Dynamic::Trace("[{}] Building request for block (pIdx={}, bOffset={}, bSize={})", 
                                                ctx.ID, pieceIdx, blockOffset, blockSize);
                                        ctx.sendBuffer += buildRequest(pieceIdx, blockOffset, blockSize);
                                        ctx.pending.emplace(pieceIdx, blockOffset, blockSize);
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
                            Logging::Dynamic::Debug("[{}] Sent {} bytes to client", ctx.ID, sentBytes);
                            ctx.sendBuffer = ctx.sendBuffer.substr(static_cast<std::size_t>(sentBytes));
                        }
                    } catch (net::SocketError &err) { 
                        Logging::Dynamic::Debug("[{}] Send to client failed: {}", ctx.ID, err.what());
                        ctx.closed = true;
                    }
                }

                // Drop client on closed or err event
                if (event & net::PollEventType::Error || event & net::PollEventType::Closed || peer.fd() == -1) { 
                    Logging::Dynamic::Debug("[{}] Got closed/err event", ctx.ID);
                    ctx.closed = true;
                }

                // Only track for events we are interested in
                if (!ctx.closed) {
                    if (ctx.sendBuffer.empty()) {
                        pollManager.updateTracking(peer.fd(), net::PollEventType::Readable);
                        Logging::Dynamic::Trace("[{}] Send buffer empty, listening only for READABLE events", ctx.ID);
                    } else if (ctx.handshaked) {
                        pollManager.updateTracking(peer.fd(), net::PollEventType::Readable | net::PollEventType::Writable);
                        Logging::Dynamic::Trace("[{}] Send buffer not empty, listening for READABLE & WRITABLE events", ctx.ID);
                    }
                }
            } // for poll loop

            // Process the clients that didn't send us anything or were closed inside our poll loop
            for (auto &[fd, ctx]: states) {
                auto timeDiff {lastTick - ctx.lastReadTimeStamp};
                auto diffInSec {std::chrono::duration_cast<std::chrono::seconds>(timeDiff).count()};
                // Good peers
                if (!ctx.closed && diffInSec < MAX_REQ_WAIT_TIME) continue;

                // Timed out peers (handshaked or not handshaked)
                else if (!ctx.closed) {
                    Logging::Dynamic::Trace("[{}] Client idled out, no message received for {}s", ctx.ID, diffInSec);
                    if (ctx.choked) {
                        Logging::Dynamic::Debug("[{}] Not handshaked or choked for too long, dropping", ctx.ID, diffInSec);
                        ctx.closed = true;
                    } else if (ctx.backlog) {
                        Logging::Dynamic::Debug("[{}] Building cancel request for {} blocks", ctx.ID, ctx.pending.size());
                        ctx.sendBuffer = "";
                        for (auto [pieceIdx, blockOffset, blockSize]: ctx.pending) {
                            Logging::Dynamic::Trace("[{}] Building cancel request for block (pIdx={}, bOffset={}, bSize={})", 
                                ctx.ID, pieceIdx, blockOffset, blockSize);
                            ctx.sendBuffer += buildRequest(pieceIdx, blockOffset, blockSize, false);
                        }
                        clearPendingFromPeer(ctx);
                    }
                }

                if (ctx.closed) {
                    // Dropped from the poll loop or from timing out from above block
                    if (fd2PeerID.contains(ctx.fd)) {
                        clearPendingFromPeer(ctx);
                        pollManager.untrack(ctx.fd);
                        fd2PeerID.erase(ctx.fd);
                        ctx.lastReadTimeStamp = lastTick;
                        Logging::Dynamic::Debug("[{}] Dropped client (Pending reconnects: {}/{}), still connected to {} clients", 
                            ctx.ID, ctx.reconnectAttempts, MAX_RECONNECT_ATTEMPTS, pollManager.size());
                    }

                    // Already been dropped and set threshold time has passed, try to reconnect
                    else if (diffInSec >= MIN_RECON_WAIT_TIME && ctx.reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
                        auto peer {net::Socket{net::SOCKTYPE::TCP, ctx.ipV4? net::IP::V4: net::IP::V6}};
                        peer.setNonBlocking(); peer.connect(ctx.ip, ctx.port);
                        ctx.onReconnect(peer.fd(), lastTick); fd2PeerID.emplace(peer.fd(), ctx.ID); 
                        pollManager.track(std::move(peer), net::PollEventType::Writable);
                        ctx.sendBuffer = handshake;
                        Logging::Dynamic::Debug("Re-Initiating connection with {}, attempt: {}/{}", 
                            ctx.ID, ctx.reconnectAttempts, 3);
                    }
                } 

            }

        } // while not interrupted && pieceManager not empty && poll manager has tracking sockets

        // Display status to user
        if (interrupted) Logging::Dynamic::Warn("Interupt received, states will be saved before exit");
        bool status {diskWriter.finish(torrentFile.files, pieceManager.finished())};
        Logging::Dynamic::Info("Download status: {}", (status? "DONE": "PENDING"));
    }
};
