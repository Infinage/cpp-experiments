#include "../include/piece_manager.hpp"

#include "../../cryptography/hashlib.hpp"
#include "../../misc/logger.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace Torrent {
    bool PieceManager::Piece::finished() const { return completedBlocks == actualNumBlocks; }

    void PieceManager::Piece::writeBlock(std::uint32_t blockOffset, std::string &&piecePayload) {
        if (blockOffset >= actualPieceSize) throw std::runtime_error("Writing out of bounds");
        auto writeSize {std::min(static_cast<std::uint32_t>(outer.blockSize), actualPieceSize - blockOffset)};
        std::memcpy(buffer.data() + blockOffset, piecePayload.c_str() + 8, writeSize);
        states[blockOffset / outer.blockSize] = State::DONE;
        --requestedBlocks; ++completedBlocks;
    }

    std::uint32_t PieceManager::Piece::requestBlockNum(std::uint16_t blockIdx) {
        if (states[blockIdx] != State::PENDING) throw std::runtime_error{"Block requested twice"};
        states[blockIdx] = Piece::State::REQUESTED; ++requestedBlocks;
        return blockIdx < actualNumBlocks - 1? outer.blockSize: lastBlockSize;
    }

    PieceManager::Piece::Piece(const PieceManager &outer, const std::uint32_t pieceIdx): 
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
        Logging::Dynamic::Trace("Piece #{}, Block #{} is now marked pending", pieceIdx, blockOffset);
        Piece &piece {partialPieces.at(pieceIdx)};
        piece.states[blockOffset / blockSize] = Piece::State::PENDING;
        --piece.requestedBlocks;
        if (!piece.requestedBlocks && !piece.completedBlocks)
            partialPieces.erase(pieceIdx);
    }

    void PieceManager::onPeerReset(const std::vector<PieceBlock> &pendingRequests) {
        for (const auto &request: pendingRequests) {
            clearInTransitBlock(request.pieceIdx, request.blockOffset);
        }
    }

    std::pair<bool, std::string> PieceManager::onBlockReceived(const std::uint32_t pieceIdx, 
        const std::uint32_t blockOffset, std::string piecePayload) 
    {
        auto &partialPiece {partialPieces.at(pieceIdx)};
        partialPiece.writeBlock(blockOffset, std::move(piecePayload));

        bool pieceCompleted {false}; std::string retBuffer;
        if (partialPiece.completedBlocks == partialPiece.actualNumBlocks) {
            retBuffer = std::string {partialPiece.buffer.c_str(), partialPiece.actualPieceSize};
            if (hashutil::sha1(retBuffer, true) != getPieceHash(pieceIdx)) {
                Logging::Dynamic::Info("Hash for piece# {} is INVALID, will be rerequested; "
                    "pending {} pieces", pieceIdx, numPieces - haves.size());
                retBuffer.clear();
            } else {
                Logging::Dynamic::Info("Hash for piece# {} is VALID, will be scheduled for disk write; "
                    "pending {} pieces", pieceIdx, numPieces - haves.size());
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
                Logging::Dynamic::Trace("Partially processed piece #{} is being prioritized", pieceIdx);
                std::uint16_t blockIdx {};
                while (blockIdx < piece.actualNumBlocks && result.size() < count) {
                    if (piece.states[blockIdx] == Piece::State::PENDING) {
                        auto actualBlockSize {piece.requestBlockNum(blockIdx)};
                        result.emplace_back(pieceIdx, static_cast<std::uint32_t>(blockIdx) * blockSize, actualBlockSize);        
                    }
                    ++blockIdx;
                }
            }
        }

        // Pick the pieces that we haven't downloaded or requested yet
        for (auto peerHavesIt {peerHaves.begin()}; peerHavesIt != peerHaves.end() && result.size() < count; ++peerHavesIt) {
            const std::uint32_t pieceIdx {*peerHavesIt};
            if (!haves.contains(pieceIdx) && !partialPieces.contains(pieceIdx)) {
                Logging::Dynamic::Trace("New piece #{} is being requested", pieceIdx);
                auto inserted {partialPieces.try_emplace(pieceIdx, *this, pieceIdx)};
                Piece &piece {inserted.first->second}; std::uint16_t blockIdx {};
                while (blockIdx < piece.actualNumBlocks && result.size() < count) {
                    auto actualBlockSize {piece.requestBlockNum(blockIdx)};
                    result.emplace_back(pieceIdx, static_cast<std::uint32_t>(blockIdx) * blockSize, actualBlockSize);
                    ++blockIdx;
                }
            }
        }

        return result;
    }
}
