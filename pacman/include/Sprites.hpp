#pragma once

#include <memory>
#include <unordered_map>

#include "Constants.hpp"
#include "BaseSprite.hpp"
#include "Animation.hpp"

// Forward Decl
class Strategy;

class Pacman: public BaseSprite {
    private:
        std::unordered_map<DIRS, Animation> anim;
        DIRS currDir;

    public:
        Pacman(const char* SPRITE_FILE, unsigned int rows, unsigned int cols, float speed = 1.f);
        void update(float deltaTime, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map);
        DIRS getDir() const;
};

class Wall: public BaseSprite {
    public:
        Wall(const char* SPRITE_FILE, unsigned int rows, unsigned int cols);
};

class Food: public BaseSprite {
    public:
        Food(const char* SPRITE_FILE, unsigned int rows, unsigned int cols);
};

class Ghost: public BaseSprite {
    public:
        enum MODE {CHASE, SCATTER, FRIGHTENED};

    private:
        std::unordered_map<DIRS, Animation> anim;
        std::unique_ptr<Strategy> chaseStrategy;
        MODE currMode {CHASE};
        DIRS currDir{DIRS::LEFT};

    public:
        Ghost(const char* SPRITE_FILE, unsigned int rows, unsigned int cols, float speed = 1.f);
        void update(float deltaTime, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, Pacman &pacman);
        bool shouldChangeDir(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map) const;
        void setChaseStrategy(std::unique_ptr<Strategy> strategy);
        DIRS getDir() const;
};
