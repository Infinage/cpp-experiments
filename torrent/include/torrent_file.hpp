#pragma once

#include <cstdint>
#include <string>

// Forward declare to speed up compilation
namespace JSON { struct JSONHandle; }

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

        public:
            TorrentFile(const std::string_view torrentFP);
    };
}
