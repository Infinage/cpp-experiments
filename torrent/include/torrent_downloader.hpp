#pragma once

#include "torrent_file.hpp"
#include "peer_context.hpp"
#include "piece_manager.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace Torrent {
    class TorrentDownloader {
        public:
            ~TorrentDownloader();

            TorrentDownloader(
                const TorrentFile &torrentFile, const std::string_view downloadDir, 
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

            // Piece Manager to determine which piece to download next
            PieceManager pieceManager;

            // Torrent download directory path
            const std::filesystem::path DownloadDir;
            std::ofstream DownloadTempFile;

            // Save torrent state
            const std::filesystem::path StateSavePath;

        private:
            void handleHave(const std::string &payload, PeerContext &ctx);
            void handleBitfield(const std::string &payload, PeerContext &ctx);
            void handlePiece(const std::string &payload, PeerContext &ctx);
            void handleChoke(const std::string&, PeerContext &ctx);
            void handleUnchoke(const std::string&, PeerContext &ctx);
            void clearPendingFromPeer(PeerContext &ctx);
    };
};
