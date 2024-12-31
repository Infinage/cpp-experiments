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

bool renderWorld(
    std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, sf::RenderWindow &window, 
    Pacman &pacman, Ghost &blinky, Ghost &pinky, Wall &wall, Food &food
) {
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
    blinky.draw(window);
    pinky.draw(window);
    return pelletExists;
}

bool checkNoWallCollision(float i, float j, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map) {  
    if (i < 0 || j < 0 || floor(i / CELL_SIZE) > MAP_HEIGHT || floor(j / CELL_SIZE) > MAP_WIDTH) {
        return CELL::WALL;
    } else {
        std::size_t i_floor {floor<std::size_t>(i / CELL_SIZE)}, j_floor {floor<std::size_t>(j / CELL_SIZE)};
        std::size_t  i_ceil { ceil<std::size_t>(i / CELL_SIZE)},  j_ceil { ceil<std::size_t>(j / CELL_SIZE)};
        CELL TL {map[i_floor][j_floor]}, TR {map[i_floor][j_ceil]};
        CELL BL {map[i_ceil][j_floor]}, BR {map[i_ceil][j_ceil]};
        return TL != CELL::WALL && TR != CELL::WALL && BL != CELL::WALL && BR != CELL::WALL; 
    }
}

sf::Vector2f snap2Grid(sf::Vector2f pos, float alignTol) {
    float topLeftX {std::round(pos.x / CELL_SIZE) * CELL_SIZE};
    float topLeftY {std::round(pos.y / CELL_SIZE) * CELL_SIZE};

    if (std::abs(pos.x - topLeftX) / CELL_SIZE <= alignTol)
        pos.x = topLeftX;
    if (std::abs(pos.y - topLeftY) / CELL_SIZE <= alignTol)
        pos.y = topLeftY;

    return pos;
}

std::pair<std::size_t, std::size_t> travel(std::size_t cx, std::size_t cy, DIRS dir, int steps) {
    // Which direction to travel?
    int dx {0}, dy {0};
    switch (dir) {
        case    DIRS::UP: dy = -steps; break;
        case  DIRS::DOWN: dy = +steps; break;
        case  DIRS::LEFT: dx = -steps; break;
        case DIRS::RIGHT: dx = +steps; break;
    }

    // Cast for comparison & ops
    std::size_t dx_ {dx < 0? static_cast<std::size_t>(-dx): static_cast<std::size_t>(dx)};
    std::size_t dy_ {dy < 0? static_cast<std::size_t>(-dy): static_cast<std::size_t>(dy)};
    std::size_t WIDTH {static_cast<std::size_t>(MAP_WIDTH)}, HEIGHT {static_cast<std::size_t>(MAP_HEIGHT)};

    // Clamp CX
    if (dx < 0) cx = dx_ > cx? 0: cx - dx_;
    else cx = std::min(WIDTH, cx + dx_);

    // Clamp CY
    if (dy < 0) cy = dy_ > cy? 0: cy - dy_;
    else cy = std::min(HEIGHT, cy + dy_);

    return {cx, cy};
}

double eDist(const std::pair<std::size_t, std::size_t> &curr, const std::pair<std::size_t, std::size_t> &target) {
    int dx { static_cast<int>(curr.first) -  static_cast<int>(target.first)};
    int dy {static_cast<int>(curr.second) - static_cast<int>(target.second)};
    return std::sqrt(dx * dx + dy * dy); 
}
