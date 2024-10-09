#include "ftxui/component/component_base.hpp"
#include <cassert>
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
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>
#include <iostream>

class Minesweeper {
private:
    struct hash_pair {
        /*
         * When using pairs inside unordered_set, we need to provide a custom hash func
         */
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
    enum CELLTYPE { MINE=-1, EMPTY=0, ONE=1, TWO=2, THREE=3, FOUR=4, FIVE=5, SIX=6, SEVEN=7, EIGHT=8 };
    enum CELLSTATUS { HIDDEN, FLAGGED, REVEALED };
    enum COLOR  { RED, WHITE, GREEN, LIGHTGRAY, BLACK, DARKRED, DARKGRAY };
    enum GAMESTATUS { INPROGESS, WON, LOST };
    typedef std::pair<CELLSTATUS, CELLTYPE> CELL;
    typedef std::tuple<std::string, COLOR, COLOR> CELLDETAILS;

    // Variables
    std::size_t rows, cols;
    int mineCount = 0, safeCells = 0, hoverRow = 0, hoverCol = 0;
    int gameStatus = INPROGESS;
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
        safeCells = (int) (rows * cols) - mineCount;

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
        /*
         * Given row and col, get the config that would get passed into ftxui drawText()
         */
        std::string text {" "};
        Minesweeper::COLOR bgcolor { Minesweeper::COLOR::BLACK }, fgcolor { Minesweeper::COLOR::WHITE };
        Minesweeper::CELL cell = grid[row][col];

        if (cell.first == Minesweeper::REVEALED || gameStatus != Minesweeper::GAMESTATUS::INPROGESS) {
            if (cell.second == Minesweeper::MINE) {
                text = "X";
                bgcolor = Minesweeper::COLOR::DARKRED;
            } else if (cell.second != Minesweeper::EMPTY) {
                text = std::to_string(cell.second);
                bgcolor = Minesweeper::COLOR::LIGHTGRAY;
                fgcolor = Minesweeper::COLOR::BLACK;
            }  else {
                bgcolor = Minesweeper::COLOR::LIGHTGRAY;
            }
        } else if (cell.first == Minesweeper::FLAGGED) {
            text = "#";
            bgcolor = Minesweeper::COLOR::RED;
        } 

        // We display the cell being highlighted a little differently
        if (row == hoverRow && col == hoverCol) {
            bgcolor =  Minesweeper::COLOR::DARKGRAY;
            fgcolor =  Minesweeper::COLOR::WHITE;
        }

        return {text, fgcolor, bgcolor};
    }

    void onMouseEvent(std::size_t row, std::size_t col, bool leftClicked, bool rightClicked, bool clickReleased) {
        /*
         * When user clicks a cell, based on the click and cell type we modify the game state here
         */

        if (!(0 <= row && row < rows && 0 <= col && col < cols) || !clickReleased) {
            return;
        } else if (rightClicked && grid[row][col].first != Minesweeper::REVEALED) {
            // Toggleable right click
            grid[row][col].first = grid[row][col].first == Minesweeper::HIDDEN? Minesweeper::FLAGGED: Minesweeper::HIDDEN;
        } else if (leftClicked && grid[row][col].first == Minesweeper::HIDDEN) {
            if (grid[row][col].second == Minesweeper::MINE) {
                gameStatus = Minesweeper::GAMESTATUS::LOST;
            } else if (grid[row][col].second != Minesweeper::EMPTY) {
                grid[row][col].first = Minesweeper::REVEALED;
                safeCells--;
            } else {
                // When we click an empty cell, do a BFS traversal and 
                // highlight all neighbours until we hit a non empty cell
                // Obviously skip revealing the mine cells
                std::queue<std::pair<int, int>> queue({{row, col}});
                grid[row][col].first = Minesweeper::REVEALED;
                safeCells--;
                while (!queue.empty()) {
                    auto [x, y] = queue.front();
                    queue.pop();
                    for (auto [dx, dy]: DIRS) {
                        int x_ = x + dx, y_ = y + dy;
                        if (0 <= x_ && x_ < grid.size() && 0 <= y_ && y_ < grid[0].size()) {
                            if (grid[x_][y_].first != Minesweeper::REVEALED && grid[x_][y_].second != Minesweeper::MINE) {
                                grid[x_][y_].first = Minesweeper::REVEALED;
                                safeCells--;
                                if (grid[x_][y_].second == Minesweeper::EMPTY)
                                    queue.push({x_, y_});
                            }
                        }
                    }
                }
            }
        }

        if (safeCells == 0)
            gameStatus = Minesweeper::GAMESTATUS::WON;
    }

};

