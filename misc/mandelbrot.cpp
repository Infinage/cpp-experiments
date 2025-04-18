// $ g++ mandelbrot.cpp -std=c++23 -o mandelbrot -lsfml-graphics -lsfml-window -lsfml-system

/*
 * TODO:
 * 1. SFML + zoom to render appropriate drawings
 * 4. Save output to disk
 * 3. Implement `bigfloat` to support infinite zooms?
 */

#include <complex>
#include <stdexcept>

#include <SFML/Graphics.hpp>

#include "../cli/argparse.hpp"

using namespace std::complex_literals;

// Check whether a given complex no is part of the mandelbrot set
// Returns percentage of many iterations were run before diverging
double check(const std::complex<double> &C, unsigned maxIterations = 100, double boundary = 2.0) {
    std::complex<double> curr{C}; unsigned iterations {0};
    while (std::abs(curr) < boundary && ++iterations < maxIterations)
        curr = curr * curr + C;
    return static_cast<double>(iterations) / maxIterations;
}

// Translate hue to RGB equivalent
sf::Color getRGBColor(double d) {
    double _d {1 - d};
    std::uint8_t r = static_cast<std::uint8_t>(  9 * _d *  d *  d * d * 255);
    std::uint8_t g = static_cast<std::uint8_t>( 15 * _d * _d *  d * d * 255);
    std::uint8_t b = static_cast<std::uint8_t>(8.5 * _d * _d * _d * d * 255);
    return sf::Color(r, g, b);
}

int main(int argc, char **argv) {

    // Specify command line args
    argparse::ArgumentParser parser{"mandelbrot"};
    parser.description("Draws mandelbrot set on to the console.");
    parser.addArgument("n_iters", argparse::ARGTYPE::NAMED).alias("n").defaultValue(100)
        .help("Iterations to run for checking divergence. Must be between (0, 1000]");
    parser.parseArgs(argc, argv);

    // Assert args are valid
    int nIters {parser.get<int>("n_iters")};
    if (nIters <= 0 || nIters > 1000) throw std::runtime_error("`n_iters` must be between (0, 1000]");

    // Start with predetermined height and width, change on user interaction
    unsigned MAX_ITER = {static_cast<unsigned>(nIters)};

    // Create SFML Window - cache image
    sf::Image image; bool redraw {true};
    double MIN_RE {-2.}, MAX_RE {1.}, MIN_IM {-1.}, MAX_IM {1.};
    sf::RenderWindow window{sf::VideoMode{800, 600}, "Mandelbrot"};
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
                    redraw = true;
                    break;
                } case sf::Event::MouseWheelScrolled: {
                    sf::Vector2u winSize {window.getSize()};
                    sf::Event::MouseWheelScrollEvent scrollEvnt {evnt.mouseWheelScroll};

                    // Compute percentage from top left
                    double scrollXP {static_cast<double>(scrollEvnt.x) / winSize.x}, 
                           scrollYP {static_cast<double>(scrollEvnt.y) / winSize.y};

                    // Convert in terms of RE, IM
                    double deltaX {(MAX_RE - MIN_RE) * scrollXP}, 
                           deltaY {(MAX_IM - MIN_IM) * scrollYP};

                    std::cout << "Zoomed " << (scrollEvnt.delta > 0? "in": "out") << '\n'
                              << "Mouse X: " << MIN_RE + deltaX << ' '
                              << "Mouse Y: " << MAX_IM - deltaY << '\n';

                } default: {}
            }
        }

        // Redraw only when required
        if (redraw) {
            // Get the window height and width and compute step values
            sf::Vector2u winSize {window.getSize()};
            unsigned HEIGHT {winSize.y}, WIDTH {winSize.x};
            double STEP_IM {(MAX_IM - MIN_IM) / HEIGHT}, STEP_RE {(MAX_RE - MIN_RE) / WIDTH};
            image.create(WIDTH, HEIGHT); redraw = false;
            for (unsigned row {0}; row < HEIGHT; row++) {
                for (unsigned col {0}; col < WIDTH; col++) {
                    double re {MIN_RE + col * STEP_RE}, im {MIN_IM + row * STEP_IM};
                    double hue {check({re, im}, MAX_ITER)};
                    image.setPixel(col, row, getRGBColor(hue));
                }
            }

            // Draw the image
            window.clear(sf::Color::Black);
            sf::Texture texture;
            texture.loadFromImage(image);
            window.draw(sf::Sprite{texture});
            window.display();
        }
    }

    return 0;
}
