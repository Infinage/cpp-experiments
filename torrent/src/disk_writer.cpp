#include "../include/disk_writer.hpp"

#include <print>

namespace Torrent {
    DiskWriter::DiskWriter(
        const std::string name, const std::uint64_t totalSize, const std::uint32_t pieceSize, 
        const std::filesystem::path downloadDir, bool coldStart, const std::size_t maxQueueSize
    ): 
        name {name}, totalSize {totalSize}, pieceSize {pieceSize}, 
        DownloadDir {downloadDir}, MAX_QUEUE {maxQueueSize}
    {
        // Create download directory if it doesn't already exist
        if (!std::filesystem::exists(DownloadDir))
            std::filesystem::create_directory(DownloadDir);
        if (!std::filesystem::is_directory(DownloadDir))
            throw std::runtime_error("Download directory provided is not a valid folder path");

        // Check if already split
        if (std::filesystem::exists(DownloadDir / name))
            throw std::runtime_error("Torrent already downloaded?");

        // Create a temp sparse file for saving the pieces
        DownloadTempFilePath = DownloadDir / ("." + std::string{name});
        if (!std::filesystem::exists(DownloadTempFilePath)) {
            if (!coldStart) throw std::runtime_error{"Temp download file is missing, delete "
                "the `.ctorrent` file and restart"};
            std::ofstream tempFile {DownloadTempFilePath, std::ios::binary};
            tempFile.seekp(static_cast<std::int64_t>(totalSize - 1)); 
            tempFile.put('\0');
        }

        // Open the file handle
        DownloadTempFile = std::fstream {DownloadTempFilePath, std::ios::binary | std::ios::in | std::ios::out};
        if (!DownloadTempFile) throw std::runtime_error("Failed to open temp file for writing");

        // Spawn the worker thread and keep it running
        writer = std::thread {[this] {
            try {
                std::size_t piecesWritten {}; // Flush after every 1000 pieces written
                for (;;) {
                    std::unique_lock lock {taskMutex}; 
                    tasksCV.wait(lock, [this]{ return exitCondition || !tasks.empty(); });
                    if (exitCondition && tasks.empty()) { DownloadTempFile.close(); return; } 
                    else {
                        auto [offset, piece] {std::move(tasks.front())};
                        tasks.pop(); lock.unlock(); tasksCV.notify_one();
                        DownloadTempFile.seekp(static_cast<std::int64_t>(offset), std::ios::beg);
                        DownloadTempFile.write(piece.c_str(), static_cast<std::streamsize>(piece.size()));
                        if (!DownloadTempFile.good()) throw std::runtime_error("Write to temp file failed");
                        else if (++piecesWritten % MAX_QUEUE == 0) DownloadTempFile.flush();
                    }
                }
            } catch (std::exception &ex) { 
                std::println("An exception occured inside writer thread: {}", ex.what());
                exitCondition = true; tasksCV.notify_all();
            }
        }};
    }

    void DiskWriter::scheduleSync(std::uint64_t offset, std::string &&piece) {
        DownloadTempFile.seekp(static_cast<std::int64_t>(offset), std::ios::beg);
        DownloadTempFile.write(piece.c_str(), static_cast<std::streamsize>(piece.size()));
        if (!DownloadTempFile.good()) throw std::runtime_error("Write to temp file failed");
    }

    void DiskWriter::schedule(std::uint64_t offset, std::string &&piece) {
        std::unique_lock lock{taskMutex};
        tasksCV.wait(lock, [this]{ return exitCondition || tasks.size() < MAX_QUEUE; });
        if (exitCondition) throw std::runtime_error("Scheduled after exit called, state may be corrupt");
        tasks.emplace(offset, std::move(piece));
        tasksCV.notify_one();
    }

    bool DiskWriter::chunkCopy(std::ifstream &source, std::ofstream &destination, std::uint64_t size, std::uint64_t chunkSize) {
        std::vector<char> buffer(chunkSize);
        while (size > 0) {
            std::size_t toRead {size > chunkSize? chunkSize: size};
            source.read(buffer.data(), static_cast<std::streamsize>(toRead));
            std::streamsize bytesRead {source.gcount()};
            if (bytesRead <= 0) return false;
            destination.write(buffer.data(), bytesRead);
            if (!destination.good()) return false;
            size -= static_cast<std::uint64_t>(bytesRead);
        }
        return !size;
    }

    DiskWriter::~DiskWriter() {
        if (!exitCondition) {
            exitCondition = true;
            tasksCV.notify_all();
            writer.join();
        }
    }

    bool DiskWriter::finish(const std::vector<FileStruct> &files, bool status) {
        // Stop the running thread
        exitCondition = true;
        tasksCV.notify_all();
        if (writer.joinable()) writer.join();

        if (!status) return false;

        std::ifstream ifs {DownloadTempFilePath, std::ios::binary};
        for (auto [filePath, fileSize]: files) {
            filePath = DownloadDir / name / filePath;
            const auto parentPath {filePath.parent_path()};
            if (!parentPath.empty() && !std::filesystem::exists(parentPath))
                std::filesystem::create_directories(parentPath);
            std::ofstream ofs {filePath, std::ios::binary};
            if (!ofs || !chunkCopy(ifs, ofs, fileSize)) 
                return false;
        }

        std::filesystem::remove(DownloadTempFilePath);
        return true;
    }
}
