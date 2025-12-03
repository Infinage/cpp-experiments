#pragma once

#include "common.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>

namespace Torrent {
    class DiskWriter {
        private:
            const std::string name;
            const std::uint64_t totalSize; 
            const std::uint32_t pieceSize;
            const std::filesystem::path DownloadDir;
            std::filesystem::path DownloadTempFilePath;
            std::ofstream DownloadTempFile;

        private:
            bool chunkCopy(std::ifstream &source, std::ofstream &destination, std::uint64_t size, std::uint64_t chunkSize = 5 * 1024 * 1024);

        public:
            DiskWriter(const std::string_view name, const std::uint64_t totalSize, 
                const std::uint32_t pieceSize, const std::filesystem::path downloadDir);
            void schedule(std::uint64_t offset, std::string piece);
            [[nodiscard]] bool finish(const std::vector<FileStruct> &files, bool status);
    };
}
