#include "../include/Sprites.hpp"
#include "../include/GhostStrategy.hpp"
#include "../include/Utils.hpp"

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

void Ghost::setChaseStrategy(std::unique_ptr<Strategy> strategy) {
    strategy->setGhost(this);
    this->chaseStrategy = std::move(strategy);
}

void Ghost::setFrightStrategy(std::unique_ptr<Strategy> strategy) {
    strategy->setGhost(this);
    this->frightStrategy = std::move(strategy);
}

void Ghost::setScatterStrategy(std::unique_ptr<Strategy> strategy) {
    strategy->setGhost(this);
    this->scatterStrategy = std::move(strategy);
}

void Ghost::setMode(MODE mode) {
    // Snap the coordinates
    sf::Vector2f snappedCoords {snap2Grid(body.getPosition(), 1.f)};
    body.setPosition(snappedCoords);

    // Adjust ghost params based on mode
    switch (mode) {
        case FRIGHTENED:
            body.setTexture(frightTexture);
            setSpeed(GHOST_FRIGHTENED_SPEED);
            break;

        case CHASE:
            if (currMode != FRIGHTENED) 
                currDir = revDirs.at(currDir);
            body.setTexture(texture);
            setSpeed(GHOST_SPEED);
            break;

        case SCATTER:
            body.setTexture(texture);
            setSpeed(GHOST_SPEED);
            break;
    }

    // Set mode
    currMode = mode;
}

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


void Ghost::update(float deltaTime, MAP &map, GHOSTS &ghosts, Pacman &pacman) {
    if (currMode == CHASE) {
        if (shouldChangeDir(map))
            currDir = chaseStrategy->getNext(map, pacman, ghosts);

        anim[currDir].update(deltaTime);
        body.setTextureRect(anim[currDir].getRect());
    }

    else if (currMode == SCATTER) {
        if (shouldChangeDir(map))
            currDir = scatterStrategy->getNext(map, pacman, ghosts);

        anim[currDir].update(deltaTime);
        body.setTextureRect(anim[currDir].getRect());
    }

    else {
        if (shouldChangeDir(map))
            currDir = frightStrategy->getNext(map, pacman, ghosts);

        frightAnimation.update(deltaTime);
        body.setTextureRect(frightAnimation.getRect());
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
