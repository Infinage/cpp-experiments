#include <utility>

#include "../include/Constants.hpp"
#include "../include/BaseSprite.hpp"
#include "../include/Utils.hpp"

BaseSprite::BaseSprite(const char* SPRITE_FILE, unsigned int rows, unsigned int cols, float speed): speed(speed) {
    // Load the sprite texture
    texture.loadFromFile(SPRITE_FILE);
    body.setTexture(texture);

    // Set the appropriate scale
    sf::Vector2u textureSize{texture.getSize()};
    spriteHeight = textureSize.y / rows;
    scaleY = static_cast<float>(CELL_SIZE) / static_cast<float>(spriteHeight);
    spriteWidth = textureSize.x / cols;
    scaleX = static_cast<float>(CELL_SIZE) / static_cast<float>(spriteWidth);
    body.setScale(scaleX, scaleY);
}

void BaseSprite::setPosition(std::size_t row, std::size_t col) { 
    body.setPosition(static_cast<float>(col * CELL_SIZE), static_cast<float>(row * CELL_SIZE)); 
}

std::pair<std::size_t, std::size_t> BaseSprite::getPosition() const {
    sf::Vector2f pos{snap2Grid(body.getPosition(), 0.05f)};
    return {static_cast<std::size_t>(pos.x / CELL_SIZE), static_cast<std::size_t>(pos.y / CELL_SIZE)};
}

void BaseSprite::setSpeed(float speed) { 
    this->speed = speed; 
}

void BaseSprite::setTextureRect(const sf::IntRect &rect) { 
    body.setTextureRect(rect); 
}

void BaseSprite::draw(sf::RenderWindow &window) const { 
    window.draw(body); 
}

unsigned int BaseSprite::getHeight() const { 
    return this->spriteHeight; 
}

unsigned int BaseSprite::getWidth() const { 
    return this->spriteWidth; 
}

