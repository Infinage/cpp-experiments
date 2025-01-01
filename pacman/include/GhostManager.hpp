#pragma once

#include "Constants.hpp"
#include "Sprites.hpp"

class GhostManager {
    private:
        GHOSTS &ghosts;
        MAP &map;
        Ghost::MODE currState{Ghost::MODE::SCATTER}, prevState;
        float currTimer{GHOST_SCATTER_TIME}, prevTimer{0};

    public:
        GhostManager(GHOSTS &ghosts, MAP &map);
        void update(float deltaTime);
        void frightenGhosts();
};
