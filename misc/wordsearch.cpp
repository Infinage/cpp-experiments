#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_set>
#include <vector>

class WordSearch {
    private:
        // Datastructure to hold list of words and the puzzle grid
        std::vector<std::vector<char>> grid;
        std::vector<std::string> words;

        // Overlaps, Rand int, row, col, dirx, diry
        using GENERATE_CANDIDATE = std::tuple<int, int, int, int, int, int>;

        // For random number generation
        std::mt19937 random_gen;
        std::uniform_int_distribution<int> randInt;
        std::uniform_int_distribution<char> randChar;

        class Trie {
            public:
                bool end; 
                std::size_t minDist;
                std::array<std::unique_ptr<Trie>, 26> next;
                Trie(): end(false), minDist(std::numeric_limits<int>::max()) {}

                static std::size_t ord(const char ch) {
                    return (std::size_t) ch - 'A';
                }

                static std::unique_ptr<Trie> init(std::vector<std::string> &words) {
                    std::unique_ptr<Trie> root = std::make_unique<Trie>();
                    for (const std::string &word: words)
                        insert(root, word);
                    return root;
                }

                static void insert(std::unique_ptr<Trie> &root, const std::string &word) {
                    Trie *curr = root.get();
                    for (std::size_t idx{0}; idx < word.size(); idx++) {
                        const char ch {word[idx]};
                        if (curr->next[ord(ch)] == nullptr)
                            curr->next[ord(ch)] = std::make_unique<Trie>();
                        curr->minDist = std::min(curr->minDist, (word.size() - idx));
                        curr = curr->next[ord(ch)].get();
                    }
                    curr->end = true;
                    curr->minDist = 0;
                }

                static bool erase(std::unique_ptr<Trie> &root, const std::string &word) {
                    Trie *curr = root.get(); 
                    std::stack<std::pair<Trie*, char>> stk;
                    for (const char ch: word) {
                        stk.push({curr, ch});
                        curr = curr->next[ord(ch)].get();
                        if (curr == nullptr)
                            return false;
                    }

                    if (!curr->end)
                        return false;
                    else {
                        // Mark end of node as false to 'del' the word
                        curr->end = false;

                        // Backtrack and delete nodes if it has no child
                        // Continue to update the minDist until root
                        while (!stk.empty()) {
                            bool allChildEmpty {true};
                            std::size_t minDist {std::numeric_limits<int>::max()};
                            for (std::unique_ptr<Trie> &node: stk.top().first->next) {
                                if (node != nullptr) {
                                    allChildEmpty = false;
                                    minDist = std::min(minDist, node->minDist);
                                }
                            }

                            if (allChildEmpty)
                                stk.top().first->next[ord(stk.top().second)].reset();
                            else
                                stk.top().first->minDist = minDist;

                            stk.pop();
                        }
                        return true;
                    }
                }
        };

        struct HashPair {
            inline std::size_t operator() (const std::pair<int, int> &p) const {
                return (std::size_t) ((31 * p.first) + p.second);
            }
        };

        struct OrderCandidate {
            inline bool operator() (const GENERATE_CANDIDATE &c1, const GENERATE_CANDIDATE &c2) const {
                return std::get<0>(c1) < std::get<0>(c2) || (std::get<0>(c1) == std::get<0>(c2) && std::get<1>(c1) > std::get<1>(c2));
            }
        };

        constexpr static std::array<std::pair<int, int>, 8> dirs {{
            {-1, -1}, {-1, 0}, {-1, 1},
            { 0, -1},          { 0, 1},
            { 1, -1}, { 1, 0}, { 1, 1}
        }};

        bool isValid(int i, int j) {
            return 0 <= i && 0 <= j && i < (int)grid.size() && j < (int)grid[0].size();
        }

        std::size_t maxDist(int x, int y, int dx, int dy) {
            int bx {dx == 0? std::numeric_limits<int>::max(): dx < 0? 0: (int)grid.size()};
            int by {dy == 0? std::numeric_limits<int>::max(): dy < 0? 0: (int)grid[0].size()};
            return (std::size_t) std::min(std::abs(x - bx), std::abs(y - by)) + 1;
        }

