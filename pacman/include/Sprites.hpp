#pragma once

#include <unordered_map>

#include "Constants.hpp"
#include "BaseSprite.hpp"
#include "Animation.hpp"

class Pacman: public BaseSprite {
    private:
        std::unordered_map<DIRS, Animation> anim;
        DIRS currDir;

    public:
        Pacman(const char* SPRITE_FILE, unsigned int rows, unsigned int cols, float speed = 1.f);
        void update(float deltaTime, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map);
};

class Wall: public BaseSprite {
    public:
        Wall(const char* SPRITE_FILE, unsigned int rows, unsigned int cols);
};

class Food: public BaseSprite {
    public:
        Food(const char* SPRITE_FILE, unsigned int rows, unsigned int cols);
};
