// $ g++ missilesim.cpp -std=c++23 -o missilesim -lsfml-graphics -lsfml-window -lsfml-system -O2
#include <SFML/Graphics.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

int main() {
    // Create SFML Window
    sf::RenderWindow window{sf::VideoMode{800, 800}, "Missile Simulator"};
    window.setMouseCursorVisible(false);

    // Create the target that moves with the mouse
    sf::ConvexShape target;
    target.setPointCount(3);
    target.setPoint(0, sf::Vector2f(  0.f, -10.f));
    target.setPoint(1, sf::Vector2f(-10.f,  10.f));
    target.setPoint(2, sf::Vector2f(+10.f,  10.f));
    target.setFillColor(sf::Color::Red);

    while (window.isOpen()) {
        // Poll events and respond to user interaction
        sf::Event evnt;
        while (window.pollEvent(evnt)) {
            switch (evnt.type) {
                case sf::Event::Closed: {
                    window.close();
                    break;
                } case sf::Event::Resized: {
                    float width_ {static_cast<float>(evnt.size.width)}, height_ {static_cast<float>(evnt.size.height)};
                    window.setView(sf::View{sf::FloatRect{0, 0, width_, height_}});
                    break;
                } default: break;
            }
        }

        // Track mouse and set target position
        sf::Vector2f winSize {window.getSize()};
        sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        float clampedX {std::clamp(static_cast<float>(mousePos.x), 0.f, winSize.x - 20.f)};
        float clampedY {std::clamp(static_cast<float>(mousePos.y), 0.f, winSize.y - 20.f)};
        target.setPosition(clampedX, clampedY);

        // Clear and redraw
        window.clear();
        window.draw(target);
        window.display();
    }
}
