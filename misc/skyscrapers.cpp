#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <istream>
#include <limits>
#include <random>
#include <sstream>
#include <stack>
#include <unordered_set>
#include <vector>

class Skyscrapers {
    private:
        static std::mt19937 RANDOM_GEN;
        enum AXIS { ROW, COL };

        // Use a stack to compute the actuals for a row / col to compare againt hints for validation
        static std::pair<std::size_t, std::size_t> computeHintActuals(std::vector<std::vector<int>> &grid, std::size_t idx, AXIS flag) {
            std::size_t N {grid.size()};
            std::stack<int> leftStk, rightStk;
            for (std::size_t i {0}; i < N; i++) {
                int val {flag == ROW? grid[idx][i]: grid[i][idx]};
                if (leftStk.empty() || leftStk.top() < val)
                    leftStk.push(val);
                while (!rightStk.empty() && rightStk.top() < val)
                    rightStk.pop();
                rightStk.push(val);
            }
            return {leftStk.size(), rightStk.size()};
        }

        // Get the puzzle hints for the board
        static std::array<std::vector<std::size_t>, 4> generateHints(std::vector<std::vector<int>> &grid) {
            std::size_t N {grid.size()};
            std::vector<std::size_t> top(N, 0), left(N, 0), bottom(N, 0), right(N, 0); 

            // Row hints
            for (std::size_t row {0}; row < N; row++) {
                std::pair<std::size_t, std::size_t> rowHint {computeHintActuals(grid, row, AXIS::ROW)};
                left[row] = rowHint.first;
                right[row] = rowHint.second;
            }

            // Col hints
            for (std::size_t col {0}; col < N; col++) {
                std::pair<std::size_t, std::size_t> colHint {computeHintActuals(grid, col, AXIS::COL)};
                top[col] = colHint.first;
                bottom[col] = colHint.second;
            }

            return {top, right, bottom, left};
        }


        // Generate the puzzle board
        static bool generateBoard(std::vector<std::vector<int>> &grid, std::size_t row, std::size_t col) {
            // For each cell generate candidates, shuffle and try assigning to grid
            std::size_t N {grid.size()};
            if (row == N) 
                return true;

            else if (col == N) 
                return generateBoard(grid, row + 1, 0);

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
                    if (generateBoard(grid, row, col + 1))
                        return true;
                }

