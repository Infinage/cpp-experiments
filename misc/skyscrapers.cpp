#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <istream>
#include <random>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

// Random num gen
std::mt19937 RANDOM_GEN{std::random_device{}()};

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

std::string print(std::vector<std::vector<int>> &puzzle, std::array<std::vector<int>, 4> &hints, bool pretty = true) {
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

std::vector<int> generateCandidates(
    const std::vector<std::vector<int>> &grid, 
    const std::array<std::vector<int>, 4> &hints, 
    std::size_t row, std::size_t col
) {
    std::unordered_set<int> invalidCandidates;
    int  topActual{0},  topHint{hints[0][col]};
    int leftActual{0}, leftHint{hints[3][row]};
    int N {static_cast<int>(grid.size())};

    // Eliminate all existing values - row
    int prevMaxTop{0};
    for (std::size_t i{0}; i < static_cast<std::size_t>(N); i++) {
        invalidCandidates.insert(grid[i][col]);
        if (i < row && grid[i][col] > prevMaxTop) {
            prevMaxTop = grid[i][col];
            topActual++;
        }
    }

    // Eliminate all existing values - col
    int prevMaxLeft = 0;
    for (std::size_t j{0}; j < static_cast<std::size_t>(N); j++) {
        invalidCandidates.insert(grid[row][j]);
        if (j < col && grid[row][j] > prevMaxLeft) {
            prevMaxLeft = grid[row][j];
            leftActual++;
        }
    }

    // Fail early
    if ((topHint > 0 && topActual > topHint) || (leftHint > 0 && leftActual > leftHint))
        return {};

    // When Hint equals Actual, we can no longer insert values greater than prevMax
    if (topHint > 0 && topHint == topActual) {
        for (int i {prevMaxTop + 1}; i <= N; i++)
            invalidCandidates.insert(i);
    }
    if (leftHint > 0 && leftHint == leftActual) {
        for (int i {prevMaxLeft + 1}; i <= N; i++)
            invalidCandidates.insert(i);
    }

    // Eliminate invalid candidates based on hints
    for (int i {N - topHint + topActual + 2}; i <= N; i++)
        invalidCandidates.insert(i);
    for (int i {N - leftHint + leftActual + 2}; i <= N; i++)
        invalidCandidates.insert(i);

    // Return only valid candidates
    std::vector<int> validCandidates;
    for (int i {N}; i > 0; i--)
        if (invalidCandidates.find(i) == invalidCandidates.end())
        validCandidates.push_back(i);

    return validCandidates;
}

bool solve(std::vector<std::vector<int>> &grid, const std::array<std::vector<int>, 4> &hints, std::size_t row, std::size_t col) {
    std::size_t N {grid.size()}; 
    if (row == N) {
        return true;
    } else if (col == N) {
        return validateRight(grid, hints, row) && solve(grid, hints, row + 1, 0);
    } else if (grid[row][col] != 0) {
        bool checks {true};
        if (row == N - 1) {
            checks &= validateDown(grid, hints, col);
            checks &= validateUp(grid, hints, col);
        }
        return checks && solve(grid, hints, row, col + 1);
    } else {
        std::vector<int> candidates{generateCandidates(grid, hints, row, col)};
        while(!candidates.empty()) {
             int candidate {candidates.back()};
             candidates.pop_back();
             grid[row][col] = candidate;
             if ((row < N - 1 || validateDown(grid, hints, col)) && solve(grid, hints, row, col + 1))
                 return true;
        }

        grid[row][col] = 0;
        return false;
    }
}

int main(int argc, char** argv) {
    if (argc != 2 || (std::strcmp(argv[1], "generate") != 0 && std::strcmp(argv[1], "solve") != 0))
        std::cout << "Usage: `echo <N> | skyscrapers generate` (OR) `skyscrapers solve < <file.txt>`\n";

    else if (std::strcmp(argv[1], "generate") == 0) {
        std::size_t N;
        std::cin >> N;
        std::vector<std::vector<int>> puzzle(N, std::vector<int>(N));
        generate(puzzle, 0, 0);
        std::array<std::vector<int>, 4> hints {generateHints(puzzle)};
        std::cout << print(puzzle, hints, false);
    }

    else {
        auto [puzzle, hints] = read(std::cin);
        bool status {solve(puzzle, hints, 0, 0)};
        if (!status) { std::cerr << "Invalid board.\n"; return 1; }
        std::cout << print(puzzle, hints, true);
    }
}
