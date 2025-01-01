#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <array>
#include <cmath>
#include <memory>

#include "include/GhostStrategy.hpp"
#include "include/Sprites.hpp"
#include "include/Constants.hpp"
#include "include/Utils.hpp"

int main() {

    sf::RenderWindow window{sf::VideoMode{CELL_SIZE * MAP_WIDTH, CELL_SIZE * MAP_HEIGHT}, "Pacman"};
    std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> map{getMap()};

    // Create the frightened animation common to all ghosts
    sf::Texture frightTexture;
    frightTexture.loadFromFile(FRIGHTENED_GHOST_SPRITE_FILE);
    sf::Vector2u textureSize{frightTexture.getSize()};
    unsigned int spriteHeight {textureSize.y / FRIGHTENED_GHOST_SPRITE_ROWS};
    unsigned int spriteWidth {textureSize.x / FRIGHTENED_GHOST_SPRITE_COLS};
    Animation frightAnimation {spriteHeight, spriteWidth, 0.15f, {{0 * spriteWidth, 0}, {1 * spriteWidth, 0}}};

    // Init sprites
    Pacman pacman{PACMAN_SPRITE_FILE, 4, 2};
    pacman.setPosition(15, 9);

    Ghost blinky{BLINKY_SPRITE_FILE, 1, 8, frightTexture, frightAnimation};
    blinky.setPosition(7, 9);

    Ghost pinky{PINKY_SPRITE_FILE, 1, 8, frightTexture, frightAnimation};
    pinky.setPosition(9, 8);

    Wall wall{WALL_SPRITE_FILE, 1, 1};
    Food food{FOOD_SPRITE_FILE, 1, 2};

    // Create and set strategies for the ghosts
    blinky.setChaseStrategy(std::make_unique<Shadow>());
    blinky.setFrightStrategy(std::make_unique<Fright>());
    pinky.setChaseStrategy(std::make_unique<Ambush>());
    pinky.setFrightStrategy(std::make_unique<Fright>());

    // Ghosts Array
    GHOSTS ghosts = {&blinky, &pinky, nullptr, nullptr};

    sf::Clock clk;
    float deltaTime;

    bool pelletExists {true};
    while (window.isOpen() && pelletExists) {

        // Get time elapsed
        deltaTime = clk.restart().asSeconds();

        sf::Event evnt;
        while (window.pollEvent(evnt)) {
            switch (evnt.type) {
                case sf::Event::Closed: {
                    window.close();
                    break;
                } default: {}
            }
        }

        // Move pacman
        pacman.update(deltaTime, map);

        // Move Ghosts
        blinky.update(deltaTime, map, ghosts, pacman);
        pinky.update(deltaTime, map, ghosts, pacman);

        // Draw the elements
        window.clear();
        pelletExists = renderWorld(map, window, pacman, blinky, pinky, wall, food);
        window.display();
    }

    return 0;
}
