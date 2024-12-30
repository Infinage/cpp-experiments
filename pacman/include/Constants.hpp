#pragma once

#include <array>
#include <string>

constexpr int MAP_HEIGHT {21};
constexpr int MAP_WIDTH {19};
constexpr int CELL_SIZE {30};
constexpr const char* PACMAN_SPRITE_FILE {"assets/pacman.png"};
constexpr const char* FOOD_SPRITE_FILE {"assets/food.png"};
constexpr const char* WALL_SPRITE_FILE {"assets/wall.png"};
enum DIRS {UP=0, RIGHT=1, DOWN=2, LEFT=3}; 
enum CELL {WALL, PELLET, ENERGIZER, EMPTY};

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
