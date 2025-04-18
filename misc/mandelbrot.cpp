// $ g++ mandelbrot.cpp -std=c++23 -o mandelbrot -lsfml-graphics -lsfml-window -lsfml-system

/*
 * TODO:
 * 1. Implement `bigfloat` to support infinite zooms?
 */

#include <algorithm>
#include <complex>
#include <stdexcept>

#include <SFML/Graphics.hpp>

#include "../cli/argparse.hpp"
#include "../misc/threadPool.hpp"

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
    parser.addArgument("refresh_rate", argparse::ARGTYPE::NAMED).alias("r").defaultValue(0.25)
        .help("Image is refreshed every <refresh_rate> seconds. Must be in float, eg: 1.");
    parser.addArgument("n_workers", argparse::ARGTYPE::NAMED).alias("j").defaultValue(4)
        .help("No. of concurrent threads to use for rendering image.");

    // Parse & validate args
    parser.parseArgs(argc, argv);
    int nIters {parser.get<int>("n_iters")}, nThreads {parser.get<int>("n_workers")}; 
    double rRate {parser.get<double>("refresh_rate")};
    if (nIters <= 0 || nIters > 1000) throw std::runtime_error("`n_iters` must be between (0, 1000]");
    if (rRate < 0) throw std::runtime_error("`refresh_rate` cannot be less than 0.");
    if (nThreads < 0) throw std::runtime_error("`n_workers` cannot be less than 0.");

    // Info on console
    std::cout << "n_iters: " << nIters << "; "
              << "refresh_rate: " << rRate << "; "
              << "n_workers: " << nThreads << '\n';

    // Start with predetermined height and width, change on user interaction
    unsigned MAX_ITER = {static_cast<unsigned>(nIters)};

    // Create SFML Window and a pool of threads
    sf::RenderWindow window{sf::VideoMode{800, 600}, "Mandelbrot"};
    ThreadPool<std::function<void()>> pool(static_cast<std::size_t>(nThreads));

    // Define variables to be used inside event loop
    double MIN_RE {-2.}, MAX_RE {1.}, MIN_IM {-1.}, MAX_IM {1.};
    bool isMouseDragged {false}; int oldMouseX {-1}, oldMouseY {-1};

    // Cache image & redraw only on demand
    sf::Image image; bool redraw {true}; sf::Clock clk;
    sf::Cursor normalCursor, handCursor;
    normalCursor.loadFromSystem(sf::Cursor::Arrow);
    handCursor.loadFromSystem(sf::Cursor::SizeAll);
    window.setMouseCursor(normalCursor);

    // Run event loop
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
                    // Compute percentage from top left
                    sf::Vector2u winSize {window.getSize()};
                    double scrollXP {static_cast<double>(evnt.mouseWheelScroll.x) / winSize.x};
                    double scrollYP {static_cast<double>(evnt.mouseWheelScroll.y) / winSize.y};

                    // Convert in terms of RE, IM
                    double currRE {MIN_RE + (MAX_RE - MIN_RE) * scrollXP}; 
                    double currIM {MIN_IM + (MAX_IM - MIN_IM) * scrollYP};

                    // Determine zoom factor
                    double reRange {MAX_RE - MIN_RE}, imRange {MAX_IM - MIN_IM}, zoom = evnt.mouseWheelScroll.delta > 0? 0.9: 1.1;

                    // Compute new bounds - ensuring we don't go out of bounds
                    MIN_RE = std::max(currRE - (currRE - MIN_RE) * zoom, -2.); 
                    MAX_RE = std::min(MIN_RE + reRange * zoom, 1.);
                    MIN_IM = std::max(currIM - (currIM - MIN_IM) * zoom, -1.); 
                    MAX_IM = std::min(MIN_IM + imRange * zoom, 1.);
                    redraw = true; break;
                } case sf::Event::MouseButtonPressed: {
                    if (evnt.mouseButton.button == 0) {
                        isMouseDragged = true;
                        oldMouseX = evnt.mouseButton.x;
                        oldMouseY = evnt.mouseButton.y;
                        window.setMouseCursor(handCursor);
                    } break;
                } case sf::Event::MouseButtonReleased: {
                    if (evnt.mouseButton.button == 0) {
                        isMouseDragged = false;
                        window.setMouseCursor(normalCursor);
                    }
                    break;
                } case sf::Event::MouseMoved: {
                    if (!isMouseDragged) break;

                    // Compute delta b/w current & prev mouse pos
                    sf::Vector2u winSize {window.getSize()};
                    double deltaX {static_cast<double>(evnt.mouseMove.x - oldMouseX) / winSize.x};
                    double deltaY {static_cast<double>(evnt.mouseMove.y - oldMouseY) / winSize.y};

                    // Compute new bounds - ensuring we don't go out of bounds
                    double reRange = MAX_RE - MIN_RE; double imRange = MAX_IM - MIN_IM;
                    MIN_RE = std::max(MIN_RE - deltaX * reRange, -2.);
                    MAX_RE = std::min(MAX_RE - deltaX * reRange, 1.);
                    MIN_IM = std::max(MIN_IM - deltaY * imRange, -1.); 
                    MAX_IM = std::min(MAX_IM - deltaY * imRange, 1.);

                    // Above might end up stretching the range - lets ensure it doesn't
                    if (MIN_RE == -2.) MAX_RE = MIN_RE + reRange;
                    else if (MAX_RE == 1.) MIN_RE = MAX_RE - reRange;
                    if (MIN_IM == -1.) MAX_IM = MIN_IM + imRange;
                    else if (MAX_IM == 1.) MIN_IM = MAX_IM - imRange;

                    oldMouseX = evnt.mouseMove.x;
                    oldMouseY = evnt.mouseMove.y;

                    redraw = true; break;
                } default: {}
            }
        }

        // Redraw only when required
        if (redraw) {
            // Do nothing if we haven't hit the theshold yet
            if (clk.getElapsedTime().asSeconds() < rRate) continue;
            else clk.restart();

            // Get the window height and width and compute step values
            sf::Vector2u winSize {window.getSize()};
            unsigned HEIGHT {winSize.y}, WIDTH {winSize.x};
            double STEP_IM {(MAX_IM - MIN_IM) / HEIGHT}, STEP_RE {(MAX_RE - MIN_RE) / WIDTH};
            image.create(WIDTH, HEIGHT); redraw = false;
            unsigned maxBatchSize {(HEIGHT * WIDTH) / static_cast<unsigned>(nThreads)};
            std::vector<std::tuple<unsigned, unsigned, double, double>> batch;
            for (unsigned row {0}; row < HEIGHT; row++) {
                for (unsigned col {0}; col < WIDTH; col++) {
                    double re {MIN_RE + col * STEP_RE}, im {MIN_IM + row * STEP_IM};
                    batch.emplace_back(row, col, re, im);
                    if (batch.size() == maxBatchSize) {
                        pool.enqueue([MAX_ITER, batch = std::move(batch), &image]() {
                            for (auto [row_, col_, re_, im_]: batch) {
                                double hue {check({re_, im_}, MAX_ITER)};
                                image.setPixel(col_, row_, getRGBColor(hue));
                            }
                        });
                        batch.clear();
                    }
                }
            }

            // Queue the remaining batch if any
            if (!batch.empty()) {
                pool.enqueue([MAX_ITER, batch, &image]() {
                    for (auto [row_, col_, re_, im_]: batch) {
                        double hue {check({re_, im_}, MAX_ITER)};
                        image.setPixel(col_, row_, getRGBColor(hue));
                    }
                });
            }

            // Draw the image
            pool.wait();
            window.clear(sf::Color::Black);
            sf::Texture texture;
            texture.loadFromImage(image);
            window.draw(sf::Sprite{texture});
            window.display();
        }
    }

    return 0;
}
