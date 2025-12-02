#pragma once

#include "../include/common.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Torrent {
    class PieceManager {
        private:
            struct Piece {
                enum class State { PENDING, REQUESTED, DONE };

                const PieceManager &outer;
                std::string buffer;
                const std::uint32_t actualPieceSize;
                const std::uint16_t lastBlockSize;
                const std::uint16_t actualNumBlocks;
                std::vector<State> states;

                // Track the current status count
                std::uint16_t requestedBlocks {}, completedBlocks {};

                // Marks the block as requested and returns the block size
                std::uint32_t requestBlockNum(std::uint16_t blockOffset);

                bool finished() const;
                void writeBlock(std::uint32_t blockOffset, std::string payload);
                Piece(const PieceManager &outer, const std::uint32_t pieceIdx);
            };

        private:
            // User defined constants
            const std::uint64_t totalSize;
            const std::uint32_t pieceSize;
            const std::uint16_t blockSize;
            const std::uint32_t numPieces;
            const std::uint16_t numBlocks;
            const std::string &pieceBlob;

            // Pieces that we have completed downloading
            std::unordered_set<std::uint32_t> haves;

            // Blocks in transit and received are accumulated 
            // here until they can be written to disk
            std::unordered_map<std::uint32_t, Piece> partialPieces;

        private:
            void clearInTransitBlock(std::uint32_t pieceIdx, std::uint32_t blockOffset);
            std::string_view getPieceHash(std::size_t idx) const;

        public:
            [[nodiscard]] inline decltype(auto) getHaves(this auto &self) { return (self.haves); }

            PieceManager(const std::uint64_t totalSize, const std::uint32_t pieceSize, 
                const std::uint16_t blockSize, const std::string &pieceBlob);

            bool finished() const;

            void onPeerReset(const std::vector<PieceBlock> &pendingRequests);

            // Input payload from peer request must be passed as it is
            // Returns a pair denoting if we are good to schedule writing to disk
            std::pair<bool, std::string> onBlockReceived(const std::uint32_t pieceIdx, 
                const std::uint32_t blockOffset, std::string piecePayload);

            // Non const since we will update the partialPieces state for the requested blocks
            std::vector<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t>> 
            getPendingBlocks(const std::unordered_set<std::uint32_t> &peerHaves, std::uint8_t count);
    };
}
