#pragma once

#include "common.hpp"

#include <chrono>
#include <unordered_set>

namespace Torrent {
    struct PeerContext {
        const std::string ip;        // peer IP (for logging)
        const std::uint16_t port;    // peer port
        const bool ipV4;             // IPV4 or IPV6
        const std::string ID;        // uniquely tagging with ip:port

        int fd;                      // Socket file descriptor

        bool handshaked {false};     // whether handshake done
        bool choked {true};          // we are choking them by default
        bool closed {false};         // whether to maintain the connection

        std::uint8_t unchokeAttempts {};   // track # of unchoke attempts and drop if needed
        std::uint8_t reconnectAttempts {}; // track # of unchoke attempts and drop if needed
        std::uint8_t backlog {};           // # of unfulfilled requests pending

        std::unordered_set<std::uint32_t> haves {};  // which pieces the peer has

        // Blocks requested from this peer
        std::unordered_set<PieceBlock, HashPieceBlock> pending {};

        std::string recvBuffer {};   // accumulate partial message data
        std::string sendBuffer {};   // pending outgoing data

        // Last read event we received from the client
        std::chrono::steady_clock::time_point lastReadTimeStamp;

        // Resets all non const fields to defaults
        inline void onReconnect(int newFd, auto &tick) {
            fd = newFd; ++reconnectAttempts;
            handshaked = false; choked = true; closed = false;
            unchokeAttempts = 0; backlog = 0;
            haves.clear(); pending.clear(); 
            recvBuffer.clear(); sendBuffer.clear();
            lastReadTimeStamp = tick;
        }
    };
}
