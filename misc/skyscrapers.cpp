#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <istream>
#include <limits>
#include <random>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

/*
 * If we at any cell we have hint* as 1, we set that to cell value to N (no candidates contenstion)
 * If a particular value can only occur at a single cell in a particular row / col - assign it that val
 */

// Random num gen
std::mt19937 RANDOM_GEN{std::random_device{}()};

struct HashPair {
    std::size_t operator()(const std::pair<std::size_t, int> &p) const {
        return p.first * (static_cast<std::size_t>(p.second) * 31);
    }
};

using SIZE_T_INT_P = std::pair<std::size_t, int>;
using SIZE_T_INT_UMAP = std::unordered_map<SIZE_T_INT_P, std::unordered_set<std::size_t>, HashPair>;

// Generate the puzzle
bool generate(std::vector<std::vector<int>> &grid, std::size_t row, std::size_t col) {
    // For each cell generate candidates, shuffle and try assigning to grid
    std::size_t N {grid.size()};
    if (row == N) 
        return true;

    else if (col == N) 
        return generate(grid, row + 1, 0);

    else {
        // Get list of invalid candidates by checking row and col so far
        std::unordered_set<int> invalidCandidates;
        for (std::size_t i {0}; i < row; i++)
            invalidCandidates.insert(grid[i][col]);
        for (std::size_t j {0}; j < col; j++)
            invalidCandidates.insert(grid[row][j]);

        // Get list of valid candidates
        std::vector<int> validCandidates;
        for (int i {1}; i <= static_cast<int>(N); i++)
            if (invalidCandidates.find(i) == invalidCandidates.end())
                validCandidates.push_back(i);

        // Shuffle and try inserting in order
        std::shuffle(validCandidates.begin(), validCandidates.end(), RANDOM_GEN);
        while (!validCandidates.empty()) {
            int curr {validCandidates.back()};
            validCandidates.pop_back();
            grid[row][col] = curr;
            if (generate(grid, row, col + 1))
                return true;
        }

        // Return if nothing works, backtrack
        return false;
    }
}

// Get the puzzle hints for the board
std::array<std::vector<int>, 4> generateHints(std::vector<std::vector<int>> &grid) {
    std::size_t N {grid.size()};
    std::vector<int> top(N, 0), left(N, 0), down(N, 0), right(N, 0); 

    for (std::size_t j {0}; j < N; j++) {
        int prevMax {0};
        for (std::size_t i {0}; i < N; i++) {
            if (grid[i][j] > prevMax) {
                prevMax = grid[i][j];
                top[j]++; 
            }
        }
    }

    for (std::size_t i {0}; i < N; i++) {
        int prevMax {0};
        for (std::size_t j {0}; j < N; j++) {
            if (grid[i][j] > prevMax) {
                prevMax = grid[i][j];
                left[i]++; 
            }
        }
    }

    for (std::size_t j {0}; j < N; j++) {
        int prevMax {0};
        for (std::size_t i {N}; i-- > 0;) {
            if (grid[i][j] > prevMax) {
                prevMax = grid[i][j];
                down[j]++; 
            }
        }
    }

    for (std::size_t i {0}; i < N; i++) {
        int prevMax {0};
        for (std::size_t j {N}; j-- > 0;) {
            if (grid[i][j] > prevMax) {
                prevMax = grid[i][j];
                right[i]++; 
            }
        }
    }

    return {top, right, down, left};
}

