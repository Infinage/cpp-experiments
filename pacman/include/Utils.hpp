#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

#include "Constants.hpp"
#include "Sprites.hpp"

std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> getMap();
bool renderWorld(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, sf::RenderWindow &window, Pacman &pacman, Wall &wall, Food &food);
