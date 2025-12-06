#pragma once

#include "common.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <openssl/ssl.h>
#include <queue>

namespace Torrent {
    class DiskWriter {
        private:
            const std::string name;
            const std::uint64_t totalSize; 
            const std::uint32_t pieceSize;
            const std::filesystem::path DownloadDir;
            const std::size_t MAX_QUEUE;

            std::filesystem::path DownloadTempFilePath;
            std::fstream DownloadTempFile;

            // Threading related stuff
            std::queue<std::pair<std::uint64_t, std::string>> tasks;
            std::atomic<bool> exitCondition {false};
            std::condition_variable tasksCV;
            std::mutex taskMutex;
            std::thread writer;

        private:
            bool chunkCopy(std::ifstream &source, std::ofstream &destination, 
                std::uint64_t size, std::uint64_t chunkSize = 5 * 1024 * 1024);

        public:
            ~DiskWriter();
            DiskWriter(const std::string name, const std::uint64_t totalSize, const std::uint32_t pieceSize, 
                    const std::filesystem::path downloadDir, bool coldStart, const std::size_t maxQueueSize);
            void schedule(std::uint64_t offset, std::string &&piece);
            [[nodiscard]] bool finish(const std::vector<FileStruct> &files, bool status);

            [[deprecated("Do not mix sync and async writes, this writes without locking")]] 
            void scheduleSync(std::uint64_t offset, std::string &&piece);
    };
}
