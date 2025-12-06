#pragma once

#include "disk_writer.hpp"
#include "torrent_tracker.hpp"
#include "peer_context.hpp"
#include "piece_manager.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace Torrent {
    class TorrentDownloader {
        public:
            ~TorrentDownloader();

            TorrentDownloader(
                TorrentTracker &tTracker, const std::filesystem::path downloadDir, 
                const std::uint16_t bSize = 1 << 14, const std::uint8_t backlog = 8,
                const std::uint8_t unchokeAttempts = 3, const std::uint16_t maxWaitTime = 5,
                const std::size_t maxDiskQueue = 5000
            );

            void download(int timeout = 10);

        private:
            // Const reference to torrent file meta data
            const TorrentFile &torrentFile;

            // Non const reference to torrent tracker
            TorrentTracker &torrentTracker;

            // Random peer ID to identify ctorrent client
            const std::string peerID;

            // User defined constants
            const std::uint16_t blockSize, MAX_WAIT_TIME;
            const std::uint8_t MAX_BACKLOG, MAX_UNCHOKE_ATTEMPTS;

            // Save torrent state
            const std::filesystem::path StateSavePath;

            // Whether to download from scratch
            // Decided based on whether we have the StateSavePath on disk
            const bool coldStart {true};

            // Piece Manager to determine which piece to download next
            PieceManager pieceManager;

            // Disk writer manages to actual file writes
            DiskWriter diskWriter;

        private:
            void handleHave(const std::string &payload, PeerContext &ctx);
            void handleBitfield(const std::string &payload, PeerContext &ctx);
            void handlePiece(const std::string &payload, PeerContext &ctx);
            void handleChoke(const std::string&, PeerContext &ctx);
            void handleUnchoke(const std::string&, PeerContext &ctx);
            void clearPendingFromPeer(PeerContext &ctx);
    };
};
