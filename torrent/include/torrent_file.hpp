#pragma once

#include "../../json-parser/json.hpp"

namespace Torrent {
    class TorrentFile {
        private:
            // Helper to extract the total length from json field 'length'
            static std::uint64_t calculateTotalLength(JSON::JSONHandle info);

        public:
            std::string announceURL;
            std::uint32_t pieceSize;
            std::uint64_t length;
            std::string name, pieceBlob, infoHash;
            std::size_t numPieces;
            std::optional<JSON::JSONHandle> root;

        public:
            TorrentFile(const std::string_view torrentFP);

            inline std::string_view getPieceHash(std::size_t idx) const {
                if (idx >= numPieces) throw std::runtime_error("Piece Hash idx requested out of range");
                return std::string_view{pieceBlob}.substr(idx * 20, 20);
            }

    };
}
