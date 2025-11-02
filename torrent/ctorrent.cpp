// https://allenkim67.github.io/programming/2016/05/04/how-to-make-your-own-bittorrent-client.html
// https://www.rasterbar.com/products/libtorrent/udp_tracker_protocol.html

#include "../networking/net.hpp"
#include "../json-parser/json.hpp"
#include "../cryptography/hashlib.hpp"

#include <cassert>
#include <endian.h>
#include <fstream>
#include <iostream>
#include <print>
#include <random>
#include <ranges>
#include <sstream>
#include <stack>
#include <stdexcept>

namespace Bencode {
    // We will need to sort keys when encoding for creating the info hash (always skip first key)
    std::string encode(JSON::JSONNodePtr root, bool sortKeys = false, bool _skipKey = true) {
        if (!root) return "";
        else {
            std::ostringstream oss;
            const std::string &key {root->getKey()};
            if (!key.empty() && !_skipKey) oss << key.size() << ':' << key;
            switch (root->getType()) {
                case JSON::NodeType::value: {
                    // Only allow string / long
                    auto &val {static_cast<JSON::JSONValueNode&>(*root).getValue()};
                    bool isValid {std::visit([](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;
                        return std::is_same_v<T, std::string> || std::is_same_v<T, long>;
                    }, val)};

                    if (!isValid) 
                        throw std::runtime_error("BEncoder got a non string / int");
                    else if (auto *ptr {std::get_if<std::string>(&val)})
                        oss << ptr->size() << ':' << *ptr;
                    else
                        oss << 'i' << std::get<long>(val) << 'e';
                    break;
                }

                case JSON::NodeType::array: {
                    oss << 'l';
                    for (auto &node: static_cast<JSON::JSONArrayNode&>(*root))
                        oss << encode(node, sortKeys, true);
                    oss << 'e';
                    break;
                }

                case JSON::NodeType::object: {
                    JSON::JSONObjectNode obj {static_cast<JSON::JSONObjectNode&>(*root)};
                    std::vector<JSON::JSONNodePtr> nodes {obj.begin(), obj.end()};
                    if (sortKeys) std::ranges::sort(nodes, [](auto &n1, auto &n2) { return n1->getKey() < n2->getKey(); });
                    oss << 'd';
                    for (auto &node: nodes) oss << encode(node, sortKeys, false);
                    oss << 'e';
                    break;
                }
            }
            return oss.str();
        }
    }

