// https://queensgame.vercel.app/level/1

#include <iostream>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <unordered_set>
#include <vector>

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

        bool solve() {
            std::stack<std::pair<std::size_t, std::size_t>> stk{{{0, 0}}};
            while (!stk.empty()) {
                std::size_t row, col;
                std::tie(row, col) = stk.top();

                // When we were able to move 1 row beyond max
                if (row == nQueens) return true;

                // When we have tried all combinations for a row
                else if (col == nQueens) {
                    stk.pop();
                    if (!stk.empty()) {
                        unset(stk.top().first, stk.top().second);
                        stk.top().second += 1;
                    }
                } 

                // If valid, place row and col onto grid, next row
                else if (check(row, col)) {
                    set(row, col);
                    stk.push({row + 1, 0});
                } 

                // Not valid and col is within bounds, try next col
                else stk.top().second += 1;
            }
            
            return false;
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
