#pragma once

#include "common.hpp"

#include <chrono>
#include <unordered_set>

namespace Torrent {
    struct PeerContext {
        int fd;                      // file descriptor, for lookup
        std::string ip;              // peer IP (for logging)
        std::uint16_t port;          // peer port

        bool handshaked {false};     // whether handshake done
        bool choked {true};          // we are choking them by default
        bool closed {false};         // whether to maintain the connection

        std::uint8_t unchokeAttempts {}; // track # of unchoke attempts and drop if needed
        std::uint8_t backlog {};         // # of unfulfilled requests pending

        std::unordered_set<std::uint32_t> haves {};  // which pieces the peer has

        // Blocks requested from this peer
        std::unordered_set<PieceBlock, HashPieceBlock> pending {};

        std::string recvBuffer {};   // accumulate partial message data
        std::string sendBuffer {};   // pending outgoing data

        std::chrono::steady_clock::time_point lastActivity {};

        inline std::string str() const { return std::format("[{}:{}]", ip, port); }
    };
}
