#include "../include/GitPack.hpp"
#include "../include/utils.hpp"
#include "../../misc/zhelper.hpp"

#include <cstring>
#include <fstream>

bool GitPack::verifyHeader(std::ifstream &ifs, const std::string &expectedHeader, unsigned int expectedVersion) {
    // Verify magic byte header
    char header[5] {}; 
    ifs.read(header, 4); 
    if (std::strcmp(header, expectedHeader.c_str()) != 0) 
        return false;

    // Verify version number
    unsigned int version;
    readBigEndian(ifs, version);
    return version == expectedVersion;
}

unsigned int GitPack::getPackIdxOffsetStart(unsigned int start, unsigned int end, 
        const std::string &part, std::ifstream &ifs) const 
{
    constexpr unsigned int skip {8 + 256 * 4}; 
    while (start <= end) {
        unsigned int mid = start + (end - start) / 2;
        ifs.seekg(skip + (mid * 20), std::ios::beg);

        char shaBin[20]; ifs.read(shaBin, 20);
        std::string sha {binary2Sha(std::string_view{shaBin, 20})};

        // We check start == mid to prevent underflow (uint)
        if (sha.compare(0, part.size(), part) >= 0) {
            if (start == mid) break;
            end = mid - 1;
        } else {
            start = mid + 1;
        }
    }

    return start;
}

std::vector<std::pair<std::string, unsigned int>> GitPack::getHashMatchFromIndex(const std::string &part, const fs::path &path) const {
    if (part.size() < 2)
        throw std::runtime_error("PackIndex: Hex must be atleast 2 chars long, got: " + part);

    std::ifstream ifs {path, std::ios::binary};
    if (!verifyHeader(ifs, "\xfftOc", 2))
        throw std::runtime_error("Not a valid pack idx file: " + path.string());

    // Check fanout table layer 1 - 256 entries, 4 bytes each
    int hexInt {std::stoi(part.substr(0, 2), nullptr, 16)};
    unsigned int curr, prev{0};
    ifs.seekg(8 + (hexInt * 4), std::ios::beg);
    readBigEndian(ifs, curr);
    if (hexInt > 0) {
        ifs.seekg(8 + ((hexInt - 1) * 4), std::ios::beg);
        readBigEndian(ifs, prev);
    }
    
    // If hashes exist in the pack, continue linearly traversing until sha
    // continues to match with part. Break immediately if it doesn't
    std::vector<std::pair<std::string, unsigned int>> matches;
    if (curr - prev > 0) {
        unsigned int skip {8 + 256 * 4}, startOffset {getPackIdxOffsetStart(prev, curr, part, ifs)};
        ifs.seekg(skip + (startOffset * 20), std::ios::beg);
        while (startOffset <= curr) {
            char shaBin[20];  ifs.read(shaBin, 20);
            std::string sha {binary2Sha(std::string_view{shaBin, 20})};
            if (!sha.starts_with(part)) break;
            matches.emplace_back(sha, startOffset);
            startOffset++;
        }
    }

    return matches;
}

std::pair<fs::path, unsigned long> GitPack::getPackFileOffset(const std::string &objectHash) const {
    std::vector<std::pair<fs::path, unsigned int>> matches;
    for (const fs::path &path: indexPaths) {
        for (const std::pair<std::string, unsigned int> &match: getHashMatchFromIndex(objectHash, path)) {
            matches.emplace_back(path, match.second);
        }
    }

    // 40 char sha will only return 1 match, since 40 char limit is not enforced
    // this function can theoretically be used on partial hashes as long
    // as they return a unique match
    if (matches.size() != 1)
        throw std::runtime_error(objectHash + ": Expected candidates to be 1, got: " 
                + std::to_string(matches.size()));

    // Get the Pack offset along with count of records
    std::ifstream ifs {matches[0].first, std::ios::binary};
    unsigned int offset {matches[0].second}, total;
    ifs.seekg(8 + (255 * 4), std::ios::beg);
    readBigEndian(ifs, total);

    // Read from first offset layer
    ifs.seekg(1032 + (total * 24) + (offset * 4), std::ios::beg);
    unsigned long result;
    unsigned int r1, mask {1u << 31}; 
    readBigEndian(ifs, r1);

    // First layer contains direct entries
    if (!(r1 & mask))
        result = static_cast<unsigned long>(r1);

    // First layer points to second layer
    else {
        r1 &= ~mask; ifs.seekg(1032 + (total * 28) + (r1 * 8), std::ios::beg);
        readBigEndian(ifs, result);
    } 

    return {matches[0].first.replace_extension(".pack"), result};
}

std::size_t GitPack::readVarLenInt(std::basic_string_view<unsigned char> &sv) {
    std::size_t pos{0}, result {0}; 
    int shift {0};
    while (true) {
        unsigned char byte {sv[pos]};
        result |= static_cast<std::size_t>((byte & 127) << shift);
        shift += 7; pos++;
        if (!(byte & 128)) break;
    }

    sv = sv.substr(pos);
    return result;
}

GitPack::GitPack (const fs::path &path) {
    if (fs::exists(path)) {
        for (const fs::directory_entry &entry: fs::directory_iterator(path)) {
            if (entry.path().extension() == ".idx")
                indexPaths.emplace_back(entry);
            else if (entry.path().extension() == ".pack")
                packPaths.emplace_back(entry);
        }
    }
}

[[nodiscard]] std::vector<std::string> GitPack::refResolve(const std::string &part) const {
    std::vector<std::string> matches;
    for (const fs::path &path: indexPaths) {
        for (const std::pair<std::string, unsigned int> &match: getHashMatchFromIndex(part, path)) {
            matches.emplace_back(match.first);
        }
    }

    return matches;
}

