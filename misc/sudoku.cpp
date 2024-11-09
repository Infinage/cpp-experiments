#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>

using BOARD_T = std::array<std::array<char, 9>, 9>;
using CANDIDATES_T = std::array<bool, 9>;

class Sudoku {
    private:
        BOARD_T board;
        std::mt19937 random_gen;

        CANDIDATES_T getCandidates(std::size_t row, std::size_t col) {
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

            return candidates;
        }

        bool backtrack(std::size_t row, std::size_t col, bool randomized = false) {
            if (row == 9)
                return true;
            else if (col == 9)
                return backtrack(row + 1, 0);
            else if (board[row][col] != '.')
                return backtrack(row, col + 1);
            else {
                CANDIDATES_T candidates{getCandidates(row, col)};

                std::vector<std::size_t> indices(9);
                std::iota(indices.begin(), indices.end(), 0);

                if (randomized)
                    std::shuffle(indices.begin(), indices.end(), random_gen);

                for (const std::size_t i: indices)
                    if (candidates[i]) {
                        board[row][col] = (char)((i + 1) + '0');
                        if (backtrack(row, col + 1))
                            return true;
                        board[row][col] = '.';
                    }
                return false;
            }
        }

        static BOARD_T createEmptyBoard() {
            BOARD_T board;
            std::for_each(board.begin(), board.end(), [](std::array<char, 9> &row) { row.fill('.'); });
            return board;
        }

    public:
        Sudoku (BOARD_T &board): board(board) {
            std::random_device rd;
            random_gen = std::mt19937(rd());
        }

        Sudoku() {
            std::random_device rd;
            random_gen = std::mt19937(rd());
            board = createEmptyBoard();
        }

        bool solve(bool randomized) { return backtrack(0, 0, randomized); }

        void print() {
            std::string horizontalSep(25, '-');
            std::cout << horizontalSep << "\n";
            for (std::size_t i {0}; i < 9; i++) {
                std::cout << "| ";
                for (std::size_t j {0}; j < 9; j++)
                    std::cout << board[i][j] << (j % 3 == 2? " | ": " ");
                std::cout << "\n" << (i % 3 == 2? horizontalSep + "\n": "");
            }
        }

        static BOARD_T read(std::string fname) {
            std::ifstream ifs{fname};
            if (!ifs) {
                std::cout << fname << ": No such file or directory\n"; 
                std::exit(1);
            } else {
                BOARD_T grid;
                for (std::size_t i {0}; i < 9; i++)
                    for (std::size_t j {0}; j < 9; j++)
                        ifs >> grid[i][j];
                return grid;
            }
        }
};

int main(int argc, char **argv) {

    if (argc > 2) {
        std::cout << "Usage: ./sudoku <filename>\n";
    } else if (argc == 1) {
        Sudoku game;
        game.solve(true);
        game.print();
    } else {
        BOARD_T grid {Sudoku::read(argv[1])};
        Sudoku game{grid};
        game.solve(false);
        game.print();
    }

    return 0;
}
