#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>
#include <stack>

class WordSearch {
    private:
        // Datastructure to hold list of words and the puzzle grid
        std::vector<std::vector<char>> grid;
        std::vector<std::string> words;

        // For random number generation
        std::mt19937 random_gen;
        std::uniform_int_distribution<int> randInt;
        std::uniform_int_distribution<char> randChar;

        // Trie data structure for efficient grid searches when solving
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

                // Erase word from trie - update minDist
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

        // Using coordinates with unordered_set
        struct HashPair {
            inline std::size_t operator() (const std::pair<int, int> &p) const {
                return (std::size_t) ((31 * p.first) + p.second);
            }
        };

        // Candidates are potentials grid coordinates a word could get inserted into
        struct Candidate {
            int overlaps;
            int rand;
            int row; int col;
            int dirX; int dirY;
        };

        // Ordering the potential candidates during grid generation
        struct OrderCandidate {
            inline bool operator() (const Candidate &c1, const Candidate &c2) const {
                return c1.overlaps < c2.overlaps || (c1.overlaps == c2.overlaps && c1.rand > c2.rand);
            }
        };

        // Traversing along 8 directions on the grid
        constexpr static std::array<std::pair<int, int>, 8> dirs {{
            {-1, -1}, {-1, 0}, {-1, 1},
            { 0, -1},          { 0, 1},
            { 1, -1}, { 1, 0}, { 1, 1}
        }};

        // Helper to check if given coordinates are valid
        bool isValid(int i, int j) {
            return 0 <= i && 0 <= j && i < (int)grid.size() && j < (int)grid[0].size();
        }

        // Max dist we can go along a direction given row and col
        // This is helpful to prune unneccessary traversals when word size exceeds available space
        std::size_t maxDist(int x, int y, int dx, int dy) {
            int bx {dx == 0? std::numeric_limits<int>::max(): dx < 0? 0: (int)grid.size()};
            int by {dy == 0? std::numeric_limits<int>::max(): dy < 0? 0: (int)grid[0].size()};
            return (std::size_t) std::min(std::abs(x - bx), std::abs(y - by)) + 1;
        }

        // Helper to init empty board given rows and cols
        void initBoard(std::size_t rows, std::size_t cols) {
            this->grid = std::vector<std::vector<char>>(rows, std::vector<char>(cols, '*'));
        }

