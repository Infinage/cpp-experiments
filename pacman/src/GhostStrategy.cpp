#include <cmath>
#include <iostream>
#include <limits>
#include <tuple>
#include <vector>

#include "../include/GhostStrategy.hpp"
#include "../include/Utils.hpp"

Strategy::Strategy(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, Ghost &ghost)
    : ghost(ghost), map(map) {}

std::vector<std::tuple<std::size_t, std::size_t, DIRS>> Strategy::getNeighbouringCells() {
    // Insert dir in order of priority - UP, LEFT, DOWN
    auto [cx, cy] {ghost.getPosition()};
    std::vector<std::tuple<std::size_t, std::size_t, DIRS>> dirs;

    if (cy > 0 && map[cy - 1][cx] != CELL::WALL)
        dirs.push_back({cx, cy - 1, DIRS::UP});

    if (cx > 0 && map[cy][cx - 1] != CELL::WALL)
        dirs.push_back({cx - 1, cy, DIRS::LEFT});

    if (cy < MAP_HEIGHT - 1 && map[cy + 1][cx] != CELL::WALL)
        dirs.push_back({cx, cy + 1, DIRS::DOWN});

    if (cx < MAP_WIDTH - 1 && map[cy][cx + 1] != CELL::WALL)
        dirs.push_back({cx + 1, cy, DIRS::RIGHT});

    return dirs;
}

Shadow::Shadow(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, Ghost &ghost, Pacman &pacman)
    : Strategy(map, ghost), pacman(pacman) {}

DIRS Shadow::getNext() {
    std::pair<std::size_t, std::size_t> targetTile {pacman.getPosition()};
    DIRS currDir {ghost.getDir()}, minDir{revDirs.at(ghost.getDir())}; 
    double minDist {std::numeric_limits<double>::max()};
    for (auto [cx, cy, dir]: getNeighbouringCells()) {
        double dist {eDist({cx, cy}, targetTile)};
        if (dist < minDist && dir != revDirs.at(currDir)) {
            minDist = dist;
            minDir = dir;
        }
    }

    // DEBUG
    std::pair<std::size_t, std::size_t> ghostPos {ghost.getPosition()};
    std::cout << "Target: " << targetTile.first << ", " << targetTile.second << "\n";
    std::cout << "Ghost: " << ghostPos.first << ", " << ghostPos.second << "\n";
    std::cout << "Dir chosen: " << minDir << ", Dist: " << minDist <<  "\n\n";

    return minDir;
}
