#include <SFML/Window/Keyboard.hpp>

#include "../include/Sprites.hpp"
#include "../include/Utils.hpp"
#include "../include/GhostManager.hpp"

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

void Pacman::update(float deltaTime, MAP &map, GhostManager &ghostManager) {
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
                map[nearestY][nearestX] = CELL::EMPTY;
                ghostManager.frightenGhosts();
            }
        }
    }

    // Update the pacman animation for every frame
    anim[currDir].update(deltaTime);
    body.setTextureRect(anim[currDir].getRect());
}
