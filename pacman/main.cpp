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
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

// Define constants
constexpr int MAP_HEIGHT {21};
constexpr int MAP_WIDTH {19};
constexpr int CELL_SIZE {30};
enum DIRS {UP=0, RIGHT=1, DOWN=2, LEFT=3}; 
enum CELL {WALL, PELLET, ENERGIZER, EMPTY};

class Animation {
    private:
        unsigned int spriteHeight, spriteWidth;
        std::vector<sf::Vector2u> frames;
        float totalTime{0}, switchTime;
        std::size_t currFrame {0};
        sf::IntRect uvRect;

    public:
        Animation() = default;
        Animation(unsigned int height, unsigned int width, float switchTime, const std::vector<sf::Vector2u> &frames): 
            spriteHeight(height), spriteWidth(width), frames(frames), switchTime(switchTime) 
        {
            uvRect.height = static_cast<int>(spriteHeight);
            uvRect.width  = static_cast<int>(spriteWidth);
            uvRect.left   = static_cast<int>(frames[0].x);
            uvRect.top    = static_cast<int>(frames[0].y);
        }

        sf::IntRect getRect() { return uvRect; }
        void update(float deltaTime) {
            totalTime += deltaTime;
            if (totalTime >= switchTime) {
                totalTime = 0;
                currFrame += 1;
                currFrame %= frames.size();
            }

            uvRect.left = static_cast<int>(frames[currFrame].x);
            uvRect.top  = static_cast<int>(frames[currFrame].y);
        }
};

class Pacman {
    private:
        sf::Sprite body;
        sf::Texture texture;
        float speed;
        float scaleX, scaleY;
        unsigned int spriteHeight, spriteWidth;
        std::unordered_map<DIRS, Animation> anim;
        DIRS currDir;

    public:
        Pacman(const std::string spriteFLoc, float speed = 1.f): speed(speed) {
            // Load the Pacman Sprite
            texture.loadFromFile(spriteFLoc);
            sf::Vector2u textureSize{texture.getSize()};
            spriteWidth = textureSize.x / 2;
            spriteHeight = textureSize.y / 4;
            body.setTexture(texture);
            scaleY = static_cast<float>(CELL_SIZE) / static_cast<float>(spriteHeight);
            scaleX = static_cast<float>(CELL_SIZE) / static_cast<float>(spriteWidth);
            body.setScale(scaleX, scaleY);

            // Create animation objects for each direction
            anim[DIRS::RIGHT] = Animation{spriteHeight, spriteWidth, 0.15f, {{0, 0 * spriteHeight}, {spriteWidth, 0 * spriteHeight}}};
            anim[DIRS::LEFT]  = Animation{spriteHeight, spriteWidth, 0.15f, {{0, 1 * spriteHeight}, {spriteWidth, 1 * spriteHeight}}};
            anim[DIRS::UP]    = Animation{spriteHeight, spriteWidth, 0.15f, {{0, 2 * spriteHeight}, {spriteWidth, 2 * spriteHeight}}};
            anim[DIRS::DOWN]  = Animation{spriteHeight, spriteWidth, 0.15f, {{0, 3 * spriteHeight}, {spriteWidth, 3 * spriteHeight}}};
            currDir = DIRS::LEFT;
        }

