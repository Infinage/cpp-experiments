#include "../include/bencode.hpp"
#include "../include/torrent_file.hpp"

#include "../../cryptography/hashlib.hpp"
#include "../../json-parser/json.hpp"
#include "../../misc/logger.hpp"

#include <fstream>

namespace Torrent {
    std::uint64_t TorrentFile::calculateTotalLength(JSON::JSONHandle info) {
        auto files {info["files"]};
        if (!files.ptr) return static_cast<std::uint64_t>(info.at("length").to<std::int64_t>());
        else {
            auto filesObj {files.cast<JSON::JSONArrayNode>()};
            return std::accumulate(filesObj.begin(), filesObj.end(), std::uint64_t {}, 
                [] (std::uint64_t acc, JSON::JSONHandle file) {
                    return acc + static_cast<std::uint64_t>(file.at("length").to<std::int64_t>());
                }
            );
        }
    }

    std::vector<FileStruct> TorrentFile::parseFileStructure(JSON::JSONHandle info) {
        auto files {info["files"]};
        if (!files.ptr) return {{
            info.at("name").to<std::string>(), 
            static_cast<std::uint64_t>(info.at("length").to<std::int64_t>())
        }};

        else {
            std::vector<FileStruct> result;
            for (JSON::JSONHandle file: files) {
                auto filePath = std::ranges::fold_left(file.at("path"), std::filesystem::path {}, 
                [] (auto acc, JSON::JSONHandle curr) { return acc / curr.to<std::string>(); });
                auto fileSize {static_cast<std::uint64_t>(file.at("length").to<std::int64_t>())};
                result.emplace_back(filePath, fileSize);
            }
            return result;
        }
    }
    
    TorrentFile::TorrentFile(const std::string_view torrentFP) {
        std::ifstream ifs {torrentFP.data(), std::ios::binary | std::ios::ate};
        auto size {ifs.tellg()};
        std::string buffer(static_cast<std::size_t>(size), 0);
        ifs.seekg(0, std::ios::beg);
        ifs.read(buffer.data(), size);

        // Read the bencoded torrent file
        JSON::JSONHandle root = Bencode::decode(buffer);

        // Extract the announce URL from torrent file
        announceURL = root.at("announce").to<std::string>();

        // Extract other required fields
        JSON::JSONHandle info {root.at("info")};
        name = info.at("name").to<std::string>();
        length = calculateTotalLength(info);
        pieceSize = static_cast<std::uint32_t>(info["piece length"].to<std::int64_t>()); 
        pieceBlob = info["pieces"].to<std::string>();
        numPieces = (length + pieceSize - 1) / pieceSize;

        // Parse the 'files' meta to replicate it post download if applicable
        files = parseFileStructure(info);

        // Sanity check on blob validity - can be equally split
        if (pieceBlob.size() % 20) throw std::runtime_error("Piece blob is corrupted");

        // Reencode just the info dict to compute its sha1 hash (raw hash)
        infoHash = hashutil::sha1(Bencode::encode(info.ptr, true), true);

        // Print out the meta
        Logging::Dynamic::Debug(
            "Loaded torrent metadata\n"
            "  {:<12} {}\n"
            "  {:<12} {}\n"
            "  {:<12} {}\n"
            "  {:<12} {}\n"
            "  {:<12} {}\n"
            "  {:<12} {}",
            "Name:",       name,
            "Length:",     length,
            "Piece Size:", pieceSize,
            "Num Pieces:", numPieces,
            "Announce:",   announceURL,
            "File Count:", files.size()
        );
    }
}
