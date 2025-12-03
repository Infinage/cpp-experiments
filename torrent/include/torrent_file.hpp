#pragma once

#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>

// Forward declare to speed up compilation
namespace JSON { struct JSONHandle; }

namespace Torrent {
    class TorrentFile {
        private:
            // Helper to extract the total length from json field 'length'
            static std::uint64_t calculateTotalLength(JSON::JSONHandle info);

            // Helper to parse the file structure from json field 'files'
            static std::vector<FileStruct> parseFileStructure(JSON::JSONHandle info);

        public:
            std::string announceURL;
            std::uint32_t pieceSize;
            std::uint64_t length;
            std::string name, pieceBlob, infoHash;
            std::size_t numPieces;
            std::vector<FileStruct> files;

        public:
            TorrentFile(const std::string_view torrentFP);
    };
}
