// $ g++ mandelbrot.cpp -std=c++23 -o mandelbrot -lsfml-graphics -lsfml-window -lsfml-system -O2

/*
 * TODO:
 * 1. Implement `bigfloat` to support infinite zooms
 * 2. Add GPU support for fast rendering
 */

#include <algorithm>
#include <chrono>

#include <SFML/Graphics.hpp>

#include "../cli/argparse.hpp"
#include "../misc/threadPool.hpp"

/* 
 * Determines how quickly the complex number `cre + cim*i` diverges under the Mandelbrot iteration.
 * Returns the number of iterations before divergence (max capped at `maxIterations`)
 *
 * Mandelbrot Equation: f(z) = z^2 + C
 * For complex number z = a + bi:
 *   z^2   = a^2 - b^2 + 2ab*i
 *   Real  = a^2 - b^2
 *   Imag  = 2ab
 *
 * On each iteration, update `re` and `im` based on z^2 + C
 */
unsigned check(const double cre, const double cim, const unsigned maxIterations = 500) {
    double re {cre}, im {cim}, re2 {cre * cre}, im2 {cim * cim}; 
    unsigned iterations {0};
    while (re2 + im2 < 4. && iterations < maxIterations) {
        // `im` first otherwise modified `re`
        // would impact `im` calculation
        im = 2. * re * im + cim;
        re = re2 - im2 + cre;

        // Recompute
        re2 = re * re;
        im2 = im * im;

        // Incr iters
        iterations++;
    }
    return iterations;
}

// Helper function to initialize a color pallete
constexpr std::array<std::array<std::uint8_t, 3>, 16> initPallete() {
    std::array<std::array<std::uint8_t, 3>, 16> pallete {{
        { 66,  30,  15},
        { 25,   7,  26},
        {  9,   1,  47},
        {  4,   4,  73},
        {  0,   7, 100},
        { 12,  44, 138},
        { 24,  82, 177},
        { 57, 125, 209},
        {134, 181, 229},
        {211, 236, 248},
        {241, 233, 191},
        {248, 201,  95},
        {255, 170,   0},
        {204, 128,   0},
        {153,  87,   0},
        {106,  52,   3},
    }};
    return pallete;
}

// Translate hue to RGB equivalent
inline std::array<std::uint8_t, 3> getRGBColor(unsigned iters, unsigned maxIters) {
    constexpr auto pallete = initPallete();
    if (iters < maxIters && iters > 0)
        return pallete[iters % pallete.size()];
    return {0, 0, 0};
}

// Helper to check if a key is present in a list
template<typename T> requires (std::is_fundamental_v<T> || std::is_enum_v<T>)
bool isInKeys(T key, std::initializer_list<T> keys) {
    return std::find(keys.begin(), keys.end(), key) != keys.end(); 
}

