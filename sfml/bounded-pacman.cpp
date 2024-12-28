#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowStyle.hpp>

constexpr int VIEW_WIDTH {512};
constexpr int VIEW_HEIGHT {512};

class TextureAnimation {
    private:
        bool currIdx {0};
        float totalTime {0}, switchTime;

    public:
        enum DIR {RIGHT=0, LEFT=1, UP=2, DOWN=3}; 
        sf::IntRect uvRect;
        DIR dir{DIR::LEFT};

    public:
        TextureAnimation() = default;
        TextureAnimation(const sf::Vector2u &textureSize, float switchTime): switchTime(switchTime) {
            uvRect.width = textureSize.x / 2; 
            uvRect.height = textureSize.y / 4;
        }

        void updateDir(DIR dir) { this->dir = dir; }
        void update(float deltaTime) {
            totalTime += deltaTime;
            if (totalTime >= switchTime) {
                totalTime = 0;   
                currIdx = !currIdx;
            }

            uvRect.left = currIdx * uvRect.width;
            uvRect.top = dir * uvRect.height;
        }
};

class Pacman {
    private:
        sf::RectangleShape body;
        float speed;
        sf::Texture texture;
        TextureAnimation animation;

    public:
        Pacman(float moveSpeed, float animationSwitchTime, const std::string &textureFpath): 
            body(sf::Vector2f(50.0f, 50.0f)), speed(moveSpeed)
        {
            texture.loadFromFile(textureFpath);
            animation = TextureAnimation{texture.getSize(), animationSwitchTime};
            body.setTexture(&texture);
            body.setOrigin(body.getSize().x / 2, body.getSize().y / 2);
            body.setFillColor(sf::Color::Yellow);
            body.setPosition(256, 256);;
        }

        void draw(sf::RenderWindow &window) { window.draw(body); }
        void move(float deltaTime, sf::View &view) {
            // Move pacman around with keystrokes
            sf::Vector2f movement {0.f, 0.f};
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::J)) {
                animation.updateDir(TextureAnimation::DIR::DOWN);
                movement.y += speed;
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::K)) {
                animation.updateDir(TextureAnimation::DIR::UP);
                movement.y -= speed;
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::H)) {
                animation.updateDir(TextureAnimation::DIR::LEFT);
                movement.x -= speed;
            } else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::L)) {
                animation.updateDir(TextureAnimation::DIR::RIGHT);
                movement.x += speed;
            }

            // Set texture for the body post update
            animation.update(deltaTime);
            body.setTextureRect(animation.uvRect);

            // Calculate global bounds using the view's properties
            sf::Vector2f viewCenter = {view.getCenter()};
            sf::Vector2f viewSize {view.getSize()};
            sf::FloatRect bounds{
                viewCenter.x - viewSize.x / 2.f,
                viewCenter.y - viewSize.y / 2.f,
                viewSize.x,
                viewSize.y
            };

            // Move the body while clamping its position to within the view bounds
            sf::Vector2f pacmanSize {body.getSize()}, nextPos {body.getPosition() + movement};
            nextPos.x = std::clamp(nextPos.x, bounds.left + pacmanSize.x / 2, bounds.left + bounds.width - pacmanSize.x / 2);
            nextPos.y = std::clamp(nextPos.y, bounds.top + pacmanSize.y / 2, bounds.top + bounds.height - pacmanSize.y / 2);
            body.setPosition(nextPos);
        }
};

int main() {
    sf::RenderWindow window {sf::VideoMode(VIEW_WIDTH, VIEW_HEIGHT), "Pacman Sample", sf::Style::Close};
    sf::View view{
        sf::Vector2f(VIEW_WIDTH / 2., VIEW_HEIGHT / 2.), 
        sf::Vector2f{static_cast<float>(VIEW_WIDTH), static_cast<float>(VIEW_HEIGHT)
    }};

    // Create pacman object
    Pacman pacman{1.f, 0.15f, "pacman.png"};

    sf::Clock clk;
    float deltaTime {0.f};

    while (window.isOpen()) {

        // Get time elapsed
        deltaTime = clk.restart().asSeconds();

        // Basic events
        sf::Event evnt; 
        while (window.pollEvent(evnt)) {
            switch (evnt.type) {
                case sf::Event::Closed: {
                    window.close();
                    break;
                } case sf::Event::Resized: {
                    sf::Vector2u windowSize {window.getSize()};
                    float aspectRatio {static_cast<float>(windowSize.x) / static_cast<float>(windowSize.y)};
                    view.setSize(VIEW_WIDTH * aspectRatio, VIEW_HEIGHT);
                    break;
                 } default: {}
            }
        }

        // Set bounding box
        pacman.move(deltaTime, view);

        // Clear and display
        window.clear();
        window.setView(view);
        pacman.draw(window);
        window.display();
    }

    return 0;
}
