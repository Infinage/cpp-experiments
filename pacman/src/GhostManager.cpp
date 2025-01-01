#include "../include/GhostManager.hpp"
#include <algorithm>

GhostManager::GhostManager(GHOSTS &ghosts, MAP &map): ghosts(ghosts), map(map) {}

void GhostManager::update(float deltaTime) {
    currTimer = std::max(0.f, currTimer - deltaTime);
    if (currTimer == 0) {
        if (currState == Ghost::MODE::SCATTER) {
            currState = Ghost::MODE::CHASE;
            currTimer = GHOST_CHASE_TIME;
        } else if (currState == Ghost::MODE::CHASE) {
            currState = Ghost::MODE::SCATTER;
            currTimer = GHOST_SCATTER_TIME;
        } else {
            currState = prevState; 
            currTimer = prevTimer;
        }

        for (std::size_t i{0}; i < ghosts.size(); i++)
            ghosts[i]->setMode(currState);
    }
}

void GhostManager::frightenGhosts() {
    prevTimer = currTimer; prevState = currState;
    currTimer = PACMAN_POWER_UP_TIME;
    currState = Ghost::MODE::FRIGHTENED;
    for (std::size_t i{0}; i < ghosts.size(); i++)
        ghosts[i]->setMode(currState);
}
