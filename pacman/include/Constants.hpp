#pragma once

#include <array>
#include <string>
#include <unordered_map>

constexpr int MAP_HEIGHT {21};
constexpr int MAP_WIDTH {19};
constexpr int CELL_SIZE {30};
constexpr const char* PACMAN_SPRITE_FILE {"assets/pacman.png"};
constexpr const char* BLINKY_SPRITE_FILE {"assets/blinky.png"};
constexpr const char* FOOD_SPRITE_FILE {"assets/food.png"};
constexpr const char* WALL_SPRITE_FILE {"assets/wall.png"};
enum DIRS {UP=0, RIGHT=1, DOWN=2, LEFT=3}; 
enum CELL {WALL, PELLET, ENERGIZER, EMPTY};

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
    "####.### # ###.####",
    "   #.#   0   #.#   ",
    "####.# ##=## #.####",
    "    .  #123#  .    ",
    "####.# ##### #.####",
    "   #.#       #.#   ",
    "####.# ##### #.####",
    "#........#........#",
    "#.##.###.#.###.##.#",
    "#O.#.....P.....#.O#",
    "##.#.#.#####.#.#.##",
    "#....#...#...#....#",
    "#.######.#.######.#",
    "#.................#",
    "###################"
}};
