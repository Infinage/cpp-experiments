#pragma once

#include "torrent_file.hpp"

#include "../../networking/net.hpp"

namespace Torrent {
    class TorrentTracker {
            private:
                std::vector<std::pair<std::string, std::uint16_t>> getUDPPeers(int timeout);
                std::vector<std::pair<std::string, std::uint16_t>> getTCPPeers(int timeout);

            public:
                const std::string peerID;
                const TorrentFile &torrentFile;
                mutable net::URL announceURL;
                std::uint32_t interval, seeders, leechers;

            public:
                TorrentTracker(const TorrentFile &torrentFile);
                [[nodiscard]] std::vector<std::pair<std::string, std::uint16_t>> getPeers(int timeout);
        };
}
