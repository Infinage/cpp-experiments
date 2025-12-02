#include "../include/common.hpp"
#include <algorithm>

namespace Torrent {
    std::string randString(std::size_t length) {
        static char chars[] { "0123456789"
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"};
        static std::mt19937 rng {std::random_device{}()};
        std::uniform_int_distribution<std::size_t> gen{0, sizeof(chars) - 2};
        std::string str; str.reserve(length);
        while (length--) str.push_back(chars[gen(rng)]);
        return str;
    }

    std::string generatePeerID() {
        static std::string peerID {"-NJT-" + randString(20 - 5)};
        return peerID;
    }

    std::unordered_set<std::uint32_t> readBitField(std::string_view payload) {
        std::unordered_set<std::uint32_t> haves;
        std::uint32_t bitIdx {};
        for (char ch: payload) {
            std::uint8_t byte {static_cast<std::uint8_t>(ch)};
            for (std::uint8_t bit {8}; bit-- > 0; ++bitIdx) {
                if (byte & (1u << bit)) 
                    haves.insert(bitIdx);
            }
        }
        return haves;
    }

    std::string writeBitField(const std::unordered_set<std::uint32_t> &haves) {
        if (haves.empty()) return "";
        std::size_t size {(*std::ranges::max_element(haves) / 8) + 1};
        std::string bitField(size, '\0');
        for (auto have: haves) bitField[have / 8] |= 1u << (7 - have % 8);
        return bitField;
    }
}
