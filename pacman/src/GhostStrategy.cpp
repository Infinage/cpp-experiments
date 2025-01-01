#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <tuple>
#include <vector>

#include "../include/GhostStrategy.hpp"
#include "../include/Utils.hpp"

void Strategy::setGhost(Ghost* ghost) {
    this->ghost = ghost;
}

std::vector<std::tuple<std::size_t, std::size_t, DIRS>> Strategy::getNeighbouringCells(MAP &map) {
    // Insert dir in order of priority - UP, LEFT, DOWN
    auto [cx, cy] {ghost->getPosition()};
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

DIRS Shadow::getNext(MAP &map, Pacman &pacman, GHOSTS&) {
    std::pair<std::size_t, std::size_t> targetTile {pacman.getPosition()};
    DIRS currDir {ghost->getDir()}; DIRS minDir{revDirs.at(currDir)}; 
    double minDist {std::numeric_limits<double>::max()};
    for (auto [cx, cy, dir]: getNeighbouringCells(map)) {
        double dist {eDist({cx, cy}, targetTile)};
        if (dist < minDist && dir != revDirs.at(currDir)) {
            minDist = dist;
            minDir = dir;
        }
    }

    return minDir;
}

DIRS Ambush::getNext(MAP &map, Pacman &pacman, GHOSTS&) {
    // Project pacman direction by 4 steps
    auto [tx, ty] {pacman.getPosition()};
    auto [nx, ny] {travel(tx, ty, pacman.getDir(), 4)};

    DIRS currDir {ghost->getDir()}; DIRS minDir{revDirs.at(currDir)}; 
    double minDist {std::numeric_limits<double>::max()};
    for (auto [cx, cy, dir]: getNeighbouringCells(map)) {
        double dist {eDist({cx, cy}, {nx, ny})};
        if (dist < minDist && dir != revDirs.at(currDir)) {
            minDist = dist;
            minDir = dir;
        }
    }

    return minDir;
}

DIRS Fright::getNext(MAP &map, Pacman&, GHOSTS&) {
    // Random dir at each intersection
    std::vector<std::tuple<std::size_t, std::size_t, DIRS>> neighbours {getNeighbouringCells(map)};
    if (neighbours.empty()) return ghost->getDir();
    else {
        std::uniform_int_distribution<std::size_t> dist(0, neighbours.size() - 1);
        std::size_t rand_idx {dist(RANDOM_GEN)};
        return std::get<2>(neighbours[rand_idx]);
    }
}

Scatter::Scatter(const std::pair<std::size_t, std::size_t> &target): targetTile(target) {}

DIRS Scatter::getNext(MAP &map, Pacman&, GHOSTS&) {
    // Fixed target
    auto [nx, ny] {targetTile};

    DIRS currDir {ghost->getDir()}; DIRS minDir{revDirs.at(currDir)}; 
    double minDist {std::numeric_limits<double>::max()};
    for (auto [cx, cy, dir]: getNeighbouringCells(map)) {
        double dist {eDist({cx, cy}, {nx, ny})};
        if (dist < minDist && dir != revDirs.at(currDir)) {
            minDist = dist;
            minDir = dir;
        }
    }

    // DEBUG
    if (ny == MAP_HEIGHT - 1) {
        std::cout << "Dist: " << minDist << "; Dir: " << minDir << "\n";
    }

    return minDir;
}

Fickle::Fickle(const std::pair<std::size_t, std::size_t> &target): targetTile(target) {}

DIRS Fickle::getNext(MAP &map, Pacman &pacman, GHOSTS&) {
    // Get fixed target, pacman dist and ghost's distance
    auto [tx, ty] {targetTile};
    auto [px, py] {pacman.getPosition()};
    auto [gx, gy] {ghost->getPosition()};

    // Compute distance from pacman and decide accordingly
    double dist {static_cast<double>(mDist({gx, gy}, {px, py}))};
    std::size_t nx, ny;
    if (dist <= 8) { nx = tx; ny = ty; } 
    else { nx = px; ny = py; }

    // Compute the best dir to travel in
    DIRS currDir {ghost->getDir()}; DIRS minDir{revDirs.at(currDir)}; 
    double minDist {std::numeric_limits<double>::max()};
    for (auto [cx, cy, dir]: getNeighbouringCells(map)) {
        dist = eDist({cx, cy}, {nx, ny});
        if (dist < minDist && dir != revDirs.at(currDir)) {
            minDist = dist;
            minDir = dir;
        }
    }

    return minDir;
}