        void update(float deltaTime, std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map) {

            std::function<std::size_t(float)> floor {[](float val) { return static_cast<std::size_t>(std::floor(val)); }};
            std::function<std::size_t(float)>  ceil {[](float val) { return static_cast<std::size_t>( std::ceil(val)); }};

            std::function<bool(float, float)> checkNoCollision {[&floor, &ceil, &map](float i, float j) -> bool {  
                if (i < 0 || j < 0 || floor(i / CELL_SIZE) > MAP_HEIGHT || floor(j / CELL_SIZE) > MAP_WIDTH) {
                    return false;
                } else {
                    std::size_t i_floor {floor(i / CELL_SIZE)}, j_floor {floor(j / CELL_SIZE)};
                    std::size_t  i_ceil { ceil(i / CELL_SIZE)},  j_ceil { ceil(j / CELL_SIZE)};

                    return map[i_floor][j_floor] != CELL::WALL
                        && map[i_ceil][j_floor] != CELL::WALL
                        && map[i_floor][j_ceil] != CELL::WALL
                        && map[i_ceil][j_ceil] != CELL::WALL; 
                }
            }};

            std::function<void(float)> snap2Grid {[this](float alignTol){
                sf::Vector2f pos {body.getPosition()};
				float TLX = std::round(pos.x / CELL_SIZE) * CELL_SIZE;
				float TLY = std::round(pos.y / CELL_SIZE) * CELL_SIZE;

				if (std::abs(pos.x - TLX) / CELL_SIZE <= alignTol)
                    pos.x = TLX;
				if (std::abs(pos.y - TLY) / CELL_SIZE <= alignTol)
                    pos.y = TLY;

                body.setPosition(pos.x, pos.y);
            }};

            // Move pacman around
            DIRS nextDir;
            sf::Vector2f movement;
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::J)) {
                nextDir = DIRS::DOWN; movement = {0, 1};
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::K)) {
                nextDir = DIRS::UP; movement = {0, -1};
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::H)) {
                nextDir = DIRS::LEFT; movement = {-1, 0};
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::L)) {
                nextDir = DIRS::RIGHT; movement = {1, 0};
            }

            if (movement.x != 0 || movement.y != 0) {
                if (nextDir != currDir) {
                    currDir = nextDir; snap2Grid(0.30f);
                }
                sf::Vector2f currPos {body.getPosition()};
                if (checkNoCollision(currPos.y + movement.y, currPos.x + movement.x))
                    body.move({movement.x * speed, movement.y * speed});
            }

            anim[currDir].update(deltaTime);
            body.setTextureRect(anim[currDir].getRect());
        }

        void setPosition(std::size_t row, std::size_t col) { 
            body.setPosition(static_cast<float>(col * CELL_SIZE), static_cast<float>(row * CELL_SIZE)); 
        }

        void draw(sf::RenderTarget &window) { window.draw(body); }
};

std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> getMap() {
    std::array<std::string, MAP_HEIGHT> sketch {{
        "###################",
		"#........#........#",
		"#O##.###.#.###.##O#",
		"#.................#",
		"#.##.#.#####.#.##.#",
		"#....#...#...#....#",
		"####.### # ###.####",
		"   #.#   0   #.#   ",
		"####.# ##=## #.####",
		"    .  #123#  .    ",
		"####.# ##### #.####",
		"   #.#       #.#   ",
		"####.# ##### #.####",
		"#........#........#",
		"#.##.###.#.###.##.#",
		"#O.#.....P.....#.O#",
		"##.#.#.#####.#.#.##",
		"#....#...#...#....#",
		"#.######.#.######.#",
		"#.................#",
		"###################"
    }};

    std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> result;
    for (std::size_t i {0}; i < MAP_HEIGHT; i++) {
        for (std::size_t j {0}; j < MAP_WIDTH; j++) {
            switch (sketch[i][j]) {
                case '#':
                    result[i][j] = CELL::WALL;
                    break;
                case '.':
                    result[i][j] = CELL::PELLET;
                    break;
                case 'O':
                    result[i][j] = CELL::ENERGIZER;
                    break;
                default:
                    result[i][j] = CELL::EMPTY;
            }
        }
    }

    return result;
}

void drawMap(std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> &map, sf::RenderWindow &window) {
    sf::RectangleShape shape{{CELL_SIZE, CELL_SIZE}};
    for (std::size_t i {0}; i < MAP_HEIGHT; i++) {
        for (std::size_t j {0}; j < MAP_WIDTH; j++) {
            if (map[i][j] == CELL::WALL) {
                shape.setFillColor(sf::Color::Blue);
                shape.setPosition(j * CELL_SIZE, i * CELL_SIZE);
                window.draw(shape);
            }
        }
    }
}

int main() {

    sf::RenderWindow window{sf::VideoMode{CELL_SIZE * MAP_WIDTH, CELL_SIZE * MAP_HEIGHT}, "Pacman"};
    Pacman pacman{"assets/pacman.png"};
    pacman.setPosition(15, 9);
    std::array<std::array<CELL, MAP_WIDTH>, MAP_HEIGHT> map{getMap()};

    sf::Clock clk;
    float deltaTime;

    while (window.isOpen()) {

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

        // Draw the elements
        window.clear();
        drawMap(map, window);
        pacman.draw(window);
        window.display();
    }

    return 0;
}
