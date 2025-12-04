#pragma once

#include "disk_writer.hpp"
#include "torrent_file.hpp"
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
                const TorrentFile &torrentFile, const std::filesystem::path downloadDir, 
                const std::uint16_t bSize = 1 << 14, const std::uint8_t backlog = 8,
                const std::uint8_t unchokeAttempts = 3
            );

            void download(std::vector<std::pair<std::string, std::uint16_t>> &peers);

        private:
            // Const reference to torrent file meta data
            const TorrentFile &torrentFile;

            // Random peer ID to identify ctorrent client
            const std::string peerID;

            // User defined constants
            const std::uint16_t blockSize;
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
