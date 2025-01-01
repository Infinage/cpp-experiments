#include "../include/Sprites.hpp"

// ****************** Define functions for Wall / Food ****************** //

Wall::Wall(const char* SPRITE_FILE, unsigned int rows, unsigned int cols): 
    BaseSprite(SPRITE_FILE, rows, cols, 0) {}

Food::Food(const char* SPRITE_FILE, unsigned int rows, unsigned int cols): 
    BaseSprite(SPRITE_FILE, rows, cols, 0) {}