[[nodiscard]] std::string GitPack::extract(const std::string &objectHash) const {
    fs::path packFile; unsigned long offset;
    std::tie(packFile, offset) = getPackFileOffset(objectHash);

    // Thin packs must be resolved already. We assume that a pack
    // is self contained. All references point to same pack file
    std::ifstream ifs {packFile, std::ios::binary};
    if (!verifyHeader(ifs, "PACK", 2))
        throw std::runtime_error("Not a valid pack file: " + packFile.string());

    // Do we have additional objects left in the chain?
    // Type, offset, size (size relevant only for base obj)
    std::vector<std::tuple<short, unsigned int, unsigned int>> deltaChain;
    while (true) {
        // type and length
        short type{0}; std::size_t length; int shift;
        ifs.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        bool msb {true};
        while (msb) {
            int byte = ifs.get();
            msb = byte & 128;
            if (!type) {
                type = (byte >> 4) & 7;
                length = byte & 15;
                shift = 4;
            } else {
                length |= static_cast<std::size_t>((byte & 127) << shift);
                shift += 7;
            }
        }

        // Non-deltified objects
        if (type >= 1 && type <= 4) {
            deltaChain.emplace_back(type, ifs.tellg(), length);
            break;
        }

        // OBJ_OFS_DELTA
        else if (type == 6) {
            // Big endian encoding
            std::size_t relOffset{0};
            while (true) {
                int byte = ifs.get();
                relOffset = (relOffset << 7) | static_cast<std::size_t>(byte & 127);
                if (!(byte & 128)) break;

                // Git specific
                relOffset += 1;
            }

            deltaChain.emplace_back(0, ifs.tellg(), length);
            offset -= relOffset;
        } 

        // OBJ_REF_DELTA
        else if (type == 7) {
            char shaBin[20];  ifs.read(shaBin, 20);
            std::string sha {binary2Sha(std::string_view{shaBin, 20})};
            deltaChain.emplace_back(0, ifs.tellg(), length);
            offset = getPackFileOffset(sha).second;
        }

        else {
            throw std::runtime_error(objectHash + 
                    " has unexpected format: " + std::to_string(type));
        }
    }

    // Resolve the delta chain
    short baseType; std::string result;
    while (!deltaChain.empty()) {
        short type; unsigned int offset, size;
        std::tie(type, offset, size) = std::move(deltaChain.back());
        deltaChain.pop_back();
        ifs.seekg(offset, std::ios::beg);
        std::string decompressed {zhelper::zdecompress(ifs)};
        if (size != decompressed.size())
            throw std::runtime_error("Resolving delta chain failed for " 
                    + objectHash + ".\nIncorrect decompressed object size.");

        if (type == 0) {
            // Store an unmodified copy - we might end up 
            // copying multiple pieces from the base obj
            std::string baseObj {std::move(result)};

            std::basic_string_view<unsigned char> dsv {
                reinterpret_cast<const unsigned char*>(decompressed.data()), 
                decompressed.size()};

            std::size_t sourceLen {readVarLenInt(dsv)}; 
            size = static_cast<unsigned int>(readVarLenInt(dsv));

            // Assert source input size is correct
            if (baseObj.size() != sourceLen)
                throw std::runtime_error("Resolving delta chain failed for " 
                        + objectHash + ".\nIncorrect source object size.");

            // Quick helper for popping from front of the view
            auto pop_front{[](std::basic_string_view<unsigned char> &dsv){
                unsigned char front {dsv.front()};
                dsv = dsv.substr(1);
                return front;
            }};

            // Continue reading until end of the instruction set 
            // ** We can have multiple instructions **
            while (!dsv.empty()) {
                unsigned char op {pop_front(dsv)};

                // Copy from prev
                if (op & 128) {
                    std::size_t copyOffset {0}, copySize {0};
                    if (op &  1) copyOffset |= static_cast<std::size_t>(pop_front(dsv) <<  0);
                    if (op &  2) copyOffset |= static_cast<std::size_t>(pop_front(dsv) <<  8);
                    if (op &  4) copyOffset |= static_cast<std::size_t>(pop_front(dsv) << 16);
                    if (op &  8) copyOffset |= static_cast<std::size_t>(pop_front(dsv) << 24);
                    if (op & 16)   copySize |= static_cast<std::size_t>(pop_front(dsv) <<  0);
                    if (op & 32)   copySize |= static_cast<std::size_t>(pop_front(dsv) <<  8);
                    if (op & 64)   copySize |= static_cast<std::size_t>(pop_front(dsv) << 16);

                    // If copy size is not set, set default size
                    if (!copySize) copySize = 0x10000;
                    result.append(baseObj.substr(copyOffset, copySize));
                }

                // Insert / add
                else {
                    result.append(dsv.begin(), dsv.begin() + op);
                    dsv = dsv.substr(op);
                }
            }
        }

        // Base object type, simply read it as it is
        else {
            baseType = type;
            result = std::move(decompressed);
        }

        // Assert final output size is correct
        if (result.size() != size)
            throw std::runtime_error("Resolving delta chain failed for " 
                    + objectHash + ".\nIncorrect dest object size.");
    }

    // Reformat to the way the other functions expect
    std::string fmt;
    switch (baseType) {
        case 1: fmt = "commit"; break;
        case 2: fmt = "tree"; break;
        case 3: fmt = "blob"; break;
        case 4: fmt = "tag"; break;
    }

    // "<FMT> <SIZE>\x00<DATA...>
    return fmt + ' ' + std::to_string(result.size()) + '\x00' + result;
}
