#include "ftxui/component/component_base.hpp"
#include <ftxui/component/screen_interactive.hpp>
#include "ftxui/component/event.hpp"
#include "ftxui/dom/node.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/screen/pixel.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/screen.hpp"
#include "ftxui/component/component.hpp"
#include <ftxui/dom/canvas.hpp>

#include <algorithm>
#include <queue>
#include <random>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

class Minesweeper {
private:
    struct hash_pair {
        inline std::size_t operator() (const std::pair<int, int>& p) const {
            return (p.first * 31) + p.second;
        }
    };

    std::vector<std::pair<int, int>> DIRS = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

    int countMines(std::size_t i, std::size_t j, std::unordered_set<std::pair<int, int>, hash_pair>& mines) {
        /*
         * Check all 8 directions if they are contained in the mines hashmap
         * We do not need the out of boundary check here since naturally
         * those cells would not be contained in the hashmap
         */
        int count = 0;
        for (auto [di, dj]: DIRS)
            if (mines.find({(int) i + di, (int) j + dj}) != mines.end())
                count++;
        return count;
    }

public:

    // Helper enum constants, types
    enum CELLSTATUS { HIDDEN = 0, FLAGGED = 1, REVEALED = 2};
    enum CELLTYPE { MINE=-1, EMPTY=0, ONE=1, TWO=2, THREE=3, FOUR=4, FIVE=5, SIX=6, SEVEN=7, EIGHT=8 };
    typedef std::pair<CELLSTATUS, CELLTYPE> CELL;
    typedef std::tuple<std::string> CELLDETAILS;

    std::size_t rows, cols;
    int mineCount = 0, hoverRow = 0, hoverCol = 0;
    bool gameOver = false;
    std::vector<std::vector<CELL>> grid;

    Minesweeper(std::size_t rows, std::size_t cols, int mc): rows(rows), cols(cols), mineCount(mc) {
        // Get all possible cells to randomly pick a subset
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
            grid.push_back(std::vector<CELL>());
            for (std::size_t j = 0; j < cols; j++) {
                if (mines.find({i, j}) != mines.end())
                    grid.back().push_back({CELLSTATUS::HIDDEN, MINE});
                else {
                    grid.back().push_back({CELLSTATUS::HIDDEN, (CELLTYPE) countMines(i, j, mines)});
                }
            }
        }
    };

    CELLDETAILS getCellDetails(std::size_t row, std::size_t col) {
        std::string text {" "};
        Minesweeper::CELL cell = grid[row][col];
        if (cell.first == Minesweeper::REVEALED || gameOver) {
            if (cell.second == Minesweeper::MINE)
                text = "X";
            else if (cell.second != Minesweeper::EMPTY)
                text = std::to_string(cell.second);
        } else if (cell.first == Minesweeper::FLAGGED) {
            text = "#";
        } 

        return {text};
    }

    void onMouseEvent(std::size_t row, std::size_t col, bool leftClicked, bool rightClicked, bool clickReleased) {
        if (!(0 <= row && row < rows && 0 <= col && col < cols) || !clickReleased) {
            return;
        } else if (rightClicked && grid[row][col].first != Minesweeper::REVEALED) {
            grid[row][col].first = grid[row][col].first == Minesweeper::HIDDEN? Minesweeper::FLAGGED: Minesweeper::HIDDEN;
        } else if (leftClicked && grid[row][col].first == Minesweeper::HIDDEN) {
            if (grid[row][col].second == Minesweeper::MINE) {
                gameOver = true;
            } else if (grid[row][col].second != Minesweeper::EMPTY) {
                grid[row][col].first = Minesweeper::REVEALED;
            } else {
                std::queue<std::pair<int, int>> queue({{row, col}});
                grid[row][col].first = Minesweeper::REVEALED;
                while (!queue.empty()) {
                    auto [x, y] = queue.front();
                    queue.pop();
                    for (auto [dx, dy]: DIRS) {
                        int x_ = x + dx, y_ = y + dy;
                        if (0 <= x_ && x_ < grid.size() && 0 <= y_ && y_ < grid[0].size()) {
                            if (grid[x_][y_].first != Minesweeper::REVEALED && grid[x_][y_].second != Minesweeper::MINE) {
                                grid[x_][y_].first = Minesweeper::REVEALED;
                                if (grid[x_][y_].second == Minesweeper::EMPTY)
                                    queue.push({x_, y_});
                            }
                        }
                    }
                }
            }
        }
    }

};

ftxui::Canvas gridToCanvas(Minesweeper& game) {
    /*
     * From the game grid we convert into a FTXUI canvas object
     */
    std::size_t rows = game.grid.size(), cols = game.grid[0].size();
    ftxui::Canvas canvas = ftxui::Canvas(rows * 8, cols * 8);
    for (std::size_t r = 0; r < rows; r++) {
        for (std::size_t c = 0; c < cols; c++) {
            Minesweeper::CELLDETAILS details = game.getCellDetails(r, c);
            canvas.DrawText(r * 8, c * 8, std::get<0>(details), [&] (ftxui::Pixel& p) { 
                p.foreground_color = ftxui::Color::Red;
                if (r == game.hoverRow && c == game.hoverCol)
                    p.background_color = ftxui::Color::White;
            });
        }
    }

    return canvas;
}

int main() {
    int ROWS = 10, COLS = 10, MINES = 15;
    Minesweeper game(ROWS, COLS, MINES);

    auto screen = ftxui::ScreenInteractive::FitComponent();

    auto gameBoard = ftxui::Renderer([&] { return ftxui::canvas(gridToCanvas(game)) | ftxui::border; });
    gameBoard = ftxui::CatchEvent(gameBoard, [&] (ftxui::Event event) {
        if (event.is_mouse()) {
            auto &mouse = event.mouse();
            game.onMouseEvent(mouse.x, mouse.y, mouse.button == ftxui::Mouse::Left, mouse.button == ftxui::Mouse::Right, mouse.motion == ftxui::Mouse::Motion::Released);
            if (0 <= mouse.x && mouse.x < game.cols && 0 <= mouse.y && mouse.y < game.rows) {
                game.hoverRow = (int) mouse.x;
                game.hoverCol = (int) mouse.y;
            }
        } else if (event == ftxui::Event::Escape || event.input() == "q") {
           screen.ExitLoopClosure()();
        }
        return false;
    });

    auto gameRenderer = ftxui::Renderer(gameBoard, [&] {
        return ftxui::vbox({
            ftxui::center(ftxui::text("Minesweeper")),
            ftxui::separator(),
            gameBoard->Render()
        }) | ftxui::border;
    });

    screen.Loop(gameRenderer);
    return 0;
}