ftxui::Color mapColor(Minesweeper::COLOR color) {
    /*
     * Mapping minesweeper enum class to FTUI 
     * Good practice and for decoupling
     */
    if (color == Minesweeper::COLOR::RED)
        return ftxui::Color::Red;
    else if (color == Minesweeper::COLOR::DARKRED) 
        return ftxui::Color::DarkRed;
    else if (color == Minesweeper::COLOR::BLACK)
        return ftxui::Color::Black;
    else if (color == Minesweeper::COLOR::LIGHTGRAY)
        return ftxui::Color::GrayLight;
    else if (color == Minesweeper::COLOR::GREEN) 
        return ftxui::Color::Green;
    else if (color == Minesweeper::COLOR::DARKGRAY)
        return ftxui::Color::GrayDark;
    else
        return ftxui::Color::White;
}

ftxui::Canvas gridToCanvas(Minesweeper& game, int mul) {
    /*
     * From the game grid we convert into a FTXUI canvas object
     */

    // Odd because we assign the actual text at the center only
    // there is no "center" for a grid having even num of rows / cols
    assert(mul % 3 == 0);

    // Constants picked from another GH repo, requires trial and error
    const int yMul = mul * 4, xMul = mul * 2;
    std::size_t rows = game.grid.size(), cols = game.grid[0].size();
    ftxui::Canvas canvas = ftxui::Canvas((int)cols * xMul, (int)rows * yMul);
    for (int r = 0; r < (int)rows; r++) {
        for (int c = 0; c < (int)cols; c++) {
            Minesweeper::CELLDETAILS details = game.getCellDetails(r, c);
            int startY = r * yMul, startX = c * xMul;
            int centerY = startY + (yMul / 2), centerX = startX + (xMul / 2);
            // Draw background block
            for (int x = startX; x < startX + xMul; x++) {
                for (int y = startY; y < startY + yMul; y++) {
                    canvas.DrawText(x, y, " ", [&] (ftxui::Pixel& p) { 
                        p.foreground_color = mapColor(std::get<1>(details));
                        p.background_color = mapColor(std::get<2>(details));
                    });
                }
            }

            // Draw the text at the center
            canvas.DrawText(centerX, centerY, std::get<0>(details), [&] (ftxui::Pixel& p) { 
                p.foreground_color = mapColor(std::get<1>(details));
                p.background_color = mapColor(std::get<2>(details));
            });
        }
    }

    return canvas;
}

int main(int argc, char* argv[]) {

    // <mines> optional, rows and cols are required
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: ./minesweeper <rows> <cols> [<mines>]\n";

    } else {
        // Input Parameters
        int ROWS = std::stoi(argv[1]);
        int COLS = std::stoi(argv[2]);
        int MINES = argc == 3? (ROWS * COLS) / 10: std::stoi(argv[3]);
        Minesweeper game(ROWS, COLS, MINES);

        auto screen = ftxui::ScreenInteractive::FitComponent();                         // App would take up the max size taken up by the container
        screen.SetCursor(ftxui::Screen::Cursor(0, 0, ftxui::Screen::Cursor::Hidden));   // Hide cursor

        // *WARNING* - Based on this mul, the mouse positions would need to be fine tuned
        int CANVAS_MUL = 3;

        auto gameBoard = ftxui::Renderer([&] { return ftxui::canvas(gridToCanvas(game, CANVAS_MUL)) | ftxui::border; });
        gameBoard = ftxui::CatchEvent(gameBoard, [&] (ftxui::Event event) {
            // Ignore all events except from mouse
            if (event.is_mouse()) {
                auto &mouse = event.mouse();
                int mouseX = (mouse.x / CANVAS_MUL) - 1, mouseY = (mouse.y / CANVAS_MUL) - 1;
                game.onMouseEvent(mouseY, mouseX, mouse.button == ftxui::Mouse::Left, mouse.button == ftxui::Mouse::Right, mouse.motion == ftxui::Mouse::Motion::Released);
                game.hoverRow = mouseY;
                game.hoverCol = mouseX;
            }

            // Quit game with a 'q' or an 'Esc'
            else if (event == ftxui::Event::Escape || event.input() == "q") {
               screen.ExitLoopClosure()();
               return true;
            }

            // Check game status and close if required
            if (game.gameStatus != Minesweeper::GAMESTATUS::INPROGESS) {
               screen.ExitLoopClosure()();
               return true;
            }

            return false;
        });

        auto gameRenderer = ftxui::Renderer(gameBoard, [&] {
            return ftxui::vbox({
                ftxui::center(ftxui::text(
                    game.gameStatus == Minesweeper::GAMESTATUS::INPROGESS?
                        "Minesweeper💥":
                        game.gameStatus == Minesweeper::GAMESTATUS::WON?
                            "Victory!🤩":
                            "Game Lost!😵"
                )),
                ftxui::separator(),
                gameBoard->Render()
            }) | ftxui::border;
        });

        screen.Loop(gameRenderer);
    }

    return 0;
}