std::string print(const std::vector<std::vector<int>> &puzzle, const std::array<std::vector<int>, 4> &hints, bool pretty = true) {
    std::size_t N {puzzle.size()};
    std::vector<int> top{hints[0]}, right{hints[1]}, down{hints[2]}, left{hints[3]};

    // Combine the hints with the puzzle into to a print friendly vector
    std::vector<std::vector<std::string>> combined(N + 2, std::vector<std::string>(N + 2, std::to_string(N)));
    for (std::size_t i{0}; i < N; i++) 
        combined[0][i + 1] = std::to_string(top[i]);

    for (std::size_t i {0}; i < N; i++) {
        combined[i + 1][0] = std::to_string(left[i]);
        for (std::size_t j {0}; j < N; j++) {
            combined[i + 1][j + 1] = std::to_string(puzzle[i][j]); 
        }
        combined[i + 1][N + 1] = std::to_string(right[i]);
    }

    for (std::size_t i{0}; i < N; i++) 
        combined[N + 1][i + 1] = std::to_string(down[i]);

    // Print combined grid
    std::ostringstream oss;
    int maxNDigits{N > 0? static_cast<int>(std::log10(N)) + 1: 1};
    std::size_t dashLen;
    for (std::size_t i{0}; i < N + 2; i++) {
        for (std::size_t j{0}; j < N + 2; j++) {
            if (pretty) 
                oss << std::setw(maxNDigits);

            oss << combined[i][j];

            if (!pretty && j <= N)
                oss << " ";
            else if (j == 0 || j == N)
                oss << " | ";
            else if (j < N)
                oss << "   ";
        }

        if (i == 0 && pretty)
            oss << '\n' + std::string(dashLen = static_cast<std::size_t>(oss.tellp()), '-') + '\n';
        else if (i == N && pretty)
            oss << '\n' + std::string(dashLen, '-') + '\n';
        else
            oss << "\n";
    }

    return oss.str();
}

std::pair<std::vector<std::vector<int>>, std::array<std::vector<int>, 4>> read(std::istream &in) {
    int val; 
    std::size_t N;
    in >> N;
    std::array<std::vector<int>, 4> hints;

    // Read top hint
    for (std::size_t i {0}; i < N; i++) {
        in >> val;
        hints[0].push_back(val);
    }

    // Skip top right
    in >> val;

    // Read next N lines
    std::vector<std::vector<int>> grid;
    for (std::size_t i{0}; i < N; i++) {
        in >> val; hints[3].push_back(val);
        grid.push_back(std::vector<int>());
        for (std::size_t j{0}; j < N; j++) {
            in >> val;
            grid.back().push_back(val);
        }
        in >> val; hints[1].push_back(val);
    }

    // Skip bottom left
    in >> val;

    // Read bottom hint
    for (std::size_t i {0}; i < N; i++) {
        in >> val;
        hints[2].push_back(val);
    }

    // Skip bottom right
    in >> val;
    
    return {grid, hints};
}

bool validateRight(const std::vector<std::vector<int>> &grid, const std::array<std::vector<int>, 4> &hints, std::size_t row) {
    // Look left and try to check if condition holds true
    int hint {hints[1][row]}, actual {0}, prevMax {0};
    if (hint == 0) return true;
    else {
        for (std::size_t j {grid.size()}; j-- > 0;) {
            if (grid[row][j] > prevMax) {
                prevMax = grid[row][j];
                actual++; 
            }
        }

        return actual == hint;
    }
}

bool validateLeft(const std::vector<std::vector<int>> &grid, const std::array<std::vector<int>, 4> &hints, std::size_t row) {
    // Look left and try to check if condition holds true
    int hint {hints[3][row]}, actual {0}, prevMax {0};
    if (hint == 0) return true;
    else {
        for (std::size_t j {0}; j < grid.size(); j++) {
            if (grid[row][j] > prevMax) {
                prevMax = grid[row][j];
                actual++; 
            }
        }

        return actual == hint;
    }
}

bool validateDown(const std::vector<std::vector<int>> &grid, const std::array<std::vector<int>, 4> &hints, std::size_t col) {
    // Look upwards and try to check if condition holds true
    int hint {hints[2][col]}, actual {0}, prevMax {0};
    if (hint == 0) return true;
    else {
        for (std::size_t i {grid.size()}; i-- > 0;) {
            if (grid[i][col] > prevMax) {
                prevMax = grid[i][col];
                actual++; 
            }
        }

        return actual == hint;
    }
}

bool validateUp(const std::vector<std::vector<int>> &grid, const std::array<std::vector<int>, 4> &hints, std::size_t col) {
    // Look upwards and try to check if condition holds true
    int hint {hints[0][col]}, actual {0}, prevMax {0};
    if (hint == 0) return true;
    else {
        for (std::size_t i {0}; i < grid.size(); i++) {
            if (grid[i][col] > prevMax) {
                prevMax = grid[i][col];
                actual++; 
            }
        }

        return actual == hint;
    }
}

