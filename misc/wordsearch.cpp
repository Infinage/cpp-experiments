#include <array>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class WordSearch {
    private:
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
                    for (const std::string &word: words) {
                        Trie *curr = root.get();
                        for (const char ch: word) {
                            if (curr->next[ord(ch)] == nullptr)
                                curr->next[ord(ch)] = std::make_unique<Trie>();
                            curr = curr->next[ord(ch)].get();
                        }
                        curr->end = true;
                    }
                    return root;
                }
        };

        struct HashPair {
            inline std::size_t operator() (const std::pair<int, int> &p) const {
                return (std::size_t) ((31 * p.first) + p.second);
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

    public:
        std::vector<std::vector<char>> grid;
        std::vector<std::string> words;
        WordSearch(std::vector<std::vector<char>> &grid, std::vector<std::string> &words): grid(grid), words(words) {}

        bool isValid(int i, int j) {
            return 0 <= i && 0 <= j && i < (int)grid.size() && j < (int)grid[0].size();
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

        void print() {
            for (std::size_t i{0}; i < grid.size(); i++) {
                for (std::size_t j{0}; j < grid[0].size(); j++)
                    std::cout << grid[i][j] << " ";
                std::cout << "\n";
            }
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
    if (argc == 4 && (std::strcmp(argv[1], "generate") == 0 || std::strcmp(argv[1], "solve") == 0)) {
        std::vector<std::vector<char>> grid {WordSearch::readGrid(std::string{argv[2]})};
        std::vector<std::string> wordList {WordSearch::readWordList(std::string{argv[3]})};
        if (std::strcmp(argv[1],  "generate") == 0) {
            std::cout << "Error: Not implemented yet!\n";
        } else {
            WordSearch ws{grid, wordList};
            std::unordered_set<std::string> found{ws.solve()};
            std::cout << "Found: " << found.size() << " words.\nNot found: " << wordList.size() - found.size() << " words.\n\n";
            ws.print();
        } 
    } else {
        std::cout << "Usage:\n"
                  << "  1. Generate word search puzzle:\n"
                  << "     ./wordsearch generate <outfile> <wordlist>\n\n"
                  << "  2. Solve word search puzzle:\n"
                  << "     ./wordsearch solve <infile> <wordlist>\n";
    }
    return 0;
}
