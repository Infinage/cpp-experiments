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

    [[nodiscard]] inline std::string zdecompress(std::ifstream &ifs) {
        constexpr std::size_t CHUNK_SIZE = 4096;
        std::vector<char> inBuffer(CHUNK_SIZE);
        std::vector<char> outBuffer(CHUNK_SIZE);
        std::string decompressed;

        z_stream strm{}; int ret;
        strm.zalloc = Z_NULL; strm.zfree = Z_NULL; strm.opaque = Z_NULL;
        if ((ret = inflateInit(&strm)) != Z_OK) {
            throw std::runtime_error("ZError: Failed to initialize zlib inflater");
        }

        do {
            ifs.read(inBuffer.data(), CHUNK_SIZE);
            std::streampos bytesRead = ifs.gcount();
            if (bytesRead == 0) break;

            strm.avail_in = static_cast<uInt>(bytesRead);
            strm.next_in = reinterpret_cast<Bytef *>(inBuffer.data());

            do {
                strm.avail_out = CHUNK_SIZE;
                strm.next_out = reinterpret_cast<Bytef *>(outBuffer.data());

                ret = inflate(&strm, Z_NO_FLUSH);
                if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
                    inflateEnd(&strm);
                    throw std::runtime_error("ZError: Decompress failed: " + std::to_string(ret));
                }

                std::size_t have = CHUNK_SIZE - strm.avail_out;
                decompressed.append(outBuffer.data(), have);

            } while (strm.avail_out == 0);
            
        } while (ret != Z_STREAM_END);

        inflateEnd(&strm);
        return decompressed;
    }

    [[nodiscard]] inline std::string zread(const std::string &ifile) {
        std::ifstream ifs {ifile, std::ios::binary | std::ios::binary};
        if (!ifs) throw std::runtime_error("ZError: Cannot open file for reading: " + ifile);
        return zdecompress(ifs);
    }

    inline void zwrite(const std::string &uncompressed, const std::string &ofile) {
        // Compress and write to ofile
        std::vector<std::uint8_t> compressed {zhelper::zcompress(uncompressed)};
        std::ofstream ofs {ofile, std::ios::binary | std::ios::out};
        ofs.write(reinterpret_cast<const char*>(compressed.data()), static_cast<long>(compressed.size()));
    }
}
