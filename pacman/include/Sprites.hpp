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
        float powerUpTimer{0};

    public:
        Pacman(const char* SPRITE_FILE, unsigned int rows, unsigned int cols, float speed = 1.f);
        void update(float deltaTime, MAP &map);
        DIRS getDir() const;
        bool isPoweredUp() const;
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
        sf::Texture &frightTexture;
        Animation& frightAnimation;
        std::unique_ptr<Strategy> chaseStrategy;
        std::unique_ptr<Strategy> frightStrategy;
        MODE currMode {CHASE};
        DIRS currDir{DIRS::LEFT};

    public:
        Ghost(
            const char* SPRITE_FILE, unsigned int rows, unsigned int cols,
            sf::Texture &frightTexture, Animation& frightAnimation, float speed = GHOST_SPEED
        );
        void update(float deltaTime, MAP &map, GHOSTS &ghosts, Pacman &pacman);
        bool shouldChangeDir(MAP &map) const;
        void setChaseStrategy(std::unique_ptr<Strategy> strategy);
        void setFrightStrategy(std::unique_ptr<Strategy> strategy);
        DIRS getDir() const;
};