        std::vector<GENERATE_CANDIDATE> generateCandidates(std::string &word) {
            // Iterate through all cells, in all directions
            std::vector<GENERATE_CANDIDATE> result;
            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++) {
                    for (auto [dx, dy]: dirs) {
                        std::size_t idx {0};
                        int x {(int)i}, y {(int)j}, overlaps {0};
                        bool candidateValid {isValid(x, y) && word.size() < maxDist(x, y, dx, dy)};
                        while (idx < word.size() && isValid(x, y) && candidateValid) {
                            char gridCh {grid[(std::size_t)x][(std::size_t)y]}, ch {word[idx++]};
                            if (gridCh != '*' && gridCh != ch) {
                                candidateValid = false;
                                break;
                            } else if (gridCh != '*') overlaps++;
                            x += dx; y += dy;
                        }
                        if (candidateValid)
                            result.push_back({overlaps, randInt(random_gen), i, j, dx, dy});
                    }
                }
            }
            std::sort(result.begin(), result.end(), OrderCandidate());
            return result;
        }

        /*
         * Pick a random word from the available pool of words
         * Generate candidates for all words
         * Iterate through all available candidates and try inserting them to see if they provide a valid grid
         * If all combinations exhausted backtrack
         */
        bool backtrackGenerate(std::vector<std::string> &words, int &backtrackThresh) {
            if (words.empty()) return true;
            else {
                std::string word{words.back()};
                words.pop_back();
                std::vector<GENERATE_CANDIDATE> candidates {generateCandidates(word)};
                for (GENERATE_CANDIDATE &candidate: candidates) {
                    // Extract fields
                    int row {std::get<2>(candidate)}, col {std::get<3>(candidate)};
                    int dx {std::get<4>(candidate)}, dy {std::get<5>(candidate)};

                    // Insert word into grid
                    std::unordered_set<int> overlaps;
                    for (int idx {0}; idx < (int)word.size(); idx++) {
                        int x {row + (idx * dx)}, y {col + (idx * dy)};
                        if (grid[(std::size_t)x][(std::size_t)y] == word[(std::size_t)idx])
                            overlaps.insert(idx);
                        grid[(std::size_t)x][(std::size_t)y] = word[(std::size_t)idx];
                    }

                    if (backtrackGenerate(words, backtrackThresh))
                        return true;

                    // Backtrack - remove inserted word
                    for (int idx {0}; idx < (int) word.size(); idx++) {
                        int x {row + (idx * dx)}, y {col + (idx * dy)};
                        if (overlaps.find(idx) == overlaps.end())
                            grid[(std::size_t)x][(std::size_t)y] = '*';
                    }

                    // One less backtrack allowed
                    if (--backtrackThresh <= 0)
                        break;
                }
                words.push_back(word);
                return false;
            }
        }

    public:
        // Constructor for solver
        WordSearch(std::vector<std::vector<char>> &grid, std::vector<std::string> &words): grid(grid), words(words) {}

        // Constructor for puzzle generator
        WordSearch(std::vector<std::string> &words): words(words) {
            // Find max length of words
            std::size_t maxLength {0};
            for (const std::string &word: words)
                maxLength = std::max(maxLength, word.size());

            // Compute grid dimensions
            std::uniform_int_distribution<int> randDims{std::uniform_int_distribution<int>(
                (int)(std::max(words.size() * .5, maxLength * 1.) * 1.45),
                (int)(std::max(words.size() * .5, maxLength * 1.) * 1.85)
            )};

            // For random shuffling during puzzle generation
            random_gen = std::mt19937{std::random_device{}()};
            randInt = std::uniform_int_distribution<int>(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            randChar = std::uniform_int_distribution<char>('A', 'Z');

            // Create empty grid with random dimensions
            this->grid = std::vector<std::vector<char>>((std::size_t) randDims(random_gen), std::vector<char>((std::size_t) randDims(random_gen), '*'));
        }

        std::unordered_set<std::string> solve() {
            // Init a Trie object for efficient char matching
            std::unique_ptr<Trie> root {Trie::init(words)}; 

            // Do a DFS Traversal for each cell
            std::unordered_set<std::pair<int, int>, HashPair> visited;
            std::unordered_set<std::string> found;
            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++) {
                    for (auto [dx, dy]: dirs) {
                        int x {(int)i}, y {(int)j};
                        Trie* curr = root.get();
                        std::vector<std::pair<int, int>> path;
                        while (isValid(x, y) && curr->next[Trie::ord(grid[(std::size_t) x][(std::size_t) y])] != nullptr) {
                            // Stop early by checking if the available words would fit in the direction
                            if (curr->next[Trie::ord(grid[(std::size_t) x][(std::size_t) y])]->minDist > maxDist(x, y, dx, dy))
                                break;

                            curr = curr->next[Trie::ord(grid[(std::size_t) x][(std::size_t) y])].get();
                            path.push_back({x, y}); 
                            x += dx; y += dy;
                            if (curr->end) {
                                std::string acc{""};
                                for (const std::pair<int, int> &p: path) {
                                    visited.insert({p.first, p.second});
                                    acc += grid[(std::size_t)p.first][(std::size_t)p.second];
                                }
                                found.insert(acc);
                            }
                        }
                    }
                }
            }

            // Mark unvisited cells with a '*'
            for (std::size_t i{0}; i < grid.size(); i++)
                for (std::size_t j{0}; j < grid[0].size(); j++)
                    if (visited.find({(int) i, (int) j}) == visited.end())
                        grid[i][j] = '*';

            return found;
        }

        void generate() {
            bool status;
            do {
                // Restart after threshold num of steps
                int cumulativeThresh {10};
                std::shuffle(words.begin(), words.end(), random_gen);
                status = backtrackGenerate(words, cumulativeThresh);
            } while (!status);

            // Fill missing positions with random chars
            for (std::size_t i{0}; i < grid.size(); i++)
                for (std::size_t j{0}; j < grid[0].size(); j++)
                    if (grid[i][j] == '*')
                        grid[i][j] = randChar(random_gen);
        }

        void print() {
            std::ostringstream oss;
            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++)
                    oss << grid[i][j] << " ";
                oss << "\n";
            }
            std::cout << oss.str();
        }

        static std::vector<std::string> readWordList(std::string &&fname) {
            std::vector<std::string> result;
            if (std::filesystem::is_regular_file(fname)) {
                std::string word;
                std::ifstream ifs{fname};
                while (ifs >> word)
                    result.push_back(word);
            }
            return result;
        }

        static std::vector<std::vector<char>> readGrid(std::string &&fname) {
            std::vector<std::vector<char>> result;
            if (std::filesystem::is_regular_file(fname)) {
                std::string sentence;
                std::ifstream ifs{fname};
                while (std::getline(ifs, sentence)) {
                    result.push_back(std::vector<char>{});
                    for (const char ch: sentence)
                        if (!std::isspace(ch))
                            result.back().push_back(ch);
                    if (result.back().size() != result[0].size()) {
                        std::cout << "Error: Invalid Grid.\n";
                        std::exit(1);
                    }
                }
            }
            return result;
        }
};

int main(int argc, char **argv) {
    if (argc == 2) {
        std::vector<std::string> wordList{WordSearch::readWordList(std::string{argv[1]})};
        WordSearch ws{wordList};
        ws.generate();
        ws.print();
    } else if (argc == 3) {
        std::vector<std::string> wordList{WordSearch::readWordList(std::string{argv[1]})};
        std::vector<std::vector<char>> grid{WordSearch::readGrid(std::string{argv[2]})};
        WordSearch ws{grid, wordList};
        std::unordered_set<std::string> found{ws.solve()};
        std::cout << found.size() << "F, " << wordList.size() - found.size() << "NF\n\n";
        ws.print();
    } else {
        std::cout << "Usage:\n"
                  << "  1. Generate word search puzzle:\n"
                  << "     ./wordsearch <wordlist>\n\n"
                  << "  2. Solve word search puzzle:\n"
                  << "     ./wordsearch <wordlist> <grid>\n";
    }
    return 0;
}
