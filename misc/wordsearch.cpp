#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
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

        // Overlaps, Rand int, word, row, col, dirx, diry
        using GENERATE_CANDIDATE = std::tuple<int, int, std::string, int, int, int, int>;

        // For random number generation
        std::mt19937 random_gen;
        std::uniform_int_distribution<int> randInt;
        std::uniform_int_distribution<char> randChar;

        class Trie {
            public:
                bool end; 
                std::array<std::unique_ptr<Trie>, 26> next;
                Trie(): end(false) {}

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
                    for (const char ch: word) {
                        if (curr->next[ord(ch)] == nullptr)
                            curr->next[ord(ch)] = std::make_unique<Trie>();
                        curr = curr->next[ord(ch)].get();
                    }
                    curr->end = true;
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

                        // Delete nodes if it has no child
                        while (!stk.empty() && std::all_of(curr->next.begin(), curr->next.end(), [](std::unique_ptr<Trie> &node) { return node == nullptr; })) {
                            stk.top().first->next[ord(stk.top().second)].reset();
                            curr = stk.top().first;
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
            inline bool operator() (const GENERATE_CANDIDATE &c1, const GENERATE_CANDIDATE &c2) {
                return std::get<0>(c1) < std::get<0>(c2) || (std::get<0>(c1) == std::get<0>(c2) && std::get<1>(c1) < std::get<1>(c2));
            }
        };

        constexpr static std::vector<std::pair<int, int>> dirs() {
            std::vector<std::pair<int, int>> result;
            for (int i {-1}; i <= 1; i++)
                for (int j{-1}; j <= 1; j++)
                    if (i != 0 || j != 0)
                        result.push_back({i, j});
            return result;
        }

        bool isValid(int i, int j) {
            return 0 <= i && 0 <= j && i < (int)grid.size() && j < (int)grid[0].size();
        }

        std::priority_queue<GENERATE_CANDIDATE, std::vector<GENERATE_CANDIDATE>, OrderCandidate> generateCandidates(
                std::unique_ptr<Trie> &root, std::unordered_set<std::string> &wordSet
            ) {

            std::priority_queue<GENERATE_CANDIDATE, std::vector<GENERATE_CANDIDATE>, OrderCandidate> result;
            std::unordered_set<std::string> inserted;

            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++) {
                    for (auto [dx, dy]: dirs()) {
                        int x {(int)i}, y {(int)j};
                        std::vector<std::tuple<Trie*, std::string, int>> candidates{{root.get(), "", 0}}, next;
                        while (isValid(x, y) && !candidates.empty()) {
                            next.clear();
                            for (const auto [node, acc, overlaps]: candidates) {
                                for (char ch = 'A'; ch <= 'Z'; ch++) {
                                    char gridCh {grid[(std::size_t)x][(std::size_t)y]};
                                    Trie *nextNode {node->next[Trie::ord(ch)].get()};
                                    if ((gridCh == '*' || gridCh == ch) && nextNode != nullptr) {
                                        next.push_back({nextNode, acc + ch, gridCh == '*'? overlaps: overlaps + 1}); 
                                        if (nextNode->end) {
                                            inserted.insert(acc + ch);
                                            result.push({overlaps, randInt(random_gen), acc + ch, i, j, dx, dy});
                                        }
                                    }
                                }
                            }
                            candidates = next;
                            x += dx; y += dy;
                        }
                    }
                }
            }

            if (inserted.size() != wordSet.size()) return {};
            else return result;
        }

        /*
         * While we have words left to insert repeat
         *      - Check suitable candidates for all remaining words 
         *          - use trie, iterate through grid cells, iterate through all 8 directions while trie node is not null
         *          - If trie.end is true, insert into a priority queue <characters overlap count, random int, word, insert row, insert col, direction>
         *      - If at any point a word has no candidates, we backtrack
         *      - Pick per priority, insert into grid and repeat loop, remove picked element
         *      - If all of the candidates are exhausted backtrack (we can improve it further to check if all candidates of a word are exhausted backtrack)
         */
        bool backtrackGenerate(std::unique_ptr<Trie> &root, std::unordered_set<std::string> &wordSet) {
            if (wordSet.empty()) return true;
            else {
                std::priority_queue<GENERATE_CANDIDATE, std::vector<GENERATE_CANDIDATE>, OrderCandidate> pq{generateCandidates(root, wordSet)};
                while(!pq.empty()) {
                    auto [overlapCount, randn, word, row, col, dx, dy] = pq.top();
                    pq.pop();
                    // Insert into grid, keep track of characters that were already inserted
                    // These chars would be skipped during removal in case of backtracking
                    std::unordered_set<int> overlaps;
                    wordSet.erase(word);
                    Trie::erase(root, word);
                    for (int idx {0}; idx < (int)word.size(); idx++) {
                        int x {row + (idx * dx)}, y {col + (idx * dy)};
                        if (grid[(std::size_t)x][(std::size_t)y] == word[(std::size_t)idx])
                            overlaps.insert(idx);
                        grid[(std::size_t)x][(std::size_t)y] = word[(std::size_t)idx];
                    }

                    if (backtrackGenerate(root, wordSet))
                        return true;

                    // Reinsert the word and remove from grid
                    Trie::insert(root, word);
                    wordSet.insert(word);
                    for (int idx {0}; idx < (int)word.size(); idx++) {
                        int x {row + (idx * dx)}, y {col + (idx * dy)};
                        if (overlaps.find(idx) == overlaps.end())
                            grid[(std::size_t)x][(std::size_t)y] = '*';
                    }
                }
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
                    (int)std::ceil(std::sqrt((double) (words.size() * maxLength) * (1 + 0.10))),
                    (int)std::ceil(std::sqrt((double) (words.size() * maxLength) * (1 + 0.25)))
            )};

            // Create empty grid with random dimensions
            this->grid = std::vector<std::vector<char>>((std::size_t) randDims(random_gen), std::vector<char>((std::size_t) randDims(random_gen), '*'));

            // For random shuffling during puzzle generation
            random_gen = std::mt19937{std::random_device{}()};
            randInt = std::uniform_int_distribution<int>(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
            randChar = std::uniform_int_distribution<char>('A', 'Z');
        }

        std::unordered_set<std::string> solve() {
            // Init a Trie object for efficient char matching
            std::unique_ptr<Trie> root {Trie::init(words)}; 

            // Do a DFS Traversal for each cell
            std::unordered_set<std::pair<int, int>, HashPair> visited;
            std::unordered_set<std::string> found;
            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++) {
                    for (auto [dx, dy]: dirs()) {
                        int x {(int)i}, y {(int)j};
                        Trie* curr = root.get();
                        std::vector<std::pair<int, int>> path;
                        while (isValid(x, y) && curr->next[Trie::ord(grid[(std::size_t) x][(std::size_t) y])] != nullptr) {
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
            std::unique_ptr<Trie> root{Trie::init(words)}; 
            std::unordered_set<std::string> pending {words.begin(), words.end()};
            backtrackGenerate(root, pending);

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
