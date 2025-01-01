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

DIRS Pacman::getDir() const { 
    return currDir; 
}

bool Pacman::isPoweredUp() const { 
    return powerUpTimer > 0; 
}

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
            sf::Vector2f snappedCoords {snap2Grid(body.getPosition(), 0.05f)};
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
                powerUpTimer = PACMAN_POWER_UP_TIME;
                map[nearestY][nearestX] = CELL::EMPTY;
            }
        }
    }

    // Update the powerup timer if pacman has recently eaten an energizer
    if (powerUpTimer > 0)
        powerUpTimer = std::max(0.f, powerUpTimer - deltaTime);

    // Update the pacman animation for every frame
    anim[currDir].update(deltaTime);
    body.setTextureRect(anim[currDir].getRect());
}

// ****************** Define functions for Wall / Food ****************** //

Wall::Wall(const char* SPRITE_FILE, unsigned int rows, unsigned int cols): 
    BaseSprite(SPRITE_FILE, rows, cols, 0) {}

Food::Food(const char* SPRITE_FILE, unsigned int rows, unsigned int cols): 
    BaseSprite(SPRITE_FILE, rows, cols, 0) {}

// ****************** Define functions for Ghost ****************** //

Ghost::Ghost(
        const char* SPRITE_FILE, unsigned int rows, unsigned int cols, 
        sf::Texture &frightTexture, Animation& frightAnimation, float speed
    )
    : 
    BaseSprite(SPRITE_FILE, rows, cols, speed), 
    frightTexture(frightTexture), 
    frightAnimation(frightAnimation)
{
    // Create animation objects for each direction
    anim[DIRS::RIGHT] = Animation{spriteHeight, spriteWidth, 0.15f, {{0 * spriteWidth, 0}, {1 * spriteWidth, 0}}};
    anim[DIRS::LEFT]  = Animation{spriteHeight, spriteWidth, 0.15f, {{2 * spriteWidth, 0}, {3 * spriteWidth, 0}}};
    anim[DIRS::UP]    = Animation{spriteHeight, spriteWidth, 0.15f, {{4 * spriteWidth, 0}, {5 * spriteWidth, 0}}};
    anim[DIRS::DOWN]  = Animation{spriteHeight, spriteWidth, 0.15f, {{6 * spriteWidth, 0}, {7 * spriteWidth, 0}}};
    currDir = DIRS::LEFT;
}

DIRS Ghost::getDir() const { return currDir; }

bool Ghost::shouldChangeDir(MAP &map) const {
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

void Ghost::setChaseStrategy(std::unique_ptr<Strategy> strategy) {
    strategy->setGhost(this);
    this->chaseStrategy = std::move(strategy);
}

void Ghost::setFrightStrategy(std::unique_ptr<Strategy> strategy) {
    strategy->setGhost(this);
    this->frightStrategy = std::move(strategy);
}

void Ghost::update(float deltaTime, MAP &map, GHOSTS &ghosts, Pacman &pacman) {
    if (pacman.isPoweredUp()) {
        if (shouldChangeDir(map))
            currDir = frightStrategy->getNext(map, pacman, ghosts);

        if (currMode != FRIGHTENED) {
            sf::Vector2f snappedCoords {snap2Grid(body.getPosition(), 1.f)};
            body.setPosition(snappedCoords);
            body.setTexture(frightTexture);
            setSpeed(GHOST_FRIGHTENED_SPEED);
        }

        currMode = FRIGHTENED;
        frightAnimation.update(deltaTime);
        body.setTextureRect(frightAnimation.getRect());
    }

    else {
        if (shouldChangeDir(map))
            currDir = chaseStrategy->getNext(map, pacman, ghosts);

        if (currMode != CHASE) {
            sf::Vector2f snappedCoords {snap2Grid(body.getPosition(), 1.f)};
            body.setPosition(snappedCoords);
            body.setTexture(texture);
            setSpeed(GHOST_SPEED);
        }

        currMode = CHASE;
        anim[currDir].update(deltaTime);
        body.setTextureRect(anim[currDir].getRect());
    }

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

}
