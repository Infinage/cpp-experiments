#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <vector>

class Animation {
    private:
        unsigned int spriteHeight, spriteWidth;
        std::vector<sf::Vector2u> frames;
        float totalTime{0}, switchTime;
        std::size_t currFrame {0};
        sf::IntRect uvRect;

    public:
        Animation() = default;
        Animation(unsigned int height, unsigned int width, float switchTime, const std::vector<sf::Vector2u> &frames);
        sf::IntRect getRect();
        void update(float deltaTime);
};
