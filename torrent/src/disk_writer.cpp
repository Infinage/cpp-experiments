#include "../include/disk_writer.hpp"

#include <print>

namespace Torrent {
    DiskWriter::DiskWriter(
        const std::string_view name, const std::uint64_t totalSize, 
        const std::uint32_t pieceSize, const std::filesystem::path downloadDir
    ): 
        name {name}, totalSize {totalSize},
        pieceSize {pieceSize}, DownloadDir {downloadDir}
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
            DownloadTempFile = std::ofstream {DownloadTempFilePath, std::ios::binary};
            DownloadTempFile.seekp(static_cast<std::int64_t>(totalSize - 1)); 
            DownloadTempFile.put('\0');
        }
    }

    void DiskWriter::schedule(std::uint64_t offset, std::string piece) {
        DownloadTempFile.seekp(static_cast<std::int64_t>(offset), std::ios::beg);
        DownloadTempFile.write(piece.c_str(), pieceSize);
        std::println("Piece# {} has completed downloading", offset / pieceSize);
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

    bool DiskWriter::finish(const std::vector<FileStruct> &files, bool status) {
        if (!status) return false;

        DownloadTempFile.close();

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
