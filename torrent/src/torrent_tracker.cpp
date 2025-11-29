#include "../include/torrent_tracker.hpp"
#include "../include/common.hpp"
#include "../include/bencode.hpp"
#include "../include/protocol.hpp"

namespace Torrent {
    std::vector<std::pair<std::string, std::uint16_t>> TorrentTracker::getUDPPeers(long timeout) {
        // Build & send a connection request (TODO: Implement retry logic)
        [[maybe_unused]] long sentBytes;
        std::string cReq {buildConnectionRequest()};
        net::Socket udpSock {net::SOCKTYPE::UDP};
        udpSock.setTimeout(timeout, timeout);
        udpSock.connect(announceURL.ipAddr, announceURL.port);
        sentBytes = udpSock.send(cReq);
        std::string cResp {udpSock.recv()};

        // Validate recv connection response
        if (!(cResp.size() == 16 &&
            std::equal(cResp.begin(), cResp.begin() + 4, "\0\0\0\0") &&
            std::equal(cResp.begin() + 4, cResp.begin() + 8, cReq.begin() + 12)))
            throw std::runtime_error("Invalid conn response from tracker");

        // Build & send a announce request (TODO: Implement retry logic)
        std::string aReq {buildAnnounceRequest(torrentFile, cResp.substr(8, 8))};
        sentBytes = udpSock.send(aReq);
        std::string aResp {udpSock.recv()}; // Ensure we read complete IP addrs

        // Validate announce response
        if (!(aResp.size() >= 16 && 
            std::equal(aResp.begin(), aResp.begin() + 4, aReq.begin() + 8) &&
            std::equal(aResp.begin() + 4, aResp.begin() + 8, aReq.begin() + 12)))
            throw std::runtime_error("Invalid announce response from tracker");

        // Store the tracker-interval, seeders, leechers
        std::memcpy(&interval, aResp.c_str() + 8, 4);
        std::memcpy(&leechers, aResp.c_str() + 12, 4);
        std::memcpy(&seeders, aResp.c_str() + 16, 4);
        net::utils::inplace_bswap(interval, leechers, seeders);

        // Extract the peers
        std::vector<std::pair<std::string, std::uint16_t>> peers;
        auto substrs {std::string_view{aResp.data() + 20, aResp.size() - 20} | std::ranges::views::chunk(6)};
        for (auto substr: substrs) {
            std::string ip {substr.begin(), substr.begin() + 4};
            std::uint16_t port; std::memcpy(&port, substr.data() + 4, 2);
            peers.push_back({net::utils::ipBytesToString(ip), net::utils::bswap(port)});
        }

        return peers;
    }

    std::vector<std::pair<std::string, std::uint16_t>> TorrentTracker::getTCPPeers(long timeout) {
        announceURL.params.clear();
        announceURL.setParam("info_hash", torrentFile.infoHash);
        announceURL.setParam("peer_id", peerID);
        announceURL.setParam("port", "6881");
        announceURL.setParam("uploaded", "0");
        announceURL.setParam("downloaded", "0");
        announceURL.setParam("left", std::to_string(torrentFile.length));
        net::HttpRequest req {announceURL};
        req.setHeader("user-agent", "CTorrent");
        net::HttpResponse resp{req.execute(timeout)};

        std::string respBodyStr {resp.header("transfer-encoding") == "chunked"? resp.unchunk(): resp.body};
        JSON::JSONHandle respBody {Bencode::decode(respBodyStr)};
        std::vector<std::pair<std::string, std::uint16_t>> peers;
        for (JSON::JSONHandle ipObj: respBody["peers"])
            peers.push_back({ipObj["ip"].to<std::string>(), ipObj["port"].to<long>()});     

        return peers;
    }

    std::vector<std::pair<std::string, std::uint16_t>> TorrentTracker::getPeers(long timeout) {
        return announceURL.protocol == "udp"? getUDPPeers(timeout): getTCPPeers(timeout);
    }

    TorrentTracker::TorrentTracker(const TorrentFile &torrentFile): 
        peerID {generatePeerID()}, torrentFile{torrentFile}, 
        announceURL{torrentFile.announceURL} 
    { announceURL.resolve(); }
}
