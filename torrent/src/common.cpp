#include "../include/common.hpp"

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
}
