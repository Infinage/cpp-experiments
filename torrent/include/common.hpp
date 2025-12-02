#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <unordered_set>

namespace Torrent {
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

    // Helper to print out the enums
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

    template<std::integral T> 
    T randInteger() {
        static std::mt19937 rng {std::random_device{}()};
        std::uniform_int_distribution<T> gen{
            std::numeric_limits<T>::min(), 
            std::numeric_limits<T>::max()
        };
        return gen(rng);
    }

    struct PieceBlock { 
        std::uint32_t pieceIdx {}, blockOffset {}, blockSize {}; 

        PieceBlock(std::uint32_t pi, std::uint32_t bo, std::uint32_t bs):
            pieceIdx {pi}, blockOffset {bo}, blockSize {bs} {}

        bool operator==(const PieceBlock &other) const {
            return pieceIdx == other.pieceIdx && blockOffset == other.blockOffset;
        }
    };

    struct HashPieceBlock {
        inline std::size_t operator()(const PieceBlock &pb) const {
            std::hash<std::uint32_t> hasher;
            std::size_t h1 {hasher(pb.pieceIdx)}, h2 {hasher(pb.blockOffset)};
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    std::string randString(std::size_t length);
    std::string generatePeerID();

    std::unordered_set<std::uint32_t> readBitField(std::string_view payload);
    std::string writeBitField(const std::unordered_set<std::uint32_t>&);
}
