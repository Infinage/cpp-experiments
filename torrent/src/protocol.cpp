#include "../include/protocol.hpp"
#include "../include/torrent_tracker.hpp"

#include "../../networking/net.hpp"

#include <cstdint>
#include <cstring>

namespace {
    std::string buildMessageHelper(std::size_t bufSize, std::uint32_t msgSize, Torrent::MsgType msgId) {
        std::string buffer(bufSize, '\0');
        msgSize = net::utils::bswap(msgSize);
        std::memcpy(buffer.data() + 0, &msgSize, 4);
        std::memcpy(buffer.data() + 4,   &msgId, 1);
        return buffer;
    }
}

namespace Torrent {
    std::string buildConnectionRequest() {
        char buffer[16] {};
        std::uint32_t transactionId {randInteger<std::uint32_t>()};
        std::uint64_t connectionId {net::utils::bswap<std::uint64_t>(0x41727101980ULL)};
        std::memcpy(buffer +  0, &connectionId, sizeof(connectionId));
        std::memcpy(buffer + 12, &transactionId, sizeof(transactionId));
        return {buffer, 16};
    }
    
    std::string buildAnnounceRequest(const TorrentTracker &tracker, const std::string &connectionId) {
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
        std::memcpy(buffer + 16, tracker.torrentFile.infoHash.c_str(), 20);
        std::memcpy(buffer + 36, tracker.peerID.c_str(), 20);
        std::memcpy(buffer + 64, &tracker.torrentFile.length, 8);
        std::memcpy(buffer + 88, key.c_str(), 4);
        std::memcpy(buffer + 92, &numWant, 4);
        std::memcpy(buffer + 96, &pPort, 2);
        return {buffer, 98};
    }

    std::string buildHandshake(const std::string &infoHash, const std::string &peerID) {
        char buffer[68] {};
        std::uint8_t pstrlen {19}; const char *pStr {"BitTorrent protocol"};
        std::memcpy(buffer +  0, &pstrlen, 1);
        std::memcpy(buffer +  1, pStr, 19);
        std::memcpy(buffer + 28, infoHash.c_str(), 20);
        std::memcpy(buffer + 48, peerID.c_str(), 20);
        return {buffer, 68};
    }

    std::string buildNotinterested() { return buildMessageHelper(5, 1, MsgType::NotInterested); }
    std::string    buildInterested() { return buildMessageHelper(5, 1, MsgType::Interested); }
    std::string     buildKeepAlive() { return std::string{4, '\0'}; }
    std::string       buildUnchoke() { return buildMessageHelper(5, 1, MsgType::Unchoke); }
    std::string         buildChoke() { return buildMessageHelper(5, 1, MsgType::Choke); }

    std::string buildHave(std::uint32_t pIndex) {
        std::string buffer {buildMessageHelper(9, 5, MsgType::Have)};
        pIndex = net::utils::bswap(pIndex);
        std::memcpy(buffer.data() + 5, &pIndex, 4);
        return buffer;
    }

    std::string buildBitField(const std::string &bitfield) {
        std::uint32_t msgSize {static_cast<std::uint32_t>(bitfield.size() + 1)};
        std::string buffer {buildMessageHelper(msgSize + 4, msgSize, MsgType::Bitfield)};
        std::memcpy(buffer.data() + 5, bitfield.data(), msgSize - 1);
        return buffer;
    }

    std::string buildRequest(std::uint32_t pIndex, std::uint32_t pBegin, std::uint32_t pLength, bool cancel) {
        std::string buffer {buildMessageHelper(17, 13, !cancel? MsgType::Request: MsgType::Cancel)};
        net::utils::inplace_bswap(pIndex, pBegin, pLength);
        std::memcpy(buffer.data() +  5,  &pIndex, 4);
        std::memcpy(buffer.data() +  9,  &pBegin, 4);
        std::memcpy(buffer.data() + 13, &pLength, 4);
        return buffer;
    }

    std::string buildPiece(
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

    std::string buildPort(std::uint16_t port) {
        std::string buffer {buildMessageHelper(7, 3, MsgType::Port)};
        port = net::utils::bswap(port);
        std::memcpy(buffer.data() + 5, &port, 2);
        return buffer;
    }

    std::uint32_t IsCompleteMessage(std::string_view buffer) {
        if (buffer.size() < 4) return false;
        std::uint32_t msgLen;
        std::memcpy(&msgLen, buffer.data(), 4);
        msgLen = net::utils::bswap(msgLen);
        return buffer.size() >= msgLen + 4? msgLen: 0;
    }

    std::tuple<MsgType, std::string> parseMessage(std::string_view message) {
        if (message.size() < 4) return {MsgType::Unknown, ""};
        if (message.size() == 4 && message == "\0\0\0\0") return {MsgType::KeepAlive, ""};
        std::uint8_t msgId; std::memcpy(&msgId, message.data() + 4, 1);
        if (msgId > 9) return {MsgType::Unknown, ""};
        return {static_cast<MsgType>(msgId), std::string{message.substr(5)}};
    }
}
