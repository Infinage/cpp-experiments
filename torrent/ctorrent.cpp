// https://allenkim67.github.io/programming/2016/05/04/how-to-make-your-own-bittorrent-client.html
// https://www.rasterbar.com/products/libtorrent/udp_tracker_protocol.html

#include "../networking/net.hpp"
#include "../json-parser/json.hpp"
#include "../cryptography/hashlib.hpp"

#include <cassert>
#include <chrono>
#include <endian.h>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <random>
#include <ranges>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <unordered_set>

namespace Bencode {
    // We will need to sort keys when encoding for creating the info hash (always skip first key)
    [[nodiscard]] std::string encode(JSON::JSONNodePtr root, bool sortKeys = false, bool _skipKey = true) {
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

    [[nodiscard]] JSON::JSONHandle decode(const std::string &encoded, bool ignoreSpaces = true) {
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
            // Associate a random 20 char long peer id
            std::string peerID;

            // Read from torrent file
            net::URL announceURL;
            std::uint32_t pieceLen, interval, seeders, leechers;
            std::uint64_t length;
            std::string name;

            // User defined constants
            std::uint16_t BLOCK_SIZE;
            std::uint8_t MAX_BACKLOG, MAX_UNCHOKE_ATTEMPTS;

            // Pieces that we have completed downloading
            std::unordered_set<std::uint32_t> haves;
            std::unordered_map<std::uint32_t, std::unordered_set<std::uint32_t>> pending;

            // Process torrent file and store
            std::size_t numPieces, numBlocks;
            std::string pieceBlob, infoHash;

            struct PeerContext {
                int fd;                      // file descriptor, for lookup
                std::string ip;              // peer IP (for logging)
                std::uint16_t port;          // peer port

                bool handshaked {false};     // whether handshake done
                bool choked {true};          // we are choking them by default
                bool valid {true};           // whether to maintain the connection

                int unchokeAttempts {};      // track # of unchoke attempts and drop if needed
                short backlog {};            // # of unfulfilled requests pending

                std::unordered_set<std::uint32_t> haves {};  // which pieces the peer has

                std::string recvBuffer {};   // accumulate partial message data
                std::string sendBuffer {};   // pending outgoing data

                std::chrono::steady_clock::time_point lastActivity {};

                std::string str() const { return std::format("[{}:{}]", ip, port); }

                static bool IsCompleteMessage(std::string_view buffer) {
                    if (buffer.size() < 4) return false;
                    std::uint32_t msgLen;
                    std::memcpy(&msgLen, buffer.data(), 4);
                    msgLen = net::utils::bswap(msgLen);
                    return buffer.size() >= msgLen + 4;
                }
            };

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

            std::vector<std::pair<std::string, std::uint16_t>> getUDPPeers(long timeout) {
                announceURL.resolve();

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
                std::string aReq {buildAnnounceRequest(cResp.substr(8, 8))};
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

            std::vector<std::pair<std::string, std::uint16_t>> getTCPPeers(long timeout) {
                announceURL.params.clear();
                announceURL.setParam("info_hash", infoHash);
                announceURL.setParam("peer_id", peerID);
                announceURL.setParam("port", "6881");
                announceURL.setParam("uploaded", "0");
                announceURL.setParam("downloaded", "0");
                announceURL.setParam("left", std::to_string(length));
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
            
            enum class MsgType : std::uint8_t {
                Choke = 0,
                Unchoke = 1,
                Interested = 2,
                NotInterested = 3,
                Have = 4,
                Bitfield = 5,
                Request = 6,
                Piece = 7,
                Cancel = 8,
                Port = 9,
                KeepAlive = 99,
                Unknown = 100
            };

            constexpr std::string_view str(MsgType msg) {
                switch (msg) {
                    case MsgType::Choke:         return "Choke";
                    case MsgType::Unchoke:       return "Unchoke";
                    case MsgType::Interested:    return "Interested";
                    case MsgType::NotInterested: return "NotInterested";
                    case MsgType::Have:          return "Have";
                    case MsgType::Bitfield:      return "Bitfield";
                    case MsgType::Request:       return "Request";
                    case MsgType::Piece:         return "Piece";
                    case MsgType::Cancel:        return "Cancel";
                    case MsgType::Port:          return "Port";
                    case MsgType::KeepAlive:     return "KeepAlive";
                    default:                     return "Unknown";
                }
            }

            static std::string buildMessageHelper(
                std::size_t bufSize, std::uint32_t msgSize, MsgType msgId
            ) {
                std::string buffer(bufSize, '\0');
                msgSize = net::utils::bswap(msgSize);
                std::memcpy(buffer.data() + 0, &msgSize, 4);
                std::memcpy(buffer.data() + 4,   &msgId, 1);
                return buffer;
            }

            static std::string buildHandshake(const std::string &infoHash, const std::string &peerID) {
                char buffer[68] {};
                std::uint8_t pstrlen {19}; const char *pStr {"BitTorrent protocol"};
                std::memcpy(buffer +  0, &pstrlen, 1);
                std::memcpy(buffer +  1, pStr, 19);
                std::memcpy(buffer + 28, infoHash.c_str(), 20);
                std::memcpy(buffer + 48, peerID.c_str(), 20);
                return {buffer, 68};
            }

            static std::string buildNotinterested() { return buildMessageHelper(5, 1, MsgType::NotInterested); }
            static std::string    buildInterested() { return buildMessageHelper(5, 1, MsgType::Interested); }
            static std::string     buildKeepAlive() { return std::string{4, '\0'}; }
            static std::string       buildUnchoke() { return buildMessageHelper(5, 1, MsgType::Unchoke); }
            static std::string         buildChoke() { return buildMessageHelper(5, 1, MsgType::Choke); }

            static std::string buildHave(std::uint32_t pIndex) {
                std::string buffer {buildMessageHelper(9, 5, MsgType::Have)};
                pIndex = net::utils::bswap(pIndex);
                std::memcpy(buffer.data() + 5, &pIndex, 4);
                return buffer;
            }

            static std::string buildBitField(const std::string &bitfield) {
                std::uint32_t msgSize {static_cast<std::uint32_t>(bitfield.size() + 1)};
                std::string buffer {buildMessageHelper(msgSize + 4, msgSize, MsgType::Bitfield)};
                std::memcpy(buffer.data() + 5, bitfield.data(), msgSize - 1);
                return buffer;
            }

            static std::string buildRequest(
                std::uint32_t pIndex, std::uint32_t pBegin, 
                std::uint32_t pLength, bool cancel = false
            ) {
                std::string buffer {buildMessageHelper(17, 13, !cancel? MsgType::Request: MsgType::Cancel)};
                net::utils::inplace_bswap(pIndex, pBegin, pLength);
                std::memcpy(buffer.data() +  5,  &pIndex, 4);
                std::memcpy(buffer.data() +  9,  &pBegin, 4);
                std::memcpy(buffer.data() + 13, &pLength, 4);
                return buffer;
            }

            static std::string buildPiece(
                std::uint32_t pIndex, std::uint32_t pBegin, const std::string &block
            ) {
                auto blockSize {static_cast<std::uint32_t>(block.size())};
                std::string buffer {buildMessageHelper(blockSize + 13, blockSize + 9, MsgType::Piece)};
                net::utils::inplace_bswap(pIndex, pBegin);
                std::memcpy(buffer.data() +  5, &pIndex, 4);
                std::memcpy(buffer.data() +  9, &pBegin, 4);
                std::memcpy(buffer.data() + 13, block.data(), blockSize);
                return buffer;
            }

            static std::string buildPort(std::uint16_t port) {
                std::string buffer {buildMessageHelper(7, 3, MsgType::Port)};
                port = net::utils::bswap(port);
                std::memcpy(buffer.data() + 5, &port, 2);
                return buffer;
            }

            static std::tuple<MsgType, std::string> parseMessage(std::string_view message) {
                if (message.size() < 4) return {MsgType::Unknown, ""};
                if (message.size() == 4 && message == "\0\0\0\0") return {MsgType::KeepAlive, ""};
                std::uint8_t msgId; std::memcpy(&msgId, message.data() + 4, 1);
                if (msgId > 9) return {MsgType::Unknown, ""};
                return {static_cast<MsgType>(msgId), std::string{message.substr(5)}};
            }

            static void handleHave(PeerContext &ctx, const std::string &payload) {
                std::uint32_t pieceIdx;
                std::memcpy(&pieceIdx, payload.c_str(), 4);
                ctx.haves.insert(net::utils::bswap(pieceIdx));
            }

            static void handleBitfield(PeerContext &ctx, const std::string &payload) {
                std::uint32_t bitIdx {};
                for (char ch: payload) {
                    std::uint8_t byte {static_cast<std::uint8_t>(ch)};
                    for (std::uint8_t bit {8}; bit-- > 0; ++bitIdx) {
                        if (byte & (1u << bit)) 
                            ctx.haves.insert(bitIdx);
                    }
                }
            }

            static void handleChoke(PeerContext &ctx, const std::string&) {
                ctx.choked = true;
            }

            static void handleUnchoke(PeerContext &ctx, const std::string&) {
                ctx.unchokeAttempts = 0; ctx.choked = false;
            }

        public:
            TorrentFile(
                const std::string_view torrentFP, const std::uint16_t bSize = 1 << 14, 
                const std::uint8_t backlog = 8, const std::uint8_t unchokeAttempts = 3
            ): 
                peerID{"-NJT-" + randString(20 - 5)}, 
                BLOCK_SIZE {bSize}, MAX_BACKLOG {backlog},
                MAX_UNCHOKE_ATTEMPTS {unchokeAttempts}
            {
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
                numBlocks = (pieceLen + BLOCK_SIZE - 1) / BLOCK_SIZE;

                // Sanity check on blob validity - can be equally split
                if (pieceBlob.size() % 20) throw std::runtime_error("Piece blob is corrupted");

                // Reencode just the info dict to compute its sha1 hash (raw hash)
                infoHash = hashutil::sha1(Bencode::encode(info.ptr, true), true);
            }

            [[nodiscard]] std::vector<std::pair<std::string, std::uint16_t>> getPeers(long timeout = 10) {
                return announceURL.protocol == "udp"? getUDPPeers(timeout): getTCPPeers(timeout);
            }

            void download(std::string_view directory = ".") {
                std::vector<std::pair<std::string, std::uint16_t>> peerList {getPeers()};
                if (peerList.empty()) { std::println(std::cerr, "No peers available"); return; }
                std::println("Discovered {} peers.", peerList.size());

                // Connect to all available peers
                std::string handshake {buildHandshake(infoHash, peerID)};
                net::PollManager manager; std::unordered_map<int, PeerContext> states;
                for (const auto &[ip, port]: peerList) {
                    std::optional<net::IP> ipType {net::utils::checkIPType(ip)};
                    if (!ipType.has_value()) { std::println(std::cerr, "Invalid IP: {}", ip); continue; }
                    auto peer {net::Socket{net::SOCKTYPE::TCP, ipType.value()}};
                    peer.setNonBlocking(); peer.connect(ip, port);
                    states.insert({peer.fd(), {.fd=peer.fd(), .ip=ip, .port=port, .sendBuffer=handshake}});
                    manager.track(std::move(peer));
                    if (manager.size() >= 5) break; // TODO (Just connect with a few now for debugging)
                }

                while (haves.size() < numPieces) {
                    for (auto &[peer, event]: manager.poll()) {
                        PeerContext &ctx {states.at(peer.fd())};
                        ctx.lastActivity = std::chrono::steady_clock::now();

                        if (event & net::PollEventType::Readable) {
                            std::string recvBytes {peer.recvAll()}; std::size_t iSize {ctx.recvBuffer.size()};
                            ctx.recvBuffer.resize(iSize + recvBytes.size());
                            std::memmove(ctx.recvBuffer.data() + iSize, recvBytes.data(), recvBytes.size());

                            if (!ctx.handshaked && ctx.recvBuffer.size() >= 68) {
                                ctx.handshaked = std::memcmp(handshake.data(), ctx.recvBuffer.data(), 20) == 0 
                                    && std::memcmp(handshake.data() + 28, ctx.recvBuffer.data() + 28, 20) == 0;
                                if (!ctx.handshaked) { ctx.recvBuffer.clear(); ctx.sendBuffer = handshake; } 
                                else {
                                    ctx.recvBuffer = ctx.recvBuffer.substr(68);
                                    std::println("Handshake established with client: {}", ctx.str());
                                    ctx.sendBuffer = buildUnchoke() + buildInterested();
                                }
                            }

                            else if (ctx.handshaked && PeerContext::IsCompleteMessage(ctx.recvBuffer)) {
                                auto [msgType, message] {parseMessage(ctx.recvBuffer)};
                                std::println("Received message of type: {} from client: {}", str(msgType), ctx.str());

                                // Process the message and update the internal context
                                switch (msgType) {
                                    case MsgType::Choke: handleChoke(ctx, message); break;
                                    case MsgType::Unchoke: handleUnchoke(ctx, message); break;
                                    case MsgType::Have: handleHave(ctx, message); break;
                                    case MsgType::Bitfield: 
                                        if (message.size() != (length + 7) / 8)
                                            std::println(std::cerr, "Bitfield received from client "
                                                "has invalid length: {}", message.size());
                                        handleBitfield(ctx, message); 
                                        break;
                                }

                                // If choked, request for gettin unchoked. We will wait for a 
                                // maximum of 3 turns before disconnecting from the peer
                                if (ctx.choked) {
                                    if (ctx.unchokeAttempts > MAX_UNCHOKE_ATTEMPTS) ctx.valid = false;
                                    else {
                                        ctx.sendBuffer = buildUnchoke();
                                        ctx.backlog = 0;
                                        ++ctx.unchokeAttempts;
                                    }
                                }

                                // If unchoked and peer not throttled, try requesting for a piece from peer
                                else if (ctx.backlog < MAX_BACKLOG) {
                                    for (std::uint32_t pHave: ctx.haves) {
                                        if (haves.count(pHave) == 0) {
                                            for (std::uint32_t block {}; block < numBlocks; ++block) {
                                                if (pending[pHave].count(block) == 0) {
                                                    ctx.sendBuffer += buildRequest(pHave, block, BLOCK_SIZE);
                                                    ++ctx.backlog;
                                                    pending[pHave].insert(block);
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                } 

                                ctx.recvBuffer.clear();
                                if (!ctx.valid) 
                                    manager.untrack(ctx.fd), 
                                        states.erase(ctx.fd); 
                            }
                        }

                        else if (event & net::PollEventType::Writable) {
                            long sentBytes {peer.sendAll(ctx.sendBuffer)};
                            std::println("Sent {} bytes to client: {}", sentBytes, ctx.str());
                            ctx.sendBuffer = ctx.sendBuffer.substr(static_cast<std::size_t>(sentBytes));
                        }

                        else {
                            manager.untrack(ctx.fd);
                            states.erase(ctx.fd);
                        }
                    }
                }
            }
    };
};

int main(int argc, char **argv) {
    if (argc != 2) std::println(std::cerr, "Usage: ctorrent <torrent-file>");
    else {
        Torrent::TorrentFile torrent{argv[1]};
        torrent.download();
    }
}