std::tuple<
SIZE_T_INT_UMAP, SIZE_T_INT_UMAP,
std::vector<std::vector<std::unordered_set<int>>>
> generateCandidates(
    const std::vector<std::vector<int>> &grid, 
    const std::array<std::vector<int>, 4> & hints
) {

    std::size_t N {grid.size()};
    std::vector<std::vector<std::unordered_set<int>>> candidates(N, std::vector<std::unordered_set<int>>(N));

    // Each inside the puzzle can only be set at a single pos in the row / col
    // Lets track where those positions are for each cell. This would help us to
    // take a shortcut of assigning a cell a value if that value can only go to
    // a particular cell - row or col wise
    SIZE_T_INT_UMAP rowWise, colWise;
    for (std::size_t i {0};i < N; i++) {
        for (int j {1}; j <= static_cast<int>(N); j++) {
            for (std::size_t pos {0}; pos < N; pos++) {
                rowWise[{i, j}].insert(pos); 
                colWise[{i, j}].insert(pos);
            }
        }
    }

    for (std::size_t row {0}; row < N; row++) {
        for (std::size_t col {0}; col < N; col++) {
            if (grid[row][col] == 0) {
                std::unordered_set<int> invalidCandidates;
                // Eliminate existing values in same row
                for (std::size_t i{0}; i < N; i++)
                    if (grid[i][col] != 0)
                        invalidCandidates.insert(grid[i][col]);

                // Eliminate existing values in same col
                for (std::size_t j{0}; j < N; j++)
                    if (grid[row][j] != 0)
                        invalidCandidates.insert(grid[row][j]);

                // Eliminate using top hint
                for (int i{static_cast<int>(N) - hints[0][col] + 2 + static_cast<int>(row)}; i <= static_cast<int>(N); i++)
                    invalidCandidates.insert(i);

                // Eliminate using right hint
                for (int i{static_cast<int>(N) - hints[1][row] + 2 + static_cast<int>(N - col)}; i <= static_cast<int>(N); i++)
                    invalidCandidates.insert(i);

                // Eliminate using bottom hint
                for (int i{static_cast<int>(N) - hints[2][col] + 2 + static_cast<int>(N - row)}; i <= static_cast<int>(N); i++)
                    invalidCandidates.insert(i);

                // Eliminate using left hint
                for (int i{static_cast<int>(N) - hints[3][row] + 2 + static_cast<int>(col)}; i <= static_cast<int>(N); i++)
                    invalidCandidates.insert(i);

                // Only retain candidates that are valid
                for (int candidate {1}; candidate <= static_cast<int>(N); candidate++) {
                    if (invalidCandidates.find(candidate) == invalidCandidates.end()) {
                        candidates[row][col].insert(candidate);
                    } else {
                        // Row / Col cannot contain this candidate
                        rowWise[{row, candidate}].erase(col);
                        colWise[{col, candidate}].erase(row);
                    }
                }
            } else {
                rowWise[{row, grid[row][col]}] = {col};
                colWise[{col, grid[row][col]}] = {row};
            }
        }
    }

    // Update candidates if it can only be written to a single cell in a row / col
    for (const std::pair<const SIZE_T_INT_P, std::unordered_set<std::size_t>> &p: rowWise) {
        std::size_t row; int candidate;
        std::tie(row, candidate) = p.first;
        if (p.second.size() == 1) {
            candidates[row][*p.second.begin()] = {candidate};
            for (std::size_t i {0}; i < N; i++)
                colWise[{i, candidate}].erase(row);
        }
    }
    for (const std::pair<const SIZE_T_INT_P, std::unordered_set<std::size_t>> &p: colWise) {
        std::size_t col; int candidate;
        std::tie(col, candidate) = p.first;
        if (p.second.size() == 1) {
            candidates[*p.second.begin()][col] = {candidate};
            for (std::size_t i {0}; i < N; i++)
                rowWise[{i, candidate}].erase(col);
        }
    }

    return {rowWise, colWise, candidates};
}

