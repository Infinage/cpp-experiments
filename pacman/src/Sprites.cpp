#include <SFML/Window/Keyboard.hpp>
#include <cmath>
#include <functional>

#include "../include/Sprites.hpp"

Pacman::Pacman(const char* SPRITE_FILE, unsigned int rows, unsigned int cols, float speed): 
    BaseSprite(SPRITE_FILE, rows, cols, speed) 
{
    // Create animation objects for each direction
    anim[DIRS::RIGHT] = Animation{spriteHeight, spriteWidth, 0.15f, {{0, 0 * spriteHeight}, {spriteWidth, 0 * spriteHeight}}};
    anim[DIRS::LEFT]  = Animation{spriteHeight, spriteWidth, 0.15f, {{0, 1 * spriteHeight}, {spriteWidth, 1 * spriteHeight}}};
    anim[DIRS::UP]    = Animation{spriteHeight, spriteWidth, 0.15f, {{0, 2 * spriteHeight}, {spriteWidth, 2 * spriteHeight}}};
    anim[DIRS::DOWN]  = Animation{spriteHeight, spriteWidth, 0.15f, {{0, 3 * spriteHeight}, {spriteWidth, 3 * spriteHeight}}};
    currDir = DIRS::LEFT;
}

void Pacman::update(float deltaTime, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map) {

    std::function<std::size_t(float)> floor {[](float val) { return static_cast<std::size_t>(std::floor(val)); }};
    std::function<std::size_t(float)>  ceil {[](float val) { return static_cast<std::size_t>( std::ceil(val)); }};

    std::function<bool(float, float)> checkNoWallCollision {[&floor, &ceil, &map](float i, float j) -> bool {  
        if (i < 0 || j < 0 || floor(i / CELL_SIZE) > MAP_HEIGHT || floor(j / CELL_SIZE) > MAP_WIDTH) {
            return CELL::WALL;
        } else {
            std::size_t i_floor {floor(i / CELL_SIZE)}, j_floor {floor(j / CELL_SIZE)};
            std::size_t  i_ceil { ceil(i / CELL_SIZE)},  j_ceil { ceil(j / CELL_SIZE)};
            CELL TL {map[i_floor][j_floor]}, TR {map[i_floor][j_ceil]};
            CELL BL {map[i_ceil][j_floor]}, BR {map[i_ceil][j_ceil]};
            return TL != CELL::WALL && TR != CELL::WALL && BL != CELL::WALL && BR != CELL::WALL; 
        }
    }};

    std::function<void(float)> snap2Grid {[this](float alignTol){
        sf::Vector2f pos {body.getPosition()};
        float topLeftX {std::round(pos.x / CELL_SIZE) * CELL_SIZE};
        float topLeftY {std::round(pos.y / CELL_SIZE) * CELL_SIZE};

        if (std::abs(pos.x - topLeftX) / CELL_SIZE <= alignTol)
            pos.x = topLeftX;
        if (std::abs(pos.y - topLeftY) / CELL_SIZE <= alignTol)
            pos.y = topLeftY;

        body.setPosition(pos.x, pos.y);
    }};

    // Move pacman around
    DIRS nextDir;
    sf::Vector2f movement;
    if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::J)) {
        nextDir = DIRS::DOWN; movement = {0, 1};
    } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::K)) {
        nextDir = DIRS::UP; movement = {0, -1};
    } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::H)) {
        nextDir = DIRS::LEFT; movement = {-1, 0};
    } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::L)) {
        nextDir = DIRS::RIGHT; movement = {1, 0};
    }

    if (movement.x != 0 || movement.y != 0) {
        if (nextDir != currDir) {
            currDir = nextDir; snap2Grid(0.30f);
        }
        sf::Vector2f currPos {body.getPosition()};
        if (checkNoWallCollision(currPos.y + movement.y, currPos.x + movement.x)) {
            body.move({movement.x * speed, movement.y * speed});

            sf::Vector2f pos {body.getPosition()};
            std::size_t nearestX {static_cast<std::size_t>(std::round(pos.x / CELL_SIZE))};
            std::size_t nearestY {static_cast<std::size_t>(std::round(pos.y / CELL_SIZE))};
            if (map[nearestY][nearestX] == CELL::PELLET) {
                map[nearestY][nearestX] = CELL::EMPTY;
            } else if (map[nearestY][nearestX] == CELL::ENERGIZER) {
                map[nearestY][nearestX] = CELL::EMPTY;
            }
        }
    }

    anim[currDir].update(deltaTime);
    body.setTextureRect(anim[currDir].getRect());
}

Wall::Wall(const char* SPRITE_FILE, unsigned int rows, unsigned int cols): 
    BaseSprite(SPRITE_FILE, rows, cols, 0) {}

Food::Food(const char* SPRITE_FILE, unsigned int rows, unsigned int cols): 
    BaseSprite(SPRITE_FILE, rows, cols, 0) {}

