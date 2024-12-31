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

    // Init sprites
    Pacman pacman{PACMAN_SPRITE_FILE, 4, 2};
    pacman.setPosition(15, 9);
    Ghost blinky{BLINKY_SPRITE_FILE, 1, 8};
    blinky.setPosition(7, 9);
    Wall wall{WALL_SPRITE_FILE, 1, 1};
    Food food{FOOD_SPRITE_FILE, 1, 2};

    // Create a set strategies for the ghosts
    std::unique_ptr<Strategy> blinkyChaseStrategy {std::make_unique<Shadow>(map, blinky, pacman)};
    blinky.setChaseStrategy(std::move(blinkyChaseStrategy));

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
        blinky.update(deltaTime, map, pacman);

        // Draw the elements
        window.clear();
        pelletExists = renderWorld(map, window, pacman, blinky, wall, food);
        window.display();
    }

    return 0;
}
