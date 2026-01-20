#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <array>
#include <memory>

#include "include/GhostStrategy.hpp"
#include "include/Sprites.hpp"
#include "include/Constants.hpp"
#include "include/Utils.hpp"
#include "include/GhostManager.hpp"

int main() {


    sf::RenderWindow window{sf::VideoMode{{CELL_SIZE * MAP_WIDTH, CELL_SIZE * MAP_HEIGHT}}, "Pacman"};
    std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> map{getMap()};

    // Create the frightened animation common to all ghosts
    sf::Texture frightTexture;
    if (!frightTexture.loadFromFile(FRIGHTENED_GHOST_SPRITE_FILE))
        throw std::runtime_error{"Failed to load Sprite: FRIGHTENED_GHOST_SPRITE_FILE"};

    sf::Vector2u textureSize{frightTexture.getSize()};
    unsigned int spriteHeight {textureSize.y / FRIGHTENED_GHOST_SPRITE_ROWS};
    unsigned int spriteWidth {textureSize.x / FRIGHTENED_GHOST_SPRITE_COLS};
    Animation frightAnimation {spriteHeight, spriteWidth, 0.15f, {{0 * spriteWidth, 0}, {1 * spriteWidth, 0}}};

    // Init sprites
    Pacman pacman{PACMAN_SPRITE_FILE, 4, 2};
    pacman.setPosition(15, 9);

    Wall wall{WALL_SPRITE_FILE, 1, 1};
    Food food{FOOD_SPRITE_FILE, 1, 2};

    // Blinky - Red ghost
    Ghost blinky{BLINKY_SPRITE_FILE, 1, 8, frightTexture, frightAnimation};
    blinky.setPosition(7, 9);
    blinky.setChaseStrategy(std::make_unique<Shadow>());
    blinky.setFrightStrategy(std::make_unique<Fright>());
    blinky.setScatterStrategy(std::make_unique<Scatter>(BLINKY_SCATTER_TARGET));

    // Pinky - Pink ghost
    Ghost pinky{PINKY_SPRITE_FILE, 1, 8, frightTexture, frightAnimation};
    pinky.setPosition(9, 8);
    pinky.setChaseStrategy(std::make_unique<Ambush>());
    pinky.setFrightStrategy(std::make_unique<Fright>());
    pinky.setScatterStrategy(std::make_unique<Scatter>(PINKY_SCATTER_TARGET));

    // Clyde - Orange ghost
    Ghost clyde{CLYDE_SPRITE_FILE, 1, 8, frightTexture, frightAnimation};
    clyde.setPosition(9, 10);
    clyde.setChaseStrategy(std::make_unique<Fickle>(CLYDE_SCATTER_TARGET));
    clyde.setFrightStrategy(std::make_unique<Fright>());
    clyde.setScatterStrategy(std::make_unique<Scatter>(CLYDE_SCATTER_TARGET));

    // Ghosts Array & Manager
    GHOSTS ghosts = {&blinky, &pinky, &clyde};
    GhostManager ghostManager{ghosts, map};

    sf::Clock clk;
    float deltaTime;

    bool pelletExists {true};
    while (window.isOpen() && pelletExists) {

        // Get time elapsed
        deltaTime = clk.restart().asSeconds();

        while (auto evnt = window.pollEvent()) {
            if (evnt->is<sf::Event::Closed>())
                window.close();
        }

        // Update the ghost manager
        ghostManager.update(deltaTime);

        // Move pacman
        pacman.update(deltaTime, map, ghostManager);

        // Move Ghosts
        for (Ghost *ghost: ghosts)
            ghost->update(deltaTime, map, ghosts, pacman);

        // Draw the elements
        window.clear();
        pelletExists = renderWorld(map, window, pacman, ghosts, wall, food);
        window.display();
    }

    return 0;
}
