#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

class BloomFilter {
    private:

        using u64 = uint64_t;
        constexpr static u64 FNV_OFFSET {14695981039346656037ULL};
        constexpr static u64 FNV_PRIME {1099511628211ULL};

        int M, K;
        std::vector<bool> bitset;

        u64 fnv1(const std::string &word) {
           u64 hash {FNV_OFFSET}; 
           for (const char ch: word) {
                hash *= FNV_PRIME;
                hash ^= static_cast<u64>(ch);
           }
           return hash;
        }

        u64 fnv1a(const std::string &word) {
           u64 hash {FNV_OFFSET}; 
           for (const char ch: word) {
                hash ^= static_cast<u64>(ch);
                hash *= FNV_PRIME;
           }
           return hash;
        }

        std::vector<std::size_t> generateHashes(const std::string &word) {
            const u64 h1 {fnv1(word)}, h2 {fnv1a(word)};
            std::vector<std::size_t> result;
            result.reserve(static_cast<std::size_t>(K));
            for (int i {0}; i < K; i++)
                result.push_back(static_cast<std::size_t>((h1 + static_cast<u64>(i) * h2) % static_cast<u64>(M)));
            return result;
        }

    public:
        BloomFilter(int M, int K): M(M), K(K) {
            bitset = std::vector<bool>(static_cast<std::size_t>(M), false);
        }

        void insert (const std::string &word) {
            std::vector<std::size_t> positions {generateHashes(word)};
            for (const std::size_t pos: positions)
                bitset[pos] = true;
        }

        bool check(const std::string &word) {
            std::vector<std::size_t> positions {generateHashes(word)};
            bool present {true};
            for (const std::size_t pos: positions) {
                if (!bitset[pos]) {
                    present = false;
                    break;
                }
            }
            return present;
        }
};

int main() {
    BloomFilter bf(64, 3);

    std::vector<std::string> wordsPresent {{
        "abound","abounds","abundance","abundant","accessible", "bloom","blossom",
        "bolster","bonny","bonus","bonuses","coherent","cohesive","colorful","comely",
        "comfort","gems","generosity","generous","generously","genial"
    }};

    std::vector<std::string> wordsAbsent {{
        "bluff","cheater","hate","war","humanity",
        "racism","hurt","nuke","gloomy","facebook",
        "geeksforgeeks","twitter"
    }};

    for (const std::string &word: wordsPresent)
        bf.insert(word);

    for (const std::string &word: wordsAbsent) {
        std::cout << word << ": ";
        std::cout << (bf.check(word)? "Present": "Absent") << "\n";
    }
}