        // Given a word, produces a sorted (by OrderCandidate) list
        // of positions it could be inserted into
        std::vector<Candidate> generateCandidates(std::string &word) {
            std::vector<Candidate> result;
            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++) {
                    for (auto [dx, dy]: dirs) {
                        // Overlaps is one criteria by which we pick candidates
                        // How many chars overlap with this candidate?
                        int x {(int)i}, y {(int)j}, overlaps {0};
                        std::size_t idx {0};
                        bool candidateValid {isValid(x, y) && word.size() < maxDist(x, y, dx, dy)};
                        while (idx < word.size() && isValid(x, y) && candidateValid) {
                            char gridCh {grid[(std::size_t)x][(std::size_t)y]}, ch {word[idx++]};
                            if (gridCh != '*' && gridCh != ch) {
                                candidateValid = false;
                                break;
                            } else if (gridCh != '*') overlaps++;
                            x += dx; y += dy;
                        }
                        // Random int is used as a tie breaker
                        if (candidateValid)
                            result.push_back({overlaps, randInt(random_gen), (int)i, (int)j, dx, dy});
                    }
                }
            }
            std::sort(result.begin(), result.end(), OrderCandidate());
            return result;
        }

        /*
         * Pick a random word from the available pool of words
         * Generate candidates for all words, ordered by criteria*
         * Iterate through all candidates
         *  - Insert candidate and recurse to check if we can reach the solution
         *  - If successful, return
         *  - Else remove inserted word and try the next canidate
         * If all combinations are exhausted for word, backtrack
         */
        bool backtrackGenerate(std::vector<std::string> &words, int threshold) {
            if (words.empty()) return true;
            else {
                std::string word{words.back()};
                words.pop_back();

                // Only allow maximum threshold number of candidates
                // We can do better by maintaining a priority queue of size `threshold`
                std::vector<Candidate> allCandidates {generateCandidates(word)}, candidates;
                candidates = std::vector<Candidate>{allCandidates.begin(), allCandidates.begin() + std::min(threshold, (int) allCandidates.size())};

                for (Candidate &candidate: candidates) {
                    // Extract fields
                    int row {candidate.row}, col {candidate.col};
                    int dx {candidate.dirX}, dy {candidate.dirY};

                    // Insert word into grid
                    std::unordered_set<int> overlaps;
                    for (int idx {0}; idx < (int)word.size(); idx++) {
                        int x {row + (idx * dx)}, y {col + (idx * dy)};
                        if (grid[(std::size_t)x][(std::size_t)y] == word[(std::size_t)idx])
                            overlaps.insert(idx);
                        grid[(std::size_t)x][(std::size_t)y] = word[(std::size_t)idx];
                    }

                    // Does this config produce a valid grid?
                    if (backtrackGenerate(words, threshold))
                        return true;

                    // Remove inserted word
                    for (int idx {0}; idx < (int) word.size(); idx++) {
                        int x {row + (idx * dx)}, y {col + (idx * dy)};
                        if (overlaps.find(idx) == overlaps.end())
                            grid[(std::size_t)x][(std::size_t)y] = '*';
                    }
                }
                // Insert word back and backtrack
                words.push_back(word);
                return false;
            }
        }

    public:
        // Constructor for solver
        WordSearch(std::vector<std::vector<char>> &grid, std::vector<std::string> &words): grid(grid), words(words) {}

        // Constructor for puzzle generator - Defer Grid initialization to `generate` function
        WordSearch(std::vector<std::string> &words): words(words) {
            // For random shuffling during puzzle generation
            random_gen = std::mt19937{std::random_device{}()};
            randInt = std::uniform_int_distribution<int>(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            randChar = std::uniform_int_distribution<char>('A', 'Z');
        }

        // Solve puzzle using stack
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
                            // Stop early by checking if any of the available words would fit in the direction
                            if (curr->next[Trie::ord(grid[(std::size_t) x][(std::size_t) y])]->minDist > maxDist(x, y, dx, dy))
                                break;

                            curr = curr->next[Trie::ord(grid[(std::size_t) x][(std::size_t) y])].get();
                            path.push_back({x, y}); 
                            x += dx; y += dy;

                            // If end of a word is reached, inserted into solution but continue iterating
                            if (curr->end) {
                                std::string acc{""};
                                for (const std::pair<int, int> &p: path) {
                                    visited.insert({p.first, p.second});
                                    acc += grid[(std::size_t)p.first][(std::size_t)p.second];
                                }
                                found.insert(acc);
                                Trie::erase(root, acc);
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
            // Find avg length of words
            std::size_t avgLength {0};
            for (const std::string &word: words)
                avgLength += word.size();
            avgLength /= words.size();

            // Compute grid dimensions
            std::uniform_int_distribution<std::size_t> randDims{std::uniform_int_distribution<std::size_t>(
                (std::size_t)(std::ceil(std::sqrt(words.size() * avgLength * (1 + 0.05)))),
                (std::size_t)(std::ceil(std::sqrt(words.size() * avgLength * (1 + 0.25))))
            )};

            // Distribution to decide random increments
            std::uniform_int_distribution<int> randChance(0, 1);

            // Randomly pick starting dimensions
            std::size_t rows {randDims(random_gen)}, cols {randDims(random_gen)};
            bool status;

            do {
                // Create empty grid with random dimensions
                initBoard(rows, cols);

                // Increment rows / cols by chance on fail
                if (randChance(random_gen)) rows++;
                if (randChance(random_gen)) cols++;

                // Shuffle and try to generate grid
                std::shuffle(words.begin(), words.end(), random_gen);
                status = backtrackGenerate(words, 1);
            } while (!status);

            // Fill missing positions with random chars
            for (std::size_t i{0}; i < grid.size(); i++)
                for (std::size_t j{0}; j < grid[0].size(); j++)
                    if (grid[i][j] == '*')
                        grid[i][j] = randChar(random_gen);
        }

        // Helper to pretty print the puzzle board
        void print() {
            std::ostringstream oss;
            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++)
                    oss << grid[i][j] << " ";
                oss << "\n";
            }
            std::cout << oss.str();
        }

        // Helper to read a space / newline seperated list of words
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

        // Helper to read the puzzle board
        // Each row must be seperated by a new line
        // Each char may be optionally space seperated (spaces are ignored)
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
