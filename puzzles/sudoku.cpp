#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

using BOARD_T = std::array<std::array<char, 9>, 9>;

class Sudoku {
    private:
        // Canvas to solve and generate sudokus
        BOARD_T board;

        // For random shuffling
        std::mt19937 random_gen;

        // Given a sudoku cell, lists all viable candidates for that cell
        std::vector<std::size_t> getCandidates(std::size_t row, std::size_t col) {
            // Vector of bools denoting if that num is a viable candidate
            std::array<bool, 9> candidates;
            std::fill(candidates.begin(), candidates.end(), true);

            // Row wise check
            for (std::size_t j {0}; j < 9; j++)
                if (board[row][j] != '.')
                    candidates[(std::size_t)board[row][j] - '0' - 1] = false;

            // Col wise check
            for (std::size_t i {0}; i < 9; i++)
                if (board[i][col] != '.')
                    candidates[(std::size_t)board[i][col] - '0' - 1] = false;

            // Grid wise check
            std::size_t grow {(row / 3) * 3}, gcol {(col / 3) * 3};
            for (std::size_t i {grow}; i < grow + 3; i++)
                for (std::size_t j {gcol}; j < gcol + 3; j++)
                    if (board[i][j] != '.')
                        candidates[(std::size_t)board[i][j] - '0' - 1] = false;

            // Bool array to indices vector
            std::vector<std::size_t> indices;
            for (std::size_t idx{0}; idx < 9; idx++)
                if (candidates[idx])
                    indices.push_back(idx);

            return indices;
        }

        /*
        Recursively attempt filling the sudoku board. Non static in nature, solves the 'board' canvas
        Randomized shuffles the candidates order
            - If randomized we stop once a solution is reached - board is NOT cleared
            - If unrandomized post solving, board is comes to its initial state
        */
        bool backtrack(std::size_t row, std::size_t col, bool randomized = false) {
            if (row == 9) {
                solutions.push_back(board);
                return true;
            } else if (col == 9) {
                return backtrack(row + 1, 0, randomized);
            } else if (board[row][col] != '.') {
                return backtrack(row, col + 1, randomized);
            } else {
                std::vector<std::size_t> candidates{getCandidates(row, col)};

                if (randomized)
                    std::shuffle(candidates.begin(), candidates.end(), random_gen);

                for (const std::size_t i: candidates) {
                    board[row][col] = (char)((i + 1) + '0');

                    // Stop early if randomized, otherwise it might recurse forever
                    bool result {backtrack(row, col + 1, randomized)};
                    if (result && randomized)
                        return true;

                    board[row][col] = '.';
                }

                return false;
            }
        }

        // Helper to init an empty board with all '.'(s)
        static constexpr BOARD_T createEmptyBoard() {
            BOARD_T board;
            std::for_each(board.begin(), board.end(), [](std::array<char, 9> &row) { row.fill('.'); });
            return board;
        }

        // Helper to generate pairs of all cells, from (0,0) to (8,8)
        static constexpr std::vector<std::pair<std::size_t, std::size_t>> cells() {
            std::vector<std::pair<std::size_t, std::size_t>> filledCells;
            for (std::size_t i {0}; i < 9; i++)
                for (std::size_t j {0}; j < 9; j++)
                    filledCells.push_back({i, j});
            return filledCells;
        }

    public:
        std::vector<BOARD_T> solutions;
        Sudoku (BOARD_T &board): board(board) {
            random_gen = std::mt19937{std::random_device{}()};
        }

        Sudoku() {
            random_gen = std::mt19937{std::random_device{}()};
            board = createEmptyBoard();
        }

        void solve() {
            backtrack(0, 0, false); 
            if (solutions.size() == 0)
                std::cout << "Error: Invalid Sudoku.\n";
            else if (solutions.size() > 1)
                std::cout << "Error: Multiple solutions exist.\n";
            else
                print(solutions[0]);
        }

        void generate() {
            // Get random solved grid
            backtrack(0, 0, true); 

            // We'd be randomly remove numbers while there exists only a single solution
            std::vector<std::pair<std::size_t, std::size_t>> filledCells {cells()};

            // Shuffle to introduce randomness
            std::shuffle(filledCells.begin(), filledCells.end(), random_gen);

            // Start removal
            while (!filledCells.empty()) {
                auto [row, col] = filledCells.back();
                filledCells.pop_back();
                std::pair<std::pair<std::size_t, std::size_t>, char> lastRemoved {{row, col}, board[row][col]};
                board[row][col] = '.';

                // Unrandomized solving since only then we would generate all solutions
                solutions.clear();
                backtrack(0, 0, false); 

                // Reinsert last removed if uniqueness not preserved
                if (solutions.size() > 1)
                    board[lastRemoved.first.first][lastRemoved.first.second] = lastRemoved.second;
            }

            // Print the unsolved board to console
            print(board);
        }

        // Pretty print sudoku board
        static void print(BOARD_T board) {
            std::ostringstream oss;
            std::string horizontalSep(25, '-');
            oss << horizontalSep << "\n";
            for (std::size_t i {0}; i < 9; i++) {
                oss << "| ";
                for (std::size_t j {0}; j < 9; j++)
                    oss << board[i][j] << (j % 3 == 2? " | ": " ");
                oss << "\n" << (i % 3 == 2? horizontalSep + "\n": "");
            }
            std::cout << oss.str();
        }

        /*
         * Read BOARD from filename provided -
         * All characters besides digits and '.' are ignored
         */
        static BOARD_T read(std::string fname) {
            std::ifstream ifs{fname};
            if (!ifs) {
                std::cout << fname << ": No such file or directory\n"; 
                std::exit(1);
            } else {
                BOARD_T grid;
                char ch;
                std::size_t row {0}, col {0};
                while (ifs >> ch) {
                    // Push to invalid range (out of bounds)
                    if ((std::isdigit(ch) || ch == '.') && row == 9) { col++; break; }

                    else if (std::isdigit(ch) || ch == '.') { grid[row][col++] = ch; }
                    
                    // Post reaching end of each row, jump to first cell of next row
                    if (col == 9) { col = 0; row++; }
                }
                if (row == 9 && col == 0)
                    return grid;
                else {
                    std::cout << "Error: Invalid input.\n"; 
                    std::exit(1);
                }

            }
        }
};

int main(int argc, char **argv) {

    if (argc > 2) {
        std::cout << "Usage: ./sudoku <filename>\n";
    } else if (argc == 1) {
        Sudoku game;
        game.generate();
    } else {
        BOARD_T grid {Sudoku::read(argv[1])};
        Sudoku game{grid};
        game.solve();
    }

    return 0;
}
