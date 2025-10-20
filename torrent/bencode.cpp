// https://allenkim67.github.io/programming/2016/05/04/how-to-make-your-own-bittorrent-client.html
#include "../networking/net.hpp"
#include "../json-parser/json.hpp"
#include "../cryptography/hashlib.hpp"
#include <cassert>
#include <endian.h>
#include <fstream>
#include <print>
#include <random>
#include <sstream>
#include <stack>
#include <stdexcept>

namespace Bencode {
    std::string encode(JSON::JSONNodePtr root) {
        if (!root) return "";
        else {
            std::ostringstream oss;
            const std::string &key {root->getKey()};
            if (!key.empty()) oss << key.size() << ':' << key;
            switch (root->getType()) {
                case JSON::NodeType::value: {
                    // Only allow string / long
                    auto &val {static_cast<JSON::JSONValueNode&>(*root).getValue()};
                    bool isValid {std::visit([](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;
                        return std::is_same_v<T, std::string> || std::is_same_v<T, long>;
                    }, val)};

                    if (!isValid) 
                        throw std::runtime_error("Encoder got a non string / int");
                    else if (auto *ptr {std::get_if<std::string>(&val)})
                        oss << ptr->size() << ':' << *ptr;
                    else
                        oss << 'i' << std::get<long>(val) << 'e';
                    break;
                }

                case JSON::NodeType::array: {
                    oss << 'l';
                    for (auto &node: static_cast<JSON::JSONArrayNode&>(*root))
                        oss << encode(node);
                    oss << 'e';
                    break;
                }

                case JSON::NodeType::object: {
                    oss << 'd';
                    for (auto &node: static_cast<JSON::JSONObjectNode&>(*root))
                        oss << encode(node);
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

            [[nodiscard]] std::string buildTrackerURL() const {
                return std::format(
                    "{}?{}={}&{}={}&uploaded=0&downloaded=0&compact=1&left={}", 
                    announceURL, "info_hash", infoHash, "peer_id", peerID, length
                );
            }

            [[nodiscard]] std::string buildConnectionRequest() const {
                char buffer[16];
                std::uint64_t connectionId {htobe64(0x41727101980)};
                std::uint32_t action {0}, transactionId {randInteger<std::uint32_t>()};
                std::memcpy(buffer +  0, &connectionId, sizeof(connectionId));
                std::memcpy(buffer +  8, &action, sizeof(action));
                std::memcpy(buffer + 12, &transactionId, sizeof(transactionId));
                return {buffer, 16};
            }

            // Associate a random 20 char long peer id
            std::string peerID;

            // Extract port from Announce URL
            std::uint16_t announcePort;

            // Read from torrent file
            std::string announceURL, announceDomain, announcePath;
            long length, pieceLen;
            std::string name;

            // Process torrent file and store
            std::size_t numPieces;
            std::string pieceBlob, infoHash;

        public:
            [[nodiscard]] inline std::string_view getPieceHash(std::size_t idx) const {
                if (idx >= numPieces)
                    throw std::runtime_error("Piece Hash idx requested out of range");
                return std::string_view{pieceBlob}.substr(idx * 20, 20);
            }

            TorrentFile(const std::string_view torrentFP): peerID{randString(20)} {
                // Read from file
                std::ifstream ifs {torrentFP.data(), std::ios::binary | std::ios::ate};
                auto size {ifs.tellg()};
                std::string buffer(static_cast<std::size_t>(size), 0);
                ifs.seekg(0, std::ios::beg);
                ifs.read(buffer.data(), size);

                // Read the bencoded torrent file
                auto root {Bencode::decode(buffer)};

                // Ensure announce protocol is UDP (http unsupported for now)
                std::string announceProto;
                announceURL = root["announce"].to<std::string>();
                std::tie(announceProto, announceDomain, announcePort, announcePath) 
                    = net::utils::extractURLPieces(announceURL);
                if (announceProto != "udp") 
                    throw std::runtime_error("Unsuported announce protocol: " + announceProto);

                // Extract other required fields
                name = root["info"]["name"].to<std::string>();
                length = root["info"]["length"].to<long>();
                pieceLen = root["info"]["piece length"].to<long>(); 
                pieceBlob = root["info"]["pieces"].to<std::string>();
                numPieces = pieceBlob.size() / 20;

                // Sanity check on blob validity - can be equally split
                if (pieceBlob.size() % 20) throw std::runtime_error("Piece blob is corrupted");

                // Reencode just the info dict to compute its sha1 hash
                infoHash = hashutil::sha1(Bencode::encode(root["info"].ptr));
            }

            std::vector<std::string> getPeers() const {
                net::Socket udpSock {net::SOCKTYPE::UDP};
                udpSock.setTimeout(3, 3);
                std::string ipAddr {net::utils::resolveHostname(announceDomain)};
                std::println("Resolving announce url domain: {} => {}", announceDomain, ipAddr);
                udpSock.connect(ipAddr, announcePort);
                std::string connReq {buildConnectionRequest()};
                long sentBytes {udpSock.send(connReq)};
                std::println("Sent {}/16 bytes to tracker server", sentBytes);
                std::string raw {udpSock.recv()};
                std::println("Received from tracker: {}", raw);
                return {};
            }
    };
};

int main() {
    Torrent::TorrentFile torrent{"alpine.torrent"};
    torrent.getPeers();
}