    JSON::JSONHandle decode(const std::string &encoded, bool ignoreSpaces = true) {
        // Extract top and insert it into its ancestor
        auto extract_Push_to_ancestor { [](std::stack<JSON::JSONNodePtr> &stk) { 
            assert(stk.size() > 1);
            auto prevPtr {std::move(stk.top())}; stk.pop();
            if (stk.top()->getType() == JSON::NodeType::value)
                throw std::runtime_error("Invalid bencoded string");
            else if (stk.top()->getType() == JSON::NodeType::object)
                static_cast<JSON::JSONObjectNode&>(*stk.top()).push(std::move(prevPtr));
            else
                static_cast<JSON::JSONArrayNode&>(*stk.top()).push(std::move(prevPtr));
        }};

        // ** Before use ensure that stk top is a simple value node **
        // Used when we have received both key and value
        // We want to set value of parent and if the encoded string
        // is not a simple standalone, extract the parent and insert 
        // it back to its ancestor
        auto pop_Setval_Extract_Push { 
        [&extract_Push_to_ancestor] (std::stack<JSON::JSONNodePtr> &stk, const auto &val) {
            assert(stk.top()->getType() == JSON::NodeType::value);
            JSON::JSONValueNode &prev {static_cast<JSON::JSONValueNode&>(*stk.top())};
            if (!std::holds_alternative<std::nullptr_t>(prev.getValue())) 
                throw std::runtime_error("Invalid bencoded string");

            // Set the value for the existing simplevaluenode (key already set)
            prev.setValue(val);

            // Remove parent and insert into its ancestor (object / array)
            // If size == 1, we assume it is a standalone case
            if (stk.size() > 1) extract_Push_to_ancestor(stk);
        }};

        std::stack<JSON::JSONNodePtr> stk;
        std::size_t idx {}, N {encoded.size()};
        while (idx < N) {
            char ch {encoded[idx]};
            // An object (dict) must have a string key only. Cannot have array / dict as keys
            if ((ch == 'd' || ch == 'l') && (!stk.empty() && stk.top()->getType() == JSON::NodeType::object))
                throw std::runtime_error("Invalid bencoded string");

            else if (ch == 'd') { stk.push(JSON::helper::createObject("", {})); } 
            else if (ch == 'l') { stk.push(JSON::helper::createArray({})); } 

            // Inserting an integer
            // No parent -> Insert as standalone {"", val}
            // Parent is an object -> invalid; int as keys is disallowed
            // Parent is an array -> insert into parent as {"", val}
            // Parent is simple obj -> set int as parents value
            //    If there are more than 1 ancestors in chain
            //    Extract parent and insert back into its parent 
            else if (ch == 'i') {
                std::size_t endP {encoded.find('e', idx)};
                if (endP == std::string::npos || (!stk.empty() && stk.top()->getType() == JSON::NodeType::object))
                    throw std::runtime_error("Invalid bencoded string");

                long val {std::stol(encoded.substr(idx + 1, endP))};
                if (stk.empty())
                    stk.push(JSON::helper::createNode(val));
                else if (stk.top()->getType() == JSON::NodeType::array)
                    static_cast<JSON::JSONArrayNode&>(*stk.top()).push(JSON::helper::createNode(val));
                else // top == simplevaluenode
                    pop_Setval_Extract_Push(stk, val);
                idx = endP;
            } 

            // Inserting a string
            // No parent -> Insert as standalone {"", val}
            // Parent is an object -> insert as {key, ""} onto stack (wait for value)
            // Parent is an array -> insert into parent as {"", val}
            // Parent is simple obj -> set str as parents value
            //    If there are more than 1 ancestors in chain
            //    Extract parent and insert back into its parent 
            else if (std::isdigit(ch)) {
                std::size_t numEndP {encoded.find(':', idx)};
                if (numEndP == std::string::npos)
                    throw std::runtime_error("Invalid bencoded string");

                std::size_t strLen {std::stoull(encoded.substr(idx, numEndP))};
                if (numEndP + 1 + strLen > N) 
                    throw std::runtime_error("Invalid bencoded string");

                std::string str {encoded.substr(numEndP + 1, strLen)};
                if (stk.empty())
                    stk.push(JSON::helper::createNode(str));
                else if (stk.top()->getType() == JSON::NodeType::object)
                    stk.push(JSON::helper::createNode(str, nullptr));
                else if (stk.top()->getType() == JSON::NodeType::array)
                    static_cast<JSON::JSONArrayNode&>(*stk.top()).push(JSON::helper::createNode(str));
                else
                    pop_Setval_Extract_Push(stk, str); 
                idx = numEndP + strLen;
            }

            // End marker
            // Loop typically ends here unless this is a standalone case
            else if (ch == 'e') {
                if (stk.empty() || (idx + 1 == N && stk.size() != 1) || (stk.size() == 1 && idx + 1 < N))
                    throw std::runtime_error("Invalid bencoded string");

                auto prev {std::move(stk.top())}; stk.pop();
                if (idx + 1 == N && stk.empty())
                    return prev;
                else if (stk.top()->getType() == JSON::NodeType::array)
                    static_cast<JSON::JSONArrayNode&>(*stk.top()).push(std::move(prev));
                else if (stk.top()->getType() == JSON::NodeType::object)
                    static_cast<JSON::JSONObjectNode&>(*stk.top()).push(std::move(prev));
                else {
                    prev->setKey(stk.top()->getKey()); 
                    stk.pop(); stk.push(prev);
                    if (stk.size() > 1) extract_Push_to_ancestor(stk);
                }
            }

            else if (!std::isspace(ch) || !ignoreSpaces) {
                throw std::runtime_error("Invalid bencoded string");
            }

            ++idx;
        }

        // Loop typically ends when ch == 'e', this is to support standalone values
        if (stk.size() != 1) throw std::runtime_error("Invalid bencoded string");
        return std::move(stk.top());
    }
}

