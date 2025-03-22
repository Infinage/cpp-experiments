#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>

namespace zhelper {
    [[nodiscard]] inline std::vector<std::uint8_t> zcompress(const std::string &input) {
        std::vector<std::uint8_t> data(input.begin(), input.end());

        std::size_t srcLen = data.size();
        std::size_t destLen = compressBound(srcLen);
        std::vector<std::uint8_t> compressed(destLen);

        if (compress(compressed.data(), &destLen, data.data(), srcLen) != Z_OK)
            throw std::runtime_error("ZError: Compression failed");
        
        compressed.resize(destLen);
        return compressed;
    }

    [[nodiscard]] inline std::string zdecompress(const std::vector<std::uint8_t> &compressed) {
        std::size_t destLen {compressed.size() * 4};
        std::vector<std::uint8_t> decompressed(destLen);

        int status;
        while((status = uncompress(decompressed.data(), &destLen, compressed.data(), compressed.size())) == Z_BUF_ERROR) {
            destLen *= 2; decompressed.resize(destLen);
        }

        if (status != Z_OK)
            throw std::runtime_error("ZError: Decompression failed");

        decompressed.resize(destLen);
        return std::string{decompressed.begin(), decompressed.end()};
    }

    [[nodiscard]] inline std::string zread(const std::string &ifile) {
        // Read the input as binary
        std::ifstream ifs {ifile, std::ios::binary | std::ios::ate};
        if (!ifs) throw std::runtime_error("ZError: Cannot open file for reading: " + ifile);
        long fileSize {ifs.tellg()}; ifs.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> compressed(static_cast<std::size_t>(fileSize));
        if (!ifs.read(reinterpret_cast<char*>(compressed.data()), fileSize))
            throw std::runtime_error("ZError: Failed to read from file: " + ifile);

        // Uncompress and return result
        return zhelper::zdecompress(compressed);
    }

    inline void zwrite(const std::string &uncompressed, const std::string &ofile) {
        // Compress and write to ofile
        std::vector<std::uint8_t> compressed {zhelper::zcompress(uncompressed)};
        std::ofstream ofs {ofile, std::ios::binary | std::ios::out};
        ofs.write(reinterpret_cast<const char*>(compressed.data()), static_cast<long>(compressed.size()));
    }
}