                // Return if nothing works, backtrack
                return false;
            }
        }

        // Generate candidates for the board
        static std::vector<std::vector<std::unordered_set<int>>> generateCandidates(
            const std::vector<std::vector<int>> &grid, 
            const std::array<std::vector<std::size_t>, 4> & hints
        ) {

            std::size_t N {grid.size()};
            std::vector<std::vector<std::unordered_set<int>>> candidates(N, std::vector<std::unordered_set<int>>(N));

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
                        for (int i{static_cast<int>(N) - static_cast<int>(hints[0][col]) + 2 + static_cast<int>(row)}; i <= static_cast<int>(N); i++)
                            invalidCandidates.insert(i);

                        // Eliminate using right hint
                        for (int i{static_cast<int>(N) - static_cast<int>(hints[1][row]) + 2 + static_cast<int>(N - col)}; i <= static_cast<int>(N); i++)
                            invalidCandidates.insert(i);

                        // Eliminate using bottom hint
                        for (int i{static_cast<int>(N) - static_cast<int>(hints[2][col]) + 2 + static_cast<int>(N - row)}; i <= static_cast<int>(N); i++)
                            invalidCandidates.insert(i);

                        // Eliminate using left hint
                        for (int i{static_cast<int>(N) - static_cast<int>(hints[3][row]) + 2 + static_cast<int>(col)}; i <= static_cast<int>(N); i++)
                            invalidCandidates.insert(i);

                        // Only retain candidates that are valid
                        for (int candidate {1}; candidate <= static_cast<int>(N); candidate++) {
                            if (invalidCandidates.find(candidate) == invalidCandidates.end())
                                candidates[row][col].insert(candidate);
                        }
                    }
                }
            }

            return candidates;
        }

        static bool backtrackSolve(
            std::vector<std::vector<int>> &grid, const std::array<std::vector<std::size_t>, 4> &hints, 
            std::vector<std::vector<std::unordered_set<int>>> &candidates
        ) {
            std::size_t N {grid.size()}, minRow{0}, minCol{0}; 
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
            for (int candidate: candidates[minRow][minCol]) {
                // We store the removed candidates to restore later
                std::vector<std::pair<std::size_t, std::size_t>> removedFromCandidates;
                grid[minRow][minCol] = candidate;

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
                if (rowFilled == N) {
                    std::pair<std::size_t, std::size_t> rowHint {computeHintActuals(grid, minRow, AXIS::ROW)};
                    std::size_t leftExpected {hints[3][minRow]}, rightExpected {hints[1][minRow]};
                    rowHintOkay &=  leftExpected == 0 || rowHint.first == leftExpected;
                    rowHintOkay &= rightExpected == 0 || rowHint.second == rightExpected;
                }
                if (colFilled == N) {
                    std::pair<std::size_t, std::size_t> colHint {computeHintActuals(grid, minCol, AXIS::COL)};
                    std::size_t upExpected {hints[0][minCol]}, downExpected {hints[2][minCol]};
                    colHintOkay &=   upExpected == 0 || colHint.first == upExpected;
                    colHintOkay &= downExpected == 0 || colHint.second == downExpected;
                }

                if (allCellsHaveCandidates && rowHintOkay && colHintOkay && backtrackSolve(grid, hints, candidates))
                    return true;

                // Revert changes
                grid[minRow][minCol] = 0;
                for (auto [i, j]: removedFromCandidates)
                    candidates[i][j].insert(candidate);
            }

            return false;
        }

    public:
        // Read string from console in the expected input format
        static std::pair<std::vector<std::vector<int>>, std::array<std::vector<std::size_t>, 4>> read(std::istream &in) {
            std::size_t N, val;
            in >> N;
            std::array<std::vector<std::size_t>, 4> hints;

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
                    grid.back().push_back(static_cast<int>(val));
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

        // Outupt as string given board and hints
        static std::string print(const std::vector<std::vector<int>> &puzzle, const std::array<std::vector<std::size_t>, 4> &hints, bool pretty = true) {
            std::size_t N {puzzle.size()};
            std::vector<std::size_t> top{hints[0]}, right{hints[1]}, down{hints[2]}, left{hints[3]};

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
            std::size_t dashLen{0};
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

        // Generate board given size
        static std::pair<std::vector<std::vector<int>>, std::array<std::vector<std::size_t>, 4>> generate(std::size_t N) {
            std::vector<std::vector<int>> puzzle(N, std::vector<int>(N));
            generateBoard(puzzle, 0, 0);
            std::array<std::vector<std::size_t>, 4> hints {generateHints(puzzle)};
            return {puzzle, hints};
        }

        // Solve the partially filled puzzle with provided hints
        static bool solve(std::vector<std::vector<int>> &puzzle, std::array<std::vector<std::size_t>, 4> &hints) {
            std::vector<std::vector<std::unordered_set<int>>> candidates {generateCandidates(puzzle, hints)};
            return backtrackSolve(puzzle, hints, candidates);
        }
};

// Initialize static variable
std::mt19937 Skyscrapers::RANDOM_GEN {std::random_device{}()};

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "generate") != 0 && std::strcmp(argv[1], "solve") != 0)) {
        std::cout << "Usage: `echo <N> | skyscrapers generate` (OR) `skyscrapers solve < <file.txt>`\n";
    }

    else if (std::strcmp(argv[1], "generate") == 0) {
        std::size_t N;
        std::cin >> N;
        auto [puzzle, hints] = Skyscrapers::generate(N);
        std::cout << Skyscrapers::print(puzzle, hints, false);
    }

    else {
        std::pair<std::vector<std::vector<int>>, std::array<std::vector<std::size_t>, 4>> inputResult {Skyscrapers::read(std::cin)};
        auto [puzzle, hints] {inputResult};
        bool status {Skyscrapers::solve(puzzle, hints)};
        if (!status) { std::cerr << "Invalid board.\n"; return 1; }
        std::cout << Skyscrapers::print(puzzle, hints, true);
    }
}
