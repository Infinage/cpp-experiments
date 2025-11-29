#pragma once

#include "common.hpp"

namespace Torrent {
    // Forward declaration for faster compilation times
    class TorrentTracker;

    // Helpers to construct the messages
    std::string buildConnectionRequest();
    std::string buildAnnounceRequest(const TorrentTracker &tracker, const std::string &connectionId);
    std::string buildHandshake(const std::string &infoHash, const std::string &peerID);
    std::string buildNotinterested();
    std::string buildInterested();
    std::string buildKeepAlive();
    std::string buildUnchoke();
    std::string buildChoke();
    std::string buildHave(std::uint32_t pIndex);
    std::string buildBitField(const std::string &bitfield);
    std::string buildRequest(std::uint32_t pIndex, std::uint32_t pBegin, std::uint32_t pLength, bool cancel = false);
    std::string buildPiece(std::uint32_t pIndex, std::uint32_t pBegin, const std::string &block);
    std::string buildPort(std::uint16_t port);

    // Request Message parser
    std::uint32_t IsCompleteMessage(std::string_view buffer);
    std::tuple<MsgType, std::string> parseMessage(std::string_view message);
}
