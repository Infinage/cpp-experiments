#include <SFML/Window/Keyboard.hpp>
#include <cmath>
#include <memory>

#include "../include/Sprites.hpp"
#include "../include/Utils.hpp"
#include "../include/GhostStrategy.hpp"

// ****************** Define functions for Pacman ****************** //

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

DIRS Pacman::getDir() const { return currDir; }

void Pacman::update(float deltaTime, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map) {
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
            sf::Vector2f snappedCoords {snap2Grid(body.getPosition(), 0.30f)};
            body.setPosition(snappedCoords);
            currDir = nextDir; 
        }

        sf::Vector2f currPos {body.getPosition()};
        if (checkNoWallCollision(currPos.y + movement.y, currPos.x + movement.x, map)) {
            body.move({movement.x * speed, movement.y * speed});

            sf::Vector2f pos {body.getPosition()};
            std::size_t nearestX {round<std::size_t>(pos.x / CELL_SIZE)}, nearestY {round<std::size_t>(pos.y / CELL_SIZE)};
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

// ****************** Define functions for Wall / Food ****************** //

Wall::Wall(const char* SPRITE_FILE, unsigned int rows, unsigned int cols): 
    BaseSprite(SPRITE_FILE, rows, cols, 0) {}

Food::Food(const char* SPRITE_FILE, unsigned int rows, unsigned int cols): 
    BaseSprite(SPRITE_FILE, rows, cols, 0) {}

// ****************** Define functions for Ghost ****************** //

Ghost::Ghost(const char* SPRITE_FILE, unsigned int rows, unsigned int cols, float speed): 
    BaseSprite(SPRITE_FILE, rows, cols, speed) 
{
    // Create animation objects for each direction
    anim[DIRS::RIGHT] = Animation{spriteHeight, spriteWidth, 0.15f, {{0 * spriteWidth, 0}, {1 * spriteWidth, 0}}};
    anim[DIRS::LEFT]  = Animation{spriteHeight, spriteWidth, 0.15f, {{2 * spriteWidth, 0}, {3 * spriteWidth, 0}}};
    anim[DIRS::UP]    = Animation{spriteHeight, spriteWidth, 0.15f, {{4 * spriteWidth, 0}, {5 * spriteWidth, 0}}};
    anim[DIRS::DOWN]  = Animation{spriteHeight, spriteWidth, 0.15f, {{6 * spriteWidth, 0}, {7 * spriteWidth, 0}}};
    currDir = DIRS::LEFT;
}

void Ghost::setChaseStrategy(std::unique_ptr<Strategy> strategy) {
    chaseStrategy = std::move(strategy);
}

DIRS Ghost::getDir() const { return currDir; }

bool Ghost::shouldChangeDir(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map) const {
    auto [px, py] {body.getPosition()};
    auto [cx, cy] {getPosition()};
    auto [nx, ny] = travel(cx, cy, getDir());

    bool snapped2Grid {static_cast<float>(cx * CELL_SIZE) == px && static_cast<float>(cy * CELL_SIZE) == py};
    bool nextIsWall {snapped2Grid && map[ny][nx] == CELL::WALL};
    bool isCorridor {snapped2Grid && !(
            (map[cy - 1][cx] == CELL::WALL && map[cy + 1][cx] == CELL::WALL) 
         || (map[cy][cx - 1] == CELL::WALL && map[cy][cx + 1] == CELL::WALL)
    )};

    return nextIsWall || isCorridor;
}

void Ghost::update(float deltaTime, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, Pacman &pacman) {
    if (shouldChangeDir(map))
        currDir = chaseStrategy->getNext();

    sf::Vector2f movement;
    if (currDir == DIRS::DOWN)
        movement = {0, 1};
    else if (currDir == DIRS::UP)
        movement = {0, -1};
    else if (currDir == DIRS::LEFT)
        movement = {-1, 0};
    else if (currDir == DIRS::RIGHT)
        movement = {1, 0};

    body.move({movement.x * speed, movement.y * speed});

    std::pair<std::size_t, std::size_t> ghostPos {getPosition()}, pacmanPos {pacman.getPosition()};
    if (ghostPos == pacmanPos) {
        
    }

    anim[currDir].update(deltaTime);
    body.setTextureRect(anim[currDir].getRect());
}
