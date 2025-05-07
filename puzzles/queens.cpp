// https://queensgame.vercel.app/level/1

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

struct Coords {
    std::size_t row, col;
    Coords(std::size_t row, std::size_t col): row(row), col(col) {}
    inline bool operator==(const Coords &other) const { 
        return row == other.row && col == other.col; 
    }

    // Define a new convenience type
    static std::size_t HashCoord(const Coords& p) { return (p.row * 31) + p.col; };
    using CoordSet = std::unordered_set<Coords, decltype(&HashCoord)>;
};

class QueensSolver {
    private:
        const std::vector<std::vector<char>> &grid; 
        std::size_t nQueens;
        std::unordered_set<std::size_t> cols;
        std::unordered_set<char> regions; 
        std::vector<std::vector<bool>> solution;

        // Returns true if neighbours of said cell has a queen placed
        bool checkNeighbours(const std::size_t row, const std::size_t col) const {
            for (int dr {-1}; dr <= 1; ++dr) {
                for (int dc {-1}; dc <= 1; ++dc) {
                    std::size_t cRow {row + static_cast<std::size_t>(dr)};
                    std::size_t cCol {col + static_cast<std::size_t>(dc)};
                    if (cRow < nQueens && cCol < nQueens && solution[cRow][cCol])
                        return true;
                }
            }
            return false;
        }

        bool check(const std::size_t row, const std::size_t col) const {
            if (row >= nQueens || col >= nQueens) return false;
            else if (cols.find(col) == cols.end()) return false;
            else if (regions.find(grid[row][col]) == regions.end()) return false;
            else if (checkNeighbours(row, col)) return false;
            else return true;
        }

        void set(const std::size_t row, const std::size_t col) {
            solution[row][col] = true; cols.erase(col); 
            regions.erase(grid[row][col]);
        }

        void unset(const std::size_t row, const std::size_t col) {
            solution[row][col] = false; cols.insert(col); 
            regions.insert(grid[row][col]);
        }

    public:
        QueensSolver(const std::vector<std::vector<char>> &grid): 
            grid(grid), nQueens(grid.size()), 
            solution(nQueens, std::vector<bool>(nQueens, false)) 
        {
            // Check - 1
            if (grid.empty() || grid.size() != grid[0].size())
                throw std::runtime_error("Invalid Grid dimensions");

            // Cols is the set of still unfilled columns
            for (std::size_t i {0}; i < nQueens; ++i) 
                cols.insert(i);

            // Obtain the list of regions unfilled
            for (const std::vector<char> &row: grid)
                for (const char& ch: row)
                    regions.insert(ch);

            // Check - 2
            if (regions.size() != nQueens) 
                throw std::runtime_error("Regions must equal the grid dimensions");
        }

        // Solve the board recursively
        bool solve(const std::size_t row = 0) {
            if (row == nQueens) return true;
            else {
                for (std::size_t col {0}; col < nQueens; ++col) {
                    if (check(row, col)) {
                        set(row, col);
                        if (solve(row + 1)) return true;
                        unset(row, col);
                    }
                }
                return false;
            }
        }

        // Print the board
        std::string getString() const {
            std::ostringstream oss;
            std::string lineSep(nQueens * 2, '-');
            oss << lineSep << '\n';
            for (const std::vector<bool> &row: solution) {
                for (const bool& pos: row)
                    oss << (pos? 'X': ' ') << '|';
                oss << '\n' << lineSep << '\n';
            }
            return oss.str();
        }
};

int main() {

    // Read from input
    std::ostringstream oss;
    oss << std::cin.rdbuf();

    // Write into vector post cleaning, store the regions
    std::size_t nQueens {0};
    std::vector<std::vector<char>> grid{{}};
    for (const char &ch: oss.str()) {
        if (ch == '\n') {
            std::size_t rowC {grid.back().size()};
            if (grid.back().empty()) continue;
            else if (nQueens == 0) nQueens = rowC;
            else if (nQueens != rowC) throw std::runtime_error(
                "Expected row to have " + std::to_string(nQueens) + ", got " 
                + std::to_string(rowC));
            grid.push_back({});
        } else if (ch != ' ') {
            grid.back().push_back(ch);
        }
    }

    // Ensure that we have the right no. of colors
    if (grid.back().empty()) grid.pop_back();

    // Solve the grid
    QueensSolver solver(grid);
    if (!solver.solve())
        throw std::runtime_error("No solution exists");

    // Print out the solution
    std::cout << solver.getString();

    return 0;
}
