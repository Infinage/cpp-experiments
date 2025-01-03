#include <algorithm>
#include <array>
#include <iostream>
#include <random>
#include <sstream>
#include <unordered_set>
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

    std::vector<int> top(N, 0); 
    for (std::size_t j {0}; j < N; j++) {
        int prevMax {0};
        for (std::size_t i {0}; i < N; i++) {
            if (grid[i][j] > prevMax) {
                prevMax = grid[i][j];
                top[j]++; 
            }
        }
    }

    std::vector<int> left(N, 0); 
    for (std::size_t i {0}; i < N; i++) {
        int prevMax {0};
        for (std::size_t j {0}; j < N; j++) {
            if (grid[i][j] > prevMax) {
                prevMax = grid[i][j];
                left[i]++; 
            }
        }
    }

    std::vector<int> down(N, 0); 
    for (std::size_t j {0}; j < N; j++) {
        int prevMax {0};
        for (std::size_t i {N}; i-- > 0;) {
            if (grid[i][j] > prevMax) {
                prevMax = grid[i][j];
                down[j]++; 
            }
        }
    }

    std::vector<int> right(N, 0); 
    for (std::size_t i {0}; i < N; i++) {
        int prevMax {0};
        for (std::size_t j {N}; j-- > 0;) {
            if (grid[i][j] > prevMax) {
                prevMax = grid[i][j];
                right[j]++; 
            }
        }
    }

    return {top, right, down, left};
}


int main() {
    std::size_t N;
    std::cin >> N;

    std::vector<std::vector<int>> puzzle(N, std::vector<int>(N));
    bool status {generate(puzzle, 0, 0)};
    auto [top, right, down, left] {generateHints(puzzle)};

    // Print the puzzle
    std::ostringstream oss;

    oss << "    | ";
    for (int i: top)
        oss << i << " | ";

    oss << "   \n" + std::string(10 + (N * 4), '-') + "\n";

    for (std::size_t i {0}; i < N; i++) {
        oss << left[i] << " > | ";
        for (std::size_t j {0}; j < N; j++)
            oss << puzzle[i][j] << " | ";
        oss << "< " << right[i] << "\n";
    }

    oss << std::string(10 + (N * 4), '-') + "\n    | ";

    for (int i: down)
        oss << i << " | ";
    oss << "   \n";

    // Write to console
    std::cout << oss.str();
}
