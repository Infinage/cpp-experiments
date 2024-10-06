#include <algorithm>
#include <iterator>
#include <random>
#include <iostream>
#include <unordered_set>
#include <vector>

class Minesweeper {
private:
    struct hash_pair {
        inline std::size_t operator() (const std::pair<int, int>& p) const {
            return (p.first * 31) + p.second;
        }
    };

public:
    enum CELLTYPE { MINE=-1, EMPTY=0, ONE=1, TWO=2, THREE=3, FOUR=4, FIVE=5, SIX=6, SEVEN=7, EIGHT=8 };
    std::size_t rows, cols;
    int mineCount;
    std::vector<std::vector<CELLTYPE>> grid;

    Minesweeper(std::size_t rows, std::size_t cols, int mc): rows(rows), cols(cols), grid(rows, std::vector<CELLTYPE>(cols, EMPTY)), mineCount(mc) {
        std::vector<std::pair<int, int>> cells;
        for (std::size_t i = 0; i < rows; i++)
            for (std::size_t j = 0; j < cols; j++)
                cells.push_back({i, j});

        // Randomly pick mines
        auto gen = std::mt19937{std::random_device{}()};
        std::shuffle(cells.begin(), cells.end(), gen);
        std::unordered_set<std::pair<int, int>, hash_pair> mines(cells.begin(), cells.begin() + mineCount);

        // Color the mines
        for (std::size_t i = 0; i < rows; i++) {
            for (std::size_t j = 0; j < cols; j++) {
                if (mines.find({i, j}) != mines.end())
                    grid[i][j] = MINE;
                else {
                    int count = 0;
                    for (int di = -1; di <= 1; di++) {
                        for (int dj = -1; dj <= 1; dj++) {
                            int i_ = i + di, j_ = j + dj;
                            if (
                                    0 <= i_ && i_ < rows &&
                                    0 <= j_ && j_ < cols &&
                                    mines.find({i_, j_}) != mines.end()
                                )
                                count++;
                        }
                    }
                    grid[i][j] = (CELLTYPE) count;
                }
            }
        }
    };

    void print() const {
        for (std::size_t i = 0; i < rows; i++) {
            for (std::size_t j = 0; j < cols; j++) {
                if (grid[i][j] == MINE)
                    std::cout << "X" << " ";
                else if (grid[i][j] == EMPTY)
                    std::cout << "_" << " ";
                else
                    std::cout << grid[i][j] << " ";
            }
            std::cout << "\n";
        }
    }
};

int main() {
    Minesweeper game(10, 10, 15);
    game.print();
}
