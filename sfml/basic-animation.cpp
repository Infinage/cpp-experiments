#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowStyle.hpp>

class PacmanTextureAnimation {
    private:
        bool currIdx {0};
        float totalTime {0}, switchTime;

    public:
        enum DIR {RIGHT=0, LEFT=1, UP=2, DOWN=3}; 
        sf::IntRect uvRect;
        DIR dir{DIR::LEFT};

    public:
        PacmanTextureAnimation(const sf::Vector2u &textureSize, float switchTime): 
            switchTime(switchTime) 
        {
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

int main() {
    sf::RenderWindow window {sf::VideoMode(512, 512), "Pacman Sample", sf::Style::Close};
    sf::RectangleShape obj{sf::Vector2f(50.0f, 50.0f)};
    obj.setFillColor(sf::Color::Yellow);
    obj.setPosition(256, 256);;

    // Default - 0, 0
    obj.setOrigin(25.f, 25.f);

    // Loading textures
    sf::Texture texture;
    texture.loadFromFile("pacman.png");
    obj.setTexture(&texture);
    PacmanTextureAnimation anim{texture.getSize(), 0.15f};

    sf::Clock clk;
    float deltaTime {0.f};

    while (window.isOpen()) {

        // Get time elapsed
        deltaTime = clk.restart().asSeconds();

        // Basic events
        sf::Event evnt; 
        while (window.pollEvent(evnt)) {
            if (evnt.type == sf::Event::Closed)
                window.close();
        }

        // Move the square around with keystrokes
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::J))
            anim.updateDir(PacmanTextureAnimation::DIR::DOWN);
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::K))
            anim.updateDir(PacmanTextureAnimation::DIR::UP);
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::H))
            anim.updateDir(PacmanTextureAnimation::DIR::LEFT);
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::L))
            anim.updateDir(PacmanTextureAnimation::DIR::RIGHT);

        // Set bounding box
        anim.update(deltaTime);
        obj.setTextureRect(anim.uvRect);

        // Clear and display
        window.clear();
        window.draw(obj);
        window.display();
    }

    return 0;
}
