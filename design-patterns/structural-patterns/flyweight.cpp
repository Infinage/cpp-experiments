#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using CANVAS_T =  std::vector<std::vector<std::string>>;

class TreeType {
    public:
        enum COLORS {WHITE, RED, GREEN, YELLOW, BLUE};
        struct HashTreeType {
            inline std::size_t operator() (const std::pair<char, COLORS> &p) const {
                return static_cast<std::size_t>(p.first * 31 + p.second);
            }
        };

    private:
        char rep;
        COLORS color;

    public:
        TreeType(char representation, COLORS color):
            rep(representation), color(color) {}

        void draw(std::size_t x, std::size_t y, CANVAS_T &canvas) {
            std::string coloredRep;
            switch (color) {
                case WHITE:  coloredRep = "\033[37m" + std::string(1, rep) + "\033[0m"; break;
                case RED:    coloredRep = "\033[31m" + std::string(1, rep) + "\033[0m"; break;
                case GREEN:  coloredRep = "\033[32m" + std::string(1, rep) + "\033[0m"; break;
                case YELLOW: coloredRep = "\033[33m" + std::string(1, rep) + "\033[0m"; break;
                case BLUE:   coloredRep = "\033[34m" + std::string(1, rep) + "\033[0m"; break;
            }
            canvas[x][y] = coloredRep;
        }
};

class TreeFactory {
    public:
        static std::unordered_map<std::pair<char, TreeType::COLORS>, std::shared_ptr<TreeType>, TreeType::HashTreeType> treeTypes;

        static std::shared_ptr<TreeType> getTreeType(char rep, TreeType::COLORS color) {
            if (treeTypes.find({rep, color}) == treeTypes.end())
                treeTypes[{rep, color}] = std::make_shared<TreeType>(rep, color);
            return treeTypes[{rep, color}];
        }
};

class Tree {
    private:
        std::size_t x, y;
        std::shared_ptr<TreeType> type;

    public:
        Tree(std::size_t x, std::size_t y, std::shared_ptr<TreeType> type): x(x), y(y), type(type) {}
        void draw(CANVAS_T &canvas) { type->draw(x, y, canvas); }
};

class Forest {
    private:
        CANVAS_T canvas;
        std::size_t rows, cols;
        std::vector<Tree> trees;

    public:
        Forest(std::size_t rows = 10, std::size_t cols = 10): rows(rows), cols(cols) {
            canvas = CANVAS_T(rows, std::vector<std::string>(cols));
        }

        void plantTree(std::size_t row, std::size_t col, char rep, TreeType::COLORS color) {
            if (row >= rows || col >= cols) throw "Out of bounds.";
            else trees.push_back(Tree{row, col, TreeFactory::getTreeType(rep, color)});
        }

        void resetCanvas() {
			for (std::size_t i{0}; i < rows; i++)
                for (std::size_t j{0}; j < cols; j++)
                    canvas[i][j] = " ";
        }

        void drawTrees() { for (Tree &t: trees) t.draw(canvas); }

        void draw() {
            // Reset Canvas
            resetCanvas();

            // Draw trees present
            drawTrees();

            // Print the forest to screen
            for (std::size_t i{0}; i < rows; i++) {
                for (std::size_t j{0}; j < cols; j++)
                    std::cout << canvas[i][j] << " ";
                std::cout << "\n";
            }
        }
};

// Init TreeFactory
std::unordered_map<std::pair<char, TreeType::COLORS>, std::shared_ptr<TreeType>, TreeType::HashTreeType> 
    TreeFactory::treeTypes;

int main() {
    // Config variables
    std::size_t rows {50}, cols {100};
    double coverage {0.75};

    // Init the forest
    Forest forest{rows, cols};

    // Random picking - Config
    std::vector<std::size_t> indices(rows * cols);
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 gen { std::random_device{}() };
    std::shuffle(indices.begin(), indices.end(), gen);

    std::vector<char> reps {{'#', '^', '@', 'T', '*', '+', '!'}};
    std::vector<TreeType::COLORS> colors {{
        TreeType::COLORS::WHITE, TreeType::COLORS::RED, TreeType::COLORS::GREEN,
        TreeType::COLORS::YELLOW, TreeType::COLORS::BLUE
    }};

    std::uniform_int_distribution<std::size_t>
        repsDist(0, reps.size()), 
        colorsDist(0, colors.size());   

    auto randomPick {[&gen]<typename T>(std::vector<T> &vec, std::uniform_int_distribution<size_t> &dist) -> T { 
        return vec[dist(gen)]; 
    }};

    // Random picking
    std::size_t toPick {static_cast<std::size_t>(static_cast<float>(indices.size()) * coverage)};
    for (std::size_t i {0}; i < toPick; i++) {
        std::size_t idx {indices[i]};
        forest.plantTree(idx / cols, idx % cols, randomPick(reps, repsDist), randomPick(colors, colorsDist));
    }

    // Draw the forest to screen
    forest.draw();

    return 0;
}
