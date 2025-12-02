#include "../include/piece_manager.hpp"
#include "../../cryptography/hashlib.hpp"

#include <algorithm>
#include <cstring>
#include <print>
#include <stdexcept>

namespace Torrent {
    bool PieceManager::Piece::finished() const { return completedBlocks == actualNumBlocks; }

    void PieceManager::Piece::writeBlock(std::uint32_t blockOffset, std::string piecePayload) {
        if (blockOffset >= actualPieceSize) throw std::runtime_error("Writing out of bounds");
        auto writeSize {std::min(static_cast<std::uint32_t>(outer.blockSize), actualPieceSize - blockOffset)};
        std::memcpy(buffer.data() + blockOffset, piecePayload.c_str() + 8, writeSize);
        states[blockOffset / outer.blockSize] = State::DONE;
        --requestedBlocks; ++completedBlocks;
    }

    std::uint16_t PieceManager::Piece::requestBlockNum(std::uint16_t blockIdx) {
        states[blockIdx] = Piece::State::REQUESTED; ++requestedBlocks;
        return blockIdx < actualNumBlocks - 1? outer.blockSize: lastBlockSize;
    }

    PieceManager::Piece::Piece(const PieceManager &outer, std::uint32_t pieceIdx): 
        outer {outer}, buffer {std::string(outer.pieceSize, '\0')},
        actualPieceSize {pieceIdx < outer.numPieces - 1 || outer.totalSize % outer.pieceSize == 0? 
            outer.pieceSize: static_cast<uint32_t>(outer.totalSize % outer.pieceSize)},
        lastBlockSize {actualPieceSize % outer.blockSize == 0? outer.blockSize: 
            static_cast<std::uint16_t>(actualPieceSize % outer.blockSize)},
        actualNumBlocks {static_cast<uint16_t>((actualPieceSize + outer.blockSize - 1) / outer.blockSize)},
        states {std::vector<State>(actualNumBlocks, State::PENDING)}
    {}

    bool PieceManager::finished() const { return haves.size() == numPieces; }

    PieceManager::PieceManager(
            const std::uint64_t totalSize, const std::uint32_t pieceSize, 
            const std::uint16_t blockSize, const std::string &pieceBlob
    ):
        totalSize {totalSize}, pieceSize {pieceSize}, blockSize {blockSize},
        numPieces {static_cast<std::uint32_t>(totalSize + pieceSize - 1) / pieceSize},
        numBlocks {static_cast<std::uint16_t>((pieceSize + blockSize - 1) / blockSize)},
        pieceBlob {pieceBlob}
    {}

    std::string_view PieceManager::getPieceHash(std::size_t idx) const {
        if (idx >= numPieces) throw std::runtime_error("Piece Hash idx requested out of range");
        return std::string_view{pieceBlob}.substr(idx * 20, 20);
    }

    void PieceManager::clearInTransitBlock(std::uint32_t pieceIdx, std::uint32_t blockOffset) {
        Piece &piece {partialPieces.at(pieceIdx)};
        piece.states[blockOffset / blockSize] = Piece::State::PENDING;
        --piece.requestedBlocks;
        if (!piece.requestedBlocks && !piece.completedBlocks)
            partialPieces.erase(pieceIdx);
    }

    void PieceManager::onPeerReset(const std::vector<std::pair<std::uint32_t, std::uint32_t>> &pendingRequests) {
        for (const auto &request: pendingRequests) {
            clearInTransitBlock(request.first, request.second);
        }
    }

    std::pair<bool, std::string> PieceManager::onBlockReceived(const std::uint32_t pieceIdx, 
        const std::uint32_t blockOffset, std::string piecePayload) 
    {
        auto &partialPiece {partialPieces.at(pieceIdx)};
        partialPiece.writeBlock(blockOffset, std::move(piecePayload));

        bool pieceCompleted {false}; std::string retBuffer;
        if (partialPiece.completedBlocks == partialPiece.actualNumBlocks) {
            if (hashutil::sha1(partialPiece.buffer, true) != getPieceHash(pieceIdx)) {
                std::println("Mismatching hash for piece# {} will be dropped and rerequested", pieceIdx);
            } else {
                std::println("Hash for piece# {} is valid, scheduling write to disk", pieceIdx);
                retBuffer = std::string {partialPiece.buffer.c_str(), partialPiece.actualPieceSize};
                pieceCompleted = true; haves.insert(pieceIdx);
            }
            partialPieces.erase(pieceIdx);
        }
        return {pieceCompleted, retBuffer};
    }

    std::vector<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>>
    PieceManager::getPendingBlocks(const std::unordered_set<std::uint32_t> &peerHaves, std::uint8_t count) {
        std::vector<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> result;

        // Priority is to clear the partial pieces so we don't have too much in memory
        for (auto partialIt {partialPieces.begin()}; partialIt != partialPieces.end() && result.size() < count; ++partialIt) {
            auto &[pieceIdx, piece] {*partialIt};
            if (peerHaves.contains(pieceIdx)) {
                std::uint16_t blockIdx {};
                while (blockIdx < piece.actualNumBlocks && result.size() < count) {
                    if (piece.states[blockIdx] == Piece::State::PENDING) {
                        std::uint16_t actualBlockSize {piece.requestBlockNum(blockIdx)};
                        result.emplace_back(pieceIdx, static_cast<std::uint32_t>(blockIdx) * blockSize, actualBlockSize);        
                    }
                    ++blockIdx;
                }
            }
        }

        // Pick the pieces that we haven't downloaded or requested yet
        for (auto peerHavesIt {peerHaves.begin()}; peerHavesIt != peerHaves.end() && result.size() < count; ++peerHavesIt) {
            auto pieceIdx {*peerHavesIt};
            if (!haves.contains(pieceIdx) && !partialPieces.contains(pieceIdx)) {
                auto inserted {partialPieces.emplace(pieceIdx, Piece{*this, pieceIdx})};
                Piece &piece {inserted.first->second}; std::uint16_t blockIdx {};
                while (blockIdx < piece.actualNumBlocks && result.size() < count) {
                    std::uint16_t actualBlockSize {piece.requestBlockNum(blockIdx)};
                    result.emplace_back(pieceIdx, static_cast<std::uint32_t>(blockIdx++) * blockSize, actualBlockSize);
                }
            }
        }

        return result;
    }
}
