#pragma once

#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Encapsulates logic to handle `.git/objects/pack/*.idx` and `.git/objects/pack/*.pack`
class GitPack {
    private:
        // Store all the pack indices & files from provided path
       std::vector<fs::path> indexPaths, packPaths;

       // Helper to verify .idx / .pack header
       static bool verifyHeader(std::ifstream &ifs, const std::string &expectedHeader, unsigned int expectedVersion);

        // Binary search to find *first* offset from the .idx file where `part` starts at
        unsigned int getPackIdxOffsetStart(unsigned int start, unsigned int end, 
                const std::string &part, std::ifstream &ifs) const;

        // Returns a vector of resolved hashes along with their offsets
        std::vector<std::pair<std::string, unsigned int>> getHashMatchFromIndex(const std::string &part, const fs::path &path) const;

        // Given sha1 hex string (40 char long), use .idx file to return {PackfilePath, offset}
        std::pair<fs::path, unsigned long> getPackFileOffset(const std::string &objectHash) const;

        // Read variable length int from a string_view, used during delta chain construction
        // Continues reading while the most significant bit (MSB) is set.
        // Each byte contributes 7 bits to result, with the MSB acting as a continuation flag.
        // The input string_view is sliced to remove the consumed bytes.
        static std::size_t readVarLenInt(std::basic_string_view<unsigned char> &sv);

    public:
        // Given '.git/objects/pack' path, pick all the '.idx' and '.pack' files
        GitPack (const fs::path &path);

        // Analogous to refResolve of `GitRepository` but returns a vector of matches instead of just one
        [[nodiscard]] std::vector<std::string> refResolve(const std::string &part) const;

        // Extracts and returns the serialized content of an object given its hash.
        // Auto resolves and constructs delta chains, ensuring the returned string 
        // is fully reconstructed and ready for consumption.
        [[nodiscard]] std::string extract(const std::string &objectHash) const;
};
