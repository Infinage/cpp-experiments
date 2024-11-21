#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class BloomFilter {
    private:
        using u64 = uint64_t;
        constexpr static u64 FNV_OFFSET {14695981039346656037ULL};
        constexpr static u64 FNV_PRIME {1099511628211ULL};
        constexpr static float VERSION {0.1f};

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

        // Deploy double hashing to generate 'K' hashes: (H1 + (i * H2)) % M
        std::vector<std::size_t> generateHashes(const std::string &word) {
            const u64 h1 {fnv1(word)}, h2 {fnv1a(word)};
            std::vector<std::size_t> result;
            result.reserve(static_cast<std::size_t>(K));
            for (int i {0}; i < K; i++)
                result.push_back(static_cast<std::size_t>((h1 + static_cast<u64>(i) * h2) % static_cast<u64>(M)));
            return result;
        }

    public:
        BloomFilter(int M, int K, std::vector<bool> &bitset): M(M), K(K), bitset(bitset) {}
        BloomFilter(int M, int K): M(M), K(K) { bitset = std::vector<bool>(static_cast<std::size_t>(M), false); }
        BloomFilter(std::vector<std::string> &words, float P = 0.01f) {
            double N {(double) words.size()};
            M = -static_cast<int>((N * std::log(P)) / std::pow(std::log(2), 2));
            K = static_cast<int>(((float) M / N) * std::log(2));
            bitset = std::vector<bool>(static_cast<std::size_t>(M), false);

            // Insert words into BF
            for (const std::string &word: words)
                insert(word);
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

        static void dump(BloomFilter &bf, std::string &path) {
            std::ofstream ofs{path, std::ios::binary | std::ios::trunc};
            if (!ofs.is_open()) {
                std::cout << "File couldn't be opened.\n";
                std::exit(1);
            } else {
                ofs.write("BLOOM", 5);
                ofs.write(reinterpret_cast<const char*>(&bf.VERSION), sizeof(float));
                ofs.write(reinterpret_cast<const char*>(&bf.M), sizeof(int));
                ofs.write(reinterpret_cast<const char*>(&bf.K), sizeof(int));

                // Process the bitset
                unsigned char bitbuffer {0};
                int bitsProcessed {0};
                for (std::size_t i{0}; i < (std::size_t) bf.M; i++) {
                    bitbuffer = static_cast<unsigned char>(bitbuffer << 1 | bf.bitset[i]);
                    if (++bitsProcessed == 8) {
                        ofs.write(reinterpret_cast<const char *>(&bitbuffer), 1);
                        bitsProcessed = 0;
                        bitbuffer = 0;
                    }
                }

                // If left over bits exists
                if (bitsProcessed) {
                    bitbuffer <<= 8 - bitsProcessed;
                    ofs.write(reinterpret_cast<const char *>(&bitbuffer), 1);
                }

                ofs.close();
            }
        }

        static BloomFilter load(std::string &path) {
            std::ifstream ifs{path, std::ios::binary};
            if (!ifs.is_open()) {
                std::cout << "Error reading file.\n";
                std::exit(1);
            } else {
                // Declare placeholders
                char header[5];
                float version;
                int M, K;
                unsigned char bitbuffer;
                std::vector<bool> bitset;

                // Read the input in order
                ifs.read(header, 5);
                ifs.read(reinterpret_cast<char *>(&version), sizeof(float));
                
                if (std::strcmp(header, "BLOOM") != 0 && version != BloomFilter::VERSION) {
                    std::cout << "Malformed Binary.\n";
                    std::exit(1);
                } else {
                    ifs.read(reinterpret_cast<char *>(&M), sizeof(int));
                    ifs.read(reinterpret_cast<char *>(&K), sizeof(int));

                    // Read the bitset
                    int idx{0};
                    bitset.reserve((std::size_t) M);
                    while (ifs.read(reinterpret_cast<char *>(&bitbuffer), 1)) {
                        for (int i {7}; i >= 0; i--) {
                            bitset.push_back((bitbuffer >> i) & 1);
                            if (++idx == M) break;
                        }
                    }
                    ifs.close();
                    return BloomFilter{M, K, bitset};
                }
            }
        }

        // Reads in a whitespace seperated set of words into a
        // bloom filter and dumps the filter into a binary file
        static void build(std::string &ifpath, std::string &ofpath) {
            std::ifstream ifs{ifpath};
            if (!ifs) {
                std::cout << "Error reading file.\n";
                std::exit(1);
            } else {
                // Read the file
                std::string buffer;
                std::vector<std::string> words;
                while (std::getline(ifs, buffer)) {
                    buffer.pop_back();
                    std::string word {""};
                    bool isValid {true};
                    for (const char ch: buffer) {
                        if (std::isspace(ch)) {
                            isValid = false;
                            break;
                        }
                        else if (std::isupper(ch)) 
                            word += (char) std::tolower(ch);
                        else
                            word += ch;
                    }
                    if (!buffer.empty() && isValid)
                        words.push_back(buffer);
                }
                ifs.close();

                // Insert into bloom filter and dump binary
                BloomFilter bf {words};
                BloomFilter::dump(bf, ofpath);

                // Printing out the stats
                std::cout << "Built Filter            : " << ofpath << "\n";
                std::cout << "Words processed         : " << words.size() << "\n";
                std::cout << "Optimal Bit count       : " << bf.M << "\n";
                std::cout << "Optimal Hash func count : " << bf.K << "\n";

                // Close the f pointer
                ifs.close();
            }
        }
};

int main(int argc, char **argv) {
    std::string dumpPath {"words-en.bf"};
    if (argc == 1)
        std::cout << "Spell Check using Bloom Filter. Needs to be built before it can be used.\n" 
                  << "For a suitable wordlist check: 'https://github.com/dwyl/english-words'\n\nUsage:\n" 
                  << "1. Building spellchecker: `spellcheck build <dictionary>`\n" 
                  << "2. Running spell check:   `spellcheck <file>`\n";
    else if (argc == 2) {
        if (!std::filesystem::exists(dumpPath)) {
            std::cout << "Filter not built, please build filter before using it.\n";
            std::exit(1);
        } else if (!std::filesystem::is_regular_file(argv[1])) {
            std::cout << "Not a valid input file.\n";
            std::exit(1);
        } else {
            std::string buffer;
            BloomFilter bf {BloomFilter::load(dumpPath)};
            std::ifstream ifs{argv[1]};
            std::cout << "Misspelt words:\n";
            while (ifs >> buffer) {

                std::string word {""};
                for (const char ch: buffer)
                    if (!std::ispunct(ch))
                        word += (char) std::tolower(ch);

                if (!bf.check(word))
                    std::cout << "- " << buffer << "\n";
            }
            ifs.close();
        }
    } else if (argc == 3 && std::strcmp(argv[1], "build") == 0) {
        std::string ifname {argv[2]};
        BloomFilter::build(ifname, dumpPath);
    }
}