bool solve(
    std::vector<std::vector<int>> &grid, const std::array<std::vector<int>, 4> &hints, 
    std::vector<std::vector<std::unordered_set<int>>> &candidates
) {
    std::size_t N {grid.size()}, minRow, minCol; 
    std::size_t minCandidates {std::numeric_limits<std::size_t>::max()};

    // Pick the cell with min candidates for faster pruning
    for (std::size_t row {0}; row < N; row++) {
        for (std::size_t col {0}; col < N; col++) {
            std::size_t numCandidates {candidates[row][col].size()};
            if (grid[row][col] == 0 && numCandidates == 0) return false;
            else if (grid[row][col] == 0 && numCandidates < minCandidates) {
                minCandidates = numCandidates;
                minRow = row; minCol = col;
            }
        }
    }

    // Return true if all solved
    if (minCandidates == std::numeric_limits<std::size_t>::max())
        return true;

    // Next step
    std::unordered_set<int> tempCandidates {candidates[minRow][minCol]};
    for (int candidate: tempCandidates) {

        // We store the removed candidates to restore later
        std::vector<std::pair<std::size_t, std::size_t>> removedFromCandidates;

        grid[minRow][minCol] = candidate;
        candidates[minRow][minCol].erase(candidate);

        // Remove candidates from affected cells - store to revert back on fail
        bool allCellsHaveCandidates {true}, rowHintOkay {true}, colHintOkay {true}; 
        std::size_t rowFilled {0}, colFilled {0};
        for (std::size_t i{0}; i < N; i++) { // Going downwards
            std::unordered_set<int> &cellCandidates {candidates[i][minCol]};
            if (grid[i][minCol] != 0) colFilled++;
            else if (cellCandidates.find(candidate) != cellCandidates.end()) {
                cellCandidates.erase(candidate);
                removedFromCandidates.push_back({i, minCol});
                allCellsHaveCandidates &= !cellCandidates.empty();
            }
        }
        for (std::size_t j{0}; j < N; j++) { // Going rightwards
            std::unordered_set<int> &cellCandidates {candidates[minRow][j]};
            if (grid[minRow][j] != 0) rowFilled++;
            else if (cellCandidates.find(candidate) != cellCandidates.end()) {
                cellCandidates.erase(candidate);
                removedFromCandidates.push_back({minRow, j});
                allCellsHaveCandidates &= !cellCandidates.empty();
            }
        }

        // Ensure row / col hints are okay
        rowHintOkay = rowFilled < N || (validateLeft(grid, hints, minRow) && validateRight(grid, hints, minRow));
        colHintOkay = colFilled < N || (validateUp(grid, hints, minCol) && validateDown(grid, hints, minCol));

        if (allCellsHaveCandidates && rowHintOkay && colHintOkay && solve(grid, hints, candidates))
            return true;

        // Revert changes
        candidates[minRow][minCol] = tempCandidates;
        grid[minRow][minCol] = 0;
        for (auto [i, j]: removedFromCandidates)
            candidates[i][j].insert(candidate);
    }

    return false;
}

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "generate") != 0 && std::strcmp(argv[1], "solve") != 0)) {
        std::cout << "Usage: `echo <N> | skyscrapers generate` (OR) `skyscrapers solve < <file.txt>`\n";
    }

    else if (std::strcmp(argv[1], "generate") == 0) {
        std::size_t N;
        std::cin >> N;
        std::vector<std::vector<int>> puzzle(N, std::vector<int>(N));
        generate(puzzle, 0, 0);
        std::array<std::vector<int>, 4> hints {generateHints(puzzle)};
        std::cout << print(puzzle, hints, false);
    }

    else {
        std::pair<std::vector<std::vector<int>>, std::array<std::vector<int>, 4>> inputResult {read(std::cin)};
        auto [puzzle, hints] {inputResult};
        SIZE_T_INT_UMAP rowWise, colWise;
        std::vector<std::vector<std::unordered_set<int>>> candidates;
        std::tie(rowWise, colWise, candidates) = generateCandidates(puzzle, hints);
        bool status {solve(puzzle, hints, candidates)};
        if (!status) { std::cerr << "Invalid board.\n"; return 1; }
        std::cout << print(puzzle, hints, true);
    }
}
