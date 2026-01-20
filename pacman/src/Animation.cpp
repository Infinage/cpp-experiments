#include "../include/Animation.hpp"

Animation::Animation(unsigned int height, unsigned int width, float switchTime, const std::vector<sf::Vector2u> &frames): 
    spriteHeight(height), spriteWidth(width), frames(frames), switchTime(switchTime) 
{
    uvRect.size.y = static_cast<int>(spriteHeight);
    uvRect.size.x  = static_cast<int>(spriteWidth);
    uvRect.position.x   = static_cast<int>(frames[0].x);
    uvRect.position.y    = static_cast<int>(frames[0].y);
}

sf::IntRect Animation::getRect() { 
    return uvRect; 
}

void Animation::update(float deltaTime) {
    totalTime += deltaTime;
    if (totalTime >= switchTime) {
        totalTime = 0;
        currFrame += 1;
        currFrame %= frames.size();
    }

    uvRect.position.x = static_cast<int>(frames[currFrame].x);
    uvRect.position.y  = static_cast<int>(frames[currFrame].y);
}
