#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <random>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class WordSearch {
    private:
        // Datastructure to hold list of words and the puzzle grid
        std::vector<std::vector<char>> grid;
        std::vector<std::string> words;

        // Overlaps, solutionCount, Rand int, word, row, col, dirx, diry
        using GENERATE_CANDIDATE = std::tuple<int, int, int, std::string, int, int, int, int>;

        // For random number generation
        std::mt19937 random_gen;
        std::uniform_int_distribution<int> randInt;
        std::uniform_int_distribution<char> randChar;

        class Trie {
            public:
                bool end; 
                int minDist;
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
                        curr->minDist = std::min(curr->minDist, (int) (word.size() - idx));
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
                            int minDist {std::numeric_limits<int>::max()};
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
                return std::get<0>(c1) < std::get<0>(c2)
                        || (std::get<0>(c1) == std::get<0>(c2) && std::get<1>(c1) > std::get<1>(c2))
                        || (std::get<0>(c1) == std::get<0>(c2) && std::get<1>(c1) == std::get<1>(c2) && std::get<2>(c1) < std::get<2>(c2));
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

        int maxDist(int x, int y, int dx, int dy) {
            int bx {dx == 0? std::numeric_limits<int>::max(): dx < 0? 0: (int)grid.size()};
            int by {dy == 0? std::numeric_limits<int>::max(): dy < 0? 0: (int)grid[0].size()};
            return std::min(std::abs(x - bx), std::abs(y - by)) + 1;
        }

        using CANDIDATE_PQ = std::priority_queue<GENERATE_CANDIDATE, std::vector<GENERATE_CANDIDATE>, OrderCandidate>;
        using CANDIDATE_VEC = std::vector<GENERATE_CANDIDATE>;

        CANDIDATE_PQ generateCandidates(
                std::unique_ptr<Trie> &root, std::unordered_set<std::string> &wordSet,
                std::unordered_map<std::string, int>& counts
            ) {

            // Wrap the cell processing logic into a function for parallelization
            std::function<CANDIDATE_VEC(std::size_t, std::size_t)> processCell{
                [this, &root](std::size_t i, std::size_t j) {
                    // Store the viable candidates
                    CANDIDATE_VEC localResult;
                    for (auto [dx, dy]: dirs) {
                        int x {(int)i}, y {(int)j};
                        std::vector<std::tuple<Trie*, std::string, int>> candidates{{root.get(), "", 0}}, next;
                        while (isValid(x, y) && !candidates.empty()) {
                            next.clear();
                            char gridCh {grid[(std::size_t)x][(std::size_t)y]};
                            for (auto [node, acc, overlaps]: candidates) {
                                for (char ch = 'A'; ch <= 'Z'; ch++) {
                                    Trie *nextNode {node->next[Trie::ord(ch)].get()};
                                    if ((gridCh == '*' || gridCh == ch) && nextNode != nullptr && node->minDist <= maxDist(x, y, dx, dy)) {
                                        next.push_back({nextNode, acc + ch, gridCh == '*'? overlaps: overlaps + 1}); 
                                        if (nextNode->end)
                                            localResult.push_back({overlaps, 0, randInt(random_gen), acc + ch, i, j, dx, dy});
                                    }
                                }
                            }
                            candidates = next;
                            x += dx; y += dy;
                        }
                    }
                    return localResult;
                }
            };

            // Create slots to ensure we don't create too many threads
            std::counting_semaphore task_slots(std::thread::hardware_concurrency());

            // Iterate through all cells and in all directions
            std::vector<std::future<CANDIDATE_VEC>> futures;
            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++) {
                    task_slots.acquire();  // Acquire a slot from the semaphore before launching a task
                    futures.push_back(std::async(std::launch::async, [i, j, &processCell, &task_slots]() {
                        CANDIDATE_VEC localResult = processCell(i, j);
                        task_slots.release();
                        return localResult;
                    }));
                }
            }

            // Wait for threads to finish execution and store the results
            CANDIDATE_VEC result;
            for (std::future<CANDIDATE_VEC>& ft: futures) {
                CANDIDATE_VEC vec = ft.get();
                while (!vec.empty()) {
                    GENERATE_CANDIDATE candidate{vec.back()};
                    vec.pop_back();
                    result.push_back(candidate);
                    counts[std::get<3>(candidate)]++;
                }
            }

            // Assign solution count for each candidate
            for (GENERATE_CANDIDATE &c: result)
                std::get<1>(c) = counts[std::get<3>(c)];

            // Check that we have non overlapping candidates for all words
            if (counts.size() != wordSet.size()) return {};
            else return CANDIDATE_PQ{result.begin(), result.end()};
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
        bool backtrackGenerate(std::unique_ptr<Trie> &root, std::unordered_set<std::string> &wordSet, int &backtrackThresh) {
            if (wordSet.empty()) return true;
            else {
                std::unordered_map<std::string, int> counts;
                CANDIDATE_PQ pq{generateCandidates(root, wordSet, counts)};
                while(!pq.empty()) {
                    auto [overlapCount, solCount, randn, word, row, col, dx, dy] = pq.top();
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

                    if (backtrackGenerate(root, wordSet, backtrackThresh))
                        return true;

                    // Reinsert the word and remove from grid
                    Trie::insert(root, word);
                    wordSet.insert(word);
                    for (int idx {0}; idx < (int) word.size(); idx++) {
                        int x {row + (idx * dx)}, y {col + (idx * dy)};
                        if (overlaps.find(idx) == overlaps.end())
                            grid[(std::size_t)x][(std::size_t)y] = '*';
                    }

                    // Update possibilties of particular word for early stopping
                    if (--counts[word] <= 0)
                        return false;

                    // One less backtrack allowed
                    if (--backtrackThresh <= 0)
                        return false;


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
            std::unique_ptr<Trie> root{Trie::init(words)}; 
            std::unordered_set<std::string> pending {words.begin(), words.end()};

            // Restart after threshold num of steps
            bool status = false;
            while (!status) {
                int cumulativeThresh {10};
                status = backtrackGenerate(root, pending, cumulativeThresh);
            }

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
