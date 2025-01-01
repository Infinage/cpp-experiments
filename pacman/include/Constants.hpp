#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <utility>

// Forward decl
class Ghost;

constexpr int MAP_HEIGHT {21};
constexpr int MAP_WIDTH {19};
constexpr int CELL_SIZE {30};

constexpr float PACMAN_POWER_UP_TIME {5.f};
constexpr float GHOST_SCATTER_TIME {7.f};
constexpr float GHOST_CHASE_TIME {20.f};
constexpr float GHOST_SPEED {0.75f};
constexpr float GHOST_FRIGHTENED_SPEED {0.5f};

// ****************** MOSTLY REMAINS UNCHANGED ****************** //

constexpr const char* FRIGHTENED_GHOST_SPRITE_FILE {"assets/frightened-ghost.png"};
constexpr int FRIGHTENED_GHOST_SPRITE_ROWS {1};
constexpr int FRIGHTENED_GHOST_SPRITE_COLS {2};

constexpr const char* PACMAN_SPRITE_FILE {"assets/pacman.png"};

constexpr const char* BLINKY_SPRITE_FILE {"assets/blinky.png"};
constexpr const char* PINKY_SPRITE_FILE {"assets/pinky.png"};
constexpr const char* INKY_SPRITE_FILE {"assets/inky.png"};
constexpr const char* CLYDE_SPRITE_FILE {"assets/clyde.png"};

constexpr const std::pair<std::size_t, std::size_t> BLINKY_SCATTER_TARGET {std::make_pair(MAP_WIDTH - 1, 0)};
constexpr const std::pair<std::size_t, std::size_t> PINKY_SCATTER_TARGET {std::make_pair(0, 0)};
constexpr const std::pair<std::size_t, std::size_t> INKY_SCATTER_TARGET {std::make_pair(MAP_WIDTH - 1, MAP_HEIGHT - 1)};
constexpr const std::pair<std::size_t, std::size_t> CLYDE_SCATTER_TARGET {std::make_pair(0, MAP_HEIGHT - 1)};

constexpr const char* FOOD_SPRITE_FILE {"assets/food.png"};
constexpr const char* WALL_SPRITE_FILE {"assets/wall.png"};

enum DIRS {UP=0, RIGHT=1, DOWN=2, LEFT=3}; 
enum CELL {WALL, PELLET, ENERGIZER, EMPTY};

using MAP = std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT>;
using GHOSTS = std::array<Ghost*, 3>;

const std::unordered_map<DIRS, DIRS> revDirs {
    {  DIRS::UP,  DIRS::DOWN}, { DIRS::DOWN,   DIRS::UP}, 
    {DIRS::LEFT, DIRS::RIGHT}, {DIRS::RIGHT, DIRS::LEFT}
};

const std::array<std::string, MAP_HEIGHT> sketch {{
    "###################",
    "#........#........#",
    "#O##.###.#.###.##O#",
    "#.................#",
    "#.##.#.#####.#.##.#",
    "#....#...#...#....#",
    "#.##.### # ###.##.#",
    "#.##.#   0   #.##.#",
    "#.##.# ##=## #.##.#",
    "#   .  #123#  .   #",
    "## #.# ##### #.# ##",
    " # #.#       #.# # ",
    "## #.# ##### #.# ##",
    "#........#........#",
    "#.##.###.#.###.##.#",
    "#O.#.....P.....#.O#",
    "##.#.#.#####.#.#.##",
    "#....#...#...#....#",
    "#.######.#.######.#",
    "#.................#",
    "###################"
}};
