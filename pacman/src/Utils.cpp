#include "../include/Utils.hpp"

std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> getMap() {
    std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> result;
    for (std::size_t i {0}; i < MAP_HEIGHT; i++) {
        for (std::size_t j {0}; j < MAP_WIDTH; j++) {
            switch (sketch[i][j]) {
                case '#':
                    result[i][j] = CELL::WALL;
                    break;
                case '.':
                    result[i][j] = CELL::PELLET;
                    break;
                case 'O':
                    result[i][j] = CELL::ENERGIZER;
                    break;
                default:
                    result[i][j] = CELL::EMPTY;
            }
        }
    }

    return result;
}

bool renderWorld(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, sf::RenderWindow &window, Pacman &pacman, Wall &wall, Food &food) {
    bool pelletExists {false};
    for (std::size_t i {0}; i < MAP_HEIGHT; i++) {
        for (std::size_t j {0}; j < MAP_WIDTH; j++) {
            if (map[i][j] == CELL::WALL) {
                wall.setPosition(i, j);
                wall.draw(window);
            } else if (map[i][j] == CELL::PELLET) {
                pelletExists = true;
                food.setPosition(i, j);
                food.setTextureRect({0, 0, static_cast<int>(food.getWidth()), static_cast<int>(food.getHeight())});
                food.draw(window);
            } else if (map[i][j] == CELL::ENERGIZER) {
                food.setPosition(i, j);
                food.setTextureRect({static_cast<int>(food.getWidth()), 0, static_cast<int>(food.getWidth()), static_cast<int>(food.getHeight())});
                food.draw(window);
            }
        }
    }

    pacman.draw(window);
    return pelletExists;
}
