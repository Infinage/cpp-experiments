#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

class BaseSprite {
    protected:
        sf::Sprite body;
        float speed;
        float scaleX, scaleY;
        unsigned int spriteHeight, spriteWidth;
        sf::Texture texture;

    public:
        virtual ~BaseSprite() = default;
        BaseSprite(const char* SPRITE_FILE, unsigned int rows, unsigned int cols, float speed);
        void setPosition(std::size_t row, std::size_t col);
        void setSpeed(float speed);
        void setTextureRect(const sf::IntRect &rect);
        void draw(sf::RenderWindow &window);
        unsigned int getHeight();
        unsigned int getWidth();
};
