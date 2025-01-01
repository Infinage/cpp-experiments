#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <cmath>
#include <utility>

#include "Constants.hpp"
#include "Sprites.hpp"

std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> getMap();
bool checkNoWallCollision(float i, float j, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map);
sf::Vector2f snap2Grid(const sf::Vector2f &pos, float alignTol);
double eDist(const std::pair<std::size_t, std::size_t> &curr, const std::pair<std::size_t, std::size_t> &target);
std::pair<std::size_t, std::size_t> travel(std::size_t cx, std::size_t cy, DIRS dir, int steps = 1);
bool renderWorld(
    std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, sf::RenderWindow &window, 
    Pacman &pacman, Ghost &blinky, Ghost &pinky, Wall &wall, Food &food
);

template <typename T>
T floor(float val) { return static_cast<T>(std::floor(val)); }

template <typename T> 
T ceil(float val) { return static_cast<T>( std::ceil(val)); }

template <typename T> 
T round(float val) { return static_cast<T>(std::round(val)); }
