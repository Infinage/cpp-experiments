#pragma once

#include <array>
#include <string>
#include <stdexcept>
#include <unordered_map>

namespace Base64 {
    inline std::string base64Encode(const std::string &raw) {
        constexpr const std::array<char, 64> encodeMap {
            'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
            'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
            'g','h', 'i','j','k','l','m','n','o','p','q','r','s','t','u','v',
            'w','x','y', 'z', '0','1','2','3','4','5','6','7','8','9','+','/'
        };

        std::string encoded;
        encoded.reserve((raw.size() + 2) / 3 * 4);
        std::size_t acc{0}, bitCount{0};
        for (const char &ch: raw) {
            for (int i {7}; i >= 0; i--) {
                acc = (acc << 1) | ((ch >> i) & 1);
                bitCount++;
                if (bitCount == 6) {
                    encoded += encodeMap[acc];
                    acc = 0, bitCount = 0;
                }
            }
        }

        // Pad leftovers with 0
        if (bitCount) {
            std::size_t pendingBits {6 - bitCount};
            encoded += encodeMap[acc << pendingBits] 
                + std::string(pendingBits / 2, '=');
        }

        return encoded;
    }

    inline std::unordered_map<char, std::size_t> getDecodeMap() {
        std::size_t i {0};
        std::unordered_map<char, std::size_t> decodeMap{{'+', 62}, {'/', 63}};
        for (char ch {'A'}; ch <= 'Z'; ch++) decodeMap[ch] = i++;
        for (char ch {'a'}; ch <= 'z'; ch++) decodeMap[ch] = i++;
        for (char ch {'0'}; ch <= '9'; ch++) decodeMap[ch] = i++;
        return decodeMap;
    }

    inline std::string base64Decode(const std::string &encoded) {
        if (encoded.empty()) return "";

        const std::unordered_map<char, std::size_t> decodeMap {getDecodeMap()};
        std::size_t padC {0};
        for (const char &ch: encoded) {
            if (ch == '=' && padC < 2) padC++;
            else if ((ch == '=' && padC == 2) || decodeMap.find(ch) == decodeMap.end() || padC)
                throw std::runtime_error("Not a valid Base64 encoded string");
        }

        std::string decoded;
        std::size_t end {encoded.size() - padC}, acc {0}, bitCount {0};
        for (std::size_t idx {0}; idx < end; idx++) {
            std::size_t curr {decodeMap.at(encoded[idx])};
            for (int i {5}; i >= 0; i--) {
                acc = (acc << 1) | ((curr >> i) & 1);
                bitCount++;
                if (bitCount == 8) {
                    decoded += static_cast<char>(acc);
                    acc = 0, bitCount = 0;
                }
            }
        }

        if (bitCount != padC * 2) 
            throw std::runtime_error("Not a valid Base64 encoded string");

        return decoded;
    }
}