namespace Torrent {
    class TorrentFile {
        private:
            static std::string randString(std::size_t length) {
                static char chars[] { "0123456789"
                    "abcdefghijklmnopqrstuvwxyz"
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"};
                std::mt19937 rng {std::random_device{}()};
                std::uniform_int_distribution<std::size_t> gen{0, sizeof(chars) - 2};
                std::string str; str.reserve(length);
                while (length--) str.push_back(chars[gen(rng)]);
                return str;
            }

            template<std::integral T> 
            static T randInteger() {
                std::mt19937 rng {std::random_device{}()};
                std::uniform_int_distribution<T> gen{
                    std::numeric_limits<T>::min(), 
                    std::numeric_limits<T>::max()
                };
                return gen(rng);
            }

            std::string buildConnectionRequest() const {
                char buffer[16] {};
                std::uint32_t transactionId {randInteger<std::uint32_t>()};
                std::uint64_t connectionId {net::utils::bswap<std::uint64_t>(0x41727101980ULL)};
                std::memcpy(buffer +  0, &connectionId, sizeof(connectionId));
                std::memcpy(buffer + 12, &transactionId, sizeof(transactionId));
                return {buffer, 16};
            }

            std::string buildAnnounceRequest(const std::string &connectionId) const {
                // Get the data fields to write into the announce request
                std::uint32_t action {net::utils::bswap<std::uint32_t>(1)}, 
                      transactionId {randInteger<std::uint32_t>()}; 
                std::string key {randString(4)}; 
                std::int32_t numWant {net::utils::bswap<std::int32_t>(-1)};
                std::uint16_t pPort {net::utils::bswap<std::uint16_t>(6881)};

                // Construct the announce request
                char buffer[98] {}; // Init to 0 & skip few fields below
                std::memcpy(buffer +  0, connectionId.c_str(), 8);
                std::memcpy(buffer +  8, &action, 4);
                std::memcpy(buffer + 12, &transactionId, 4);
                std::memcpy(buffer + 16, infoHash.c_str(), 20);
                std::memcpy(buffer + 36, peerID.c_str(), 20);
                std::memcpy(buffer + 64, &length, 8);
                std::memcpy(buffer + 88, key.c_str(), 4);
                std::memcpy(buffer + 92, &numWant, 4);
                std::memcpy(buffer + 96, &pPort, 2);
                return {buffer, 98};
            }

            inline std::string_view getPieceHash(std::size_t idx) const {
                if (idx >= numPieces)
                    throw std::runtime_error("Piece Hash idx requested out of range");
                return std::string_view{pieceBlob}.substr(idx * 20, 20);
            }

            // If multiple files exist, compute sum of all files else return length
            static std::uint64_t calculateTotalLength(JSON::JSONHandle info) {
                auto files {info["files"]};
                if (!files.ptr) return static_cast<std::uint64_t>(info.at("length").to<long>());
                else {
                    auto filesObj {files.cast<JSON::JSONArrayNode>()};
                    return std::accumulate(filesObj.begin(), filesObj.end(), std::uint64_t {}, 
                        [] (std::uint64_t acc, JSON::JSONHandle file) {
                            return acc + static_cast<std::uint64_t>(file.at("length").to<long>());
                        }
                    );
                }
            }

            [[nodiscard]] std::vector<std::pair<std::string, std::uint16_t>> getUDPPeers() {
                std::println("Announce url => {}://{}:{}", announceURL.protocol, announceURL.domain, announceURL.port);
                announceURL.resolve();

                // Build & send a connection request (TODO: Implement retry logic)
                std::string cReq {buildConnectionRequest()};
                net::Socket udpSock {net::SOCKTYPE::UDP};
                udpSock.setTimeout(3, 3);
                udpSock.connect(announceURL.ipAddr, announceURL.port);
                long sentBytes {udpSock.send(cReq)};
                std::println("Sent {}/16 bytes to tracker server", sentBytes);
                std::string cResp {udpSock.recv()};
                std::println("Recv {}/16 bytes from tracker server", cResp.size());

                // Validate recv connection response
                if (!(cResp.size() == 16 &&
                    std::equal(cResp.begin(), cResp.begin() + 4, "\0\0\0\0") &&
                    std::equal(cResp.begin() + 4, cResp.begin() + 8, cReq.begin() + 12)))
                    throw std::runtime_error("Invalid conn response from tracker");

                // Build & send a announce request (TODO: Implement retry logic)
                std::string aReq {buildAnnounceRequest(cResp.substr(8, 8))};
                sentBytes = udpSock.send(aReq);
                std::println("Sent {}/98 bytes to tracker server", sentBytes);
                std::string aResp {udpSock.recv()}; // Ensure we read complete IP addrs
                std::println("Recv {}/* bytes from tracker server", aResp.size());

                // Validate announce response
                if (!(aResp.size() >= 16 && 
                    std::equal(aResp.begin(), aResp.begin() + 4, aReq.begin() + 8) &&
                    std::equal(aResp.begin() + 4, aResp.begin() + 8, aReq.begin() + 12)))
                    throw std::runtime_error("Invalid announce response from tracker");

                // Store the tracker-interval, seeders, leechers
                std::memcpy(&interval, aResp.c_str() + 8, 4);
                std::memcpy(&leechers, aResp.c_str() + 12, 4);
                std::memcpy(&seeders, aResp.c_str() + 16, 4);
                interval = net::utils::bswap(interval);
                leechers = net::utils::bswap(leechers);
                seeders = net::utils::bswap(seeders);
                std::println("Seeders: {}, Leechers: {}", seeders, leechers);

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

            [[nodiscard]] std::vector<std::pair<std::string, std::uint16_t>> getTCPPeers() {
                announceURL.params.clear();
                announceURL.setParam("info_hash", infoHash);
                announceURL.setParam("peer_id", peerID);
                announceURL.setParam("port", "6881");
                announceURL.setParam("uploaded", "0");
                announceURL.setParam("downloaded", "0");
                announceURL.setParam("compact", "1");
                announceURL.setParam("left", std::to_string(length));
                net::HttpRequest req {announceURL};
                req.setHeader("user-agent", "CTorrent");
                net::HttpResponse resp{req.execute()};
                std::println("Request: {}\nResponse: {}", req.toString(), resp.raw);
                return {};
            }

            // Associate a random 20 char long peer id
            std::string peerID;

            // Read from torrent file
            net::URL announceURL;
            std::uint32_t pieceLen, interval, seeders, leechers;
            std::uint64_t length;
            std::string name;

            // Process torrent file and store
            std::size_t numPieces;
            std::string pieceBlob, infoHash;

        public:
            TorrentFile(const std::string_view torrentFP): peerID{"-NJT-" + randString(20 - 5)} {
                // Read from file
                std::ifstream ifs {torrentFP.data(), std::ios::binary | std::ios::ate};
                auto size {ifs.tellg()};
                std::string buffer(static_cast<std::size_t>(size), 0);
                ifs.seekg(0, std::ios::beg);
                ifs.read(buffer.data(), size);

                // Read the bencoded torrent file
                auto root {Bencode::decode(buffer)};

                // Extract the announce URL from torrent file
                announceURL = root.at("announce").to<std::string>();
                if (announceURL.protocol != "udp" && announceURL.protocol != "http" && announceURL.protocol != "https")
                    throw std::runtime_error("Torrent announce URL has unsupported protocol: " + announceURL.protocol);

                // Extract other required fields
                auto info {root.at("info")};
                name = info["name"].to<std::string>();
                length = calculateTotalLength(info);
                pieceLen = static_cast<std::uint32_t>(info["piece length"].to<long>()); 
                pieceBlob = info["pieces"].to<std::string>();
                numPieces = pieceBlob.size() / 20;

                // Sanity check on blob validity - can be equally split
                if (pieceBlob.size() % 20) throw std::runtime_error("Piece blob is corrupted");

                // Reencode just the info dict to compute its sha1 hash (raw hash)
                infoHash = hashutil::sha1(Bencode::encode(info.ptr, true), true);
            }

            [[nodiscard]] std::vector<std::pair<std::string, std::uint16_t>> getPeers() {
                return announceURL.protocol == "udp"? getUDPPeers(): getTCPPeers();
            }
    };
};

int main(int argc, char **argv) {
    if (argc != 2) std::println(std::cerr, "Usage: ctorrent <torrent-file>");
    else {
        Torrent::TorrentFile torrent{argv[1]};
        for (auto &[ip, port]: torrent.getPeers())
            std::println("{}:{}", ip, port);
    }
}