int main(int argc, char **argv) {
    try {
        // Specify command line args
        argparse::ArgumentParser parser{"mandelbrot"};
        parser.description(
            "Draws the Mandelbrot set on screen with interactive controls.\n"
            "Use mouse scroll or +/- keys to zoom in/out (zoom depth is limited by double precision).\n"
            "Pan the view by dragging the mouse or using arrow keys.\n"
            "Press 'z' to reset zoom level, and 's' to save a screenshot to the current directory."
        );
        parser.addArgument("n_iters", argparse::ARGTYPE::NAMED).alias("n").defaultValue(500)
            .help("Iterations to run for checking divergence. Must be between (0, 5000]")
            .validate<int>([](int nIters) { return 0 < nIters && nIters <= 5000; });
        parser.addArgument("refresh_rate", argparse::ARGTYPE::NAMED).alias("r").defaultValue(0.05)
            .help("Image is refreshed every `refresh_rate` seconds. Must be in float, eg: 1.")
            .validate<double>([](double rRate) { return rRate > 0.; });
        parser.addArgument("n_workers", argparse::ARGTYPE::NAMED).alias("j").defaultValue(4)
            .help("No. of concurrent threads to use for rendering image. Must be betweeen (0, 30]")
            .validate<int>([](int nThreads) { return 0 < nThreads && nThreads <= 30; });

        // Parse the args
        parser.parseArgs(argc, argv);
        int nIters {parser.get<int>("n_iters")}, nThreads {parser.get<int>("n_workers")}; 
        double rRate {parser.get<double>("refresh_rate")};

        // Info on console
        std::cout << "n_iters: " << nIters << "; "
                  << "refresh_rate: " << rRate << "; "
                  << "n_workers: " << nThreads << '\n';

        // Start with predetermined height and width, change on user interaction
        auto MAX_ITER = static_cast<unsigned>(nIters);

        // Create SFML Window and a pool of threads
        sf::RenderWindow window{sf::VideoMode{{800, 600}}, "Mandelbrot Set Explorer"};
        async::ThreadPool pool(static_cast<std::size_t>(nThreads));

        // Define variables to be used inside event loop
        double MIN_RE {-2.}, MAX_RE {1.}, MIN_IM {-1.}, MAX_IM {1.};
        bool isMouseDragged {false}; int oldMouseX {-1}, oldMouseY {-1};

        // Cache image & redraw only on demand
        sf::Texture texture{{800, 600}};
        sf::Sprite mandelSprite {texture};
        sf::Clock clk; bool redraw {true}; 
        auto normalCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Arrow);
        auto handCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Hand);
        if (!normalCursor || !handCursor) throw std::runtime_error{"Cursors not supported"};
        window.setMouseCursor(*normalCursor);

        // Run event loop
        while (window.isOpen()) {
            // Poll events and respond to user interaction
            while (auto event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) {
                    window.close();
                } else if (const auto *ev = event->getIf<sf::Event::Resized>()) {
                    auto width  = static_cast<float>(ev->size.x);
                    auto height = static_cast<float>(ev->size.y);
                    window.setView(sf::View{sf::FloatRect{{0.f, 0.f}, {width, height}}});
                    if (!texture.resize(ev->size)) std::cerr << "Texture resize event failed\n";
                    mandelSprite.setTexture(texture, true);
                    redraw = true;
                } else if (const auto *ev = event->getIf<sf::Event::MouseWheelScrolled>()) {
                    // Compute percentage from top left
                    sf::Vector2u winSize = window.getSize();
                    double scrollXP = static_cast<double>(ev->position.x) / winSize.x;
                    double scrollYP = static_cast<double>(ev->position.y) / winSize.y;

                    // Convert in terms of RE, IM
                    double currRE = MIN_RE + (MAX_RE - MIN_RE) * scrollXP;
                    double currIM = MIN_IM + (MAX_IM - MIN_IM) * scrollYP;

                    // Determine zoom factor - {0.9: zoom in, 1.1: zoom out}
                    double zoom = ev->delta > 0 ? 0.9 : 1.1;
                    double reRange = MAX_RE - MIN_RE;
                    double imRange = MAX_IM - MIN_IM;

                    // Prevent zoom in beyond the point it gets blurry
                    if ((reRange < 5e-14 || imRange < 5e-14) && zoom < 1.0) continue;

                    // Compute new bounds - ensuring we don't go out of bounds
                    MIN_RE = std::max(currRE - (currRE - MIN_RE) * zoom, -2.0);
                    MAX_RE = std::min(MIN_RE + reRange * zoom, 1.0);
                    MIN_IM = std::max(currIM - (currIM - MIN_IM) * zoom, -1.0);
                    MAX_IM = std::min(MIN_IM + imRange * zoom, 1.0);
                    redraw = true;
                } else if (const auto *ev = event->getIf<sf::Event::MouseButtonPressed>()) {
                    if (ev->button == sf::Mouse::Button::Left) {
                        isMouseDragged = true;
                        oldMouseX = ev->position.x;
                        oldMouseY = ev->position.y;
                        window.setMouseCursor(*handCursor);
                    }
                } else if (const auto *ev = event->getIf<sf::Event::MouseButtonReleased>()) {
                    if (ev->button == sf::Mouse::Button::Left) {
                        isMouseDragged = false;
                        window.setMouseCursor(*normalCursor);
                    }
                } else if (const auto *ev = event->getIf<sf::Event::MouseMoved>()) {
                    if (!isMouseDragged) continue;

                    // Compute delta b/w current & prev mouse pos
                    sf::Vector2u winSize = window.getSize();
                    double deltaX = static_cast<double>(ev->position.x - oldMouseX) / winSize.x;
                    double deltaY = static_cast<double>(ev->position.y - oldMouseY) / winSize.y;
                    double reRange = MAX_RE - MIN_RE;
                    double imRange = MAX_IM - MIN_IM;

                    // Compute new bounds - ensuring we don't go out of bounds
                    MIN_RE = std::max(MIN_RE - deltaX * reRange, -2.0);
                    MAX_RE = std::min(MAX_RE - deltaX * reRange, 1.0);
                    MIN_IM = std::max(MIN_IM - deltaY * imRange, -1.0);
                    MAX_IM = std::min(MAX_IM - deltaY * imRange, 1.0);

                    // Above might end up stretching the range - lets ensure it doesn't
                    if (MIN_RE == -2.0) MAX_RE = MIN_RE + reRange;
                    else if (MAX_RE == 1.0) MIN_RE = MAX_RE - reRange;
                    if (MIN_IM == -1.0) MAX_IM = MIN_IM + imRange;
                    else if (MAX_IM == 1.0) MIN_IM = MAX_IM - imRange;

                    oldMouseX = ev->position.x;
                    oldMouseY = ev->position.y;

                    redraw = true;
                } else if (const auto *ev = event->getIf<sf::Event::KeyPressed>()) {
                    sf::Keyboard::Key key = ev->code;
                    double reRange = MAX_RE - MIN_RE, imRange = MAX_IM - MIN_IM;
                    if (isInKeys(key, {sf::Keyboard::Key::Add, sf::Keyboard::Key::Equal,
                                       sf::Keyboard::Key::Subtract, sf::Keyboard::Key::Hyphen})) {
                        // Prevent zoom in beyond the point it gets blurry
                        double zoom = isInKeys(key, {sf::Keyboard::Key::Add, sf::Keyboard::Key::Equal}) ? 0.9 : 1.1;
                        if ((reRange < 5e-14 || imRange < 5e-14) && zoom < 1.0) continue;

                        // Compute new bounds - ensuring we don't go out of bounds
                        double centerRE = (MIN_RE + MAX_RE) * 0.5;
                        double centerIM = (MIN_IM + MAX_IM) * 0.5;
                        MIN_RE = std::max(centerRE - (centerRE - MIN_RE) * zoom, -2.0);
                        MAX_RE = std::min(MIN_RE + reRange * zoom, 1.0);
                        MIN_IM = std::max(centerIM - (centerIM - MIN_IM) * zoom, -1.0);
                        MAX_IM = std::min(MIN_IM + imRange * zoom, 1.0);
                        redraw = true;
                    } else if (isInKeys(key, {sf::Keyboard::Key::Up, sf::Keyboard::Key::Down,
                                            sf::Keyboard::Key::Left, sf::Keyboard::Key::Right})) {
                        // Assign delta based on key press
                        double delta {0.1}, dx {0}, dy {0};
                        if (key == sf::Keyboard::Key::Up)    dy =  delta;
                        if (key == sf::Keyboard::Key::Down)  dy = -delta;
                        if (key == sf::Keyboard::Key::Left)  dx =  delta;
                        if (key == sf::Keyboard::Key::Right) dx = -delta;

                        // Ensure we stay in range
                        MIN_RE = std::max(MIN_RE - dx * reRange, -2.0);
                        MAX_RE = std::min(MAX_RE - dx * reRange, 1.0);
                        MIN_IM = std::max(MIN_IM - dy * imRange, -1.0);
                        MAX_IM = std::min(MAX_IM - dy * imRange, 1.0);

                        // Prevent stretching
                        if (MIN_RE == -2.) MAX_RE = MIN_RE + reRange;
                        else if (MAX_RE == 1.) MIN_RE = MAX_RE - reRange;
                        if (MIN_IM == -1.) MAX_IM = MIN_IM + imRange;
                        else if (MAX_IM == 1.) MIN_IM = MAX_IM - imRange;
                        redraw = true;
                    } else if (key == sf::Keyboard::Key::Z) {
                        // Reset the coordinates
                        MIN_RE = -2.0, MAX_RE = 1.0, MIN_IM = -1.0, MAX_IM = 1.0;
                        redraw = true;
                    } else if (key == sf::Keyboard::Key::S) {
                        // Take a screenshot and save to png file with timestamp
                        sf::Texture screenshotTexture{window.getSize()};
                        screenshotTexture.update(window);
                        sf::Image screenshot {screenshotTexture.copyToImage()};
                        std::chrono::time_point now {std::chrono::system_clock::now()};
                        long ms {std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()};
                        if (!screenshot.saveToFile("screenshot-" + std::to_string(ms) + ".png"))
                            std::cerr <<  "Failed to write screenshot";

                        // Feedback for screenshot
                        sf::RectangleShape flashRect{};
                        flashRect.setSize(sf::Vector2f{window.getSize()});
                        flashRect.setFillColor(sf::Color::White);
                        window.draw(flashRect);
                        window.display(); sf::sleep(sf::milliseconds(10));
                        redraw = true;
                    }
                }
            }

            // Redraw only when required
            if (redraw) {
                // Do nothing if we haven't hit the theshold yet
                if (clk.getElapsedTime().asSeconds() < rRate) continue;
                else clk.restart(); redraw = false;

                // Get the window height and width and compute step values
                sf::Vector2u winSize {window.getSize()};
                unsigned HEIGHT {winSize.y}, WIDTH {winSize.x};
                double STEP_IM {(MAX_IM - MIN_IM) / HEIGHT}, STEP_RE {(MAX_RE - MIN_RE) / WIDTH};
                std::vector<std::uint8_t> image(WIDTH * HEIGHT * 4);
                for (unsigned row {0}; row < HEIGHT; row++) {
                    pool.enqueue([WIDTH, MIN_RE, STEP_RE, MIN_IM, STEP_IM, row, MAX_ITER, &image]() {
                        for (unsigned col {0}; col < WIDTH; col++) {
                            double re {MIN_RE + col * STEP_RE}, im {MIN_IM + row * STEP_IM};
                            unsigned iters {check(re, im, MAX_ITER)};
                            std::size_t start {(row * WIDTH + col) * 4};
                            auto [R, G, B] {getRGBColor(iters, MAX_ITER)};
                            image[start + 0] = R; image[start + 1] = G; 
                            image[start + 2] = B; image[start + 3] = 255;
                        }
                    });
                }

                // Wait for pool to complete executing
                pool.wait();

                // Update the image
                texture.update(image.data());
                window.clear(sf::Color::Black);
                window.draw(mandelSprite);
                window.display();
            }
        }

        return 0;
    }

    catch (std::exception &ex) {
        std::cerr << "Fatal> " << ex.what() << '\n';
        return 1;
    }
}
