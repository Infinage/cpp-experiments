// $ g++ mandelbrot.cpp -std=c++23 -o mandelbrot -lsfml-graphics -lsfml-window -lsfml-system -O2

/*
 * TODO:
 * 1. Implement `bigfloat` to support infinite zooms
 * 2. Add GPU support for fast rendering
 */

#include <algorithm>
#include <chrono>
#include <vector>

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
        unsigned MAX_ITER = {static_cast<unsigned>(nIters)};

        // Create SFML Window and a pool of threads
        sf::RenderWindow window{sf::VideoMode{800, 600}, "Mandelbrot Set Explorer"};
        ThreadPool<std::function<void()>> pool(static_cast<std::size_t>(nThreads));

        // Define variables to be used inside event loop
        double MIN_RE {-2.}, MAX_RE {1.}, MIN_IM {-1.}, MAX_IM {1.};
        bool isMouseDragged {false}; int oldMouseX {-1}, oldMouseY {-1};

        // Cache image & redraw only on demand
        sf::Texture texture;
        texture.create(800, 600);
        sf::Sprite mandelSprite {texture};
        sf::Clock clk; bool redraw {true}; 
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
                        texture.create(evnt.size.width, evnt.size.height);
                        mandelSprite.setTexture(texture, true);
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

                        // Determine zoom factor - {0.9: zoom in, 1.1: zoom out}
                        double zoom = evnt.mouseWheelScroll.delta > 0? 0.9: 1.1;
                        double reRange {MAX_RE - MIN_RE}, imRange {MAX_IM - MIN_IM}; 

                        // Prevent zoom in beyond the point it gets blurry
                        if ((reRange < 5e-14 || imRange < 5e-14) && zoom == 0.9) break;

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
                        double reRange = MAX_RE - MIN_RE, imRange = MAX_IM - MIN_IM;

                        // Compute new bounds - ensuring we don't go out of bounds
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
                    } case sf::Event::KeyPressed: {
                        double reRange = MAX_RE - MIN_RE, imRange = MAX_IM - MIN_IM;
                        sf::Keyboard::Key key {evnt.key.code};
                        if (isInKeys(key, {sf::Keyboard::Add, sf::Keyboard::Equal, sf::Keyboard::Subtract, sf::Keyboard::Hyphen})) {
                            // Prevent zoom in beyond the point it gets blurry
                            double zoom = isInKeys(key, {sf::Keyboard::Add, sf::Keyboard::Equal}) > 0? 0.9: 1.1;
                            if ((reRange < 5e-14 || imRange < 5e-14) && zoom == 0.9) break;

                            // Compute new bounds - ensuring we don't go out of bounds
                            double centerRE {MIN_RE + (MAX_RE - MIN_RE) * 0.5}; 
                            double centerIM {MIN_IM + (MAX_IM - MIN_IM) * 0.5};
                            MIN_RE = std::max(centerRE - (centerRE - MIN_RE) * zoom, -2.); 
                            MAX_RE = std::min(MIN_RE + reRange * zoom, 1.);
                            MIN_IM = std::max(centerIM - (centerIM - MIN_IM) * zoom, -1.); 
                            MAX_IM = std::min(MIN_IM + imRange * zoom, 1.);
                            redraw = true;
                        }

                        else if (isInKeys(key, {sf::Keyboard::Up, sf::Keyboard::Down, sf::Keyboard::Left, sf::Keyboard::Right})) {
                            // Assign delta based on key press
                            double delta {0.1}, deltaX {0}, deltaY {0};
                            if      (key ==    sf::Keyboard::Up) deltaY =  delta;
                            else if (key ==  sf::Keyboard::Down) deltaY = -delta;
                            else if (key ==  sf::Keyboard::Left) deltaX =  delta;
                            else if (key == sf::Keyboard::Right) deltaX = -delta;

                            // Ensure we stay in range
                            MIN_RE = std::max(MIN_RE - deltaX * reRange, -2.);
                            MAX_RE = std::min(MAX_RE - deltaX * reRange, 1.);
                            MIN_IM = std::max(MIN_IM - deltaY * imRange, -1.); 
                            MAX_IM = std::min(MAX_IM - deltaY * imRange, 1.);

                            // Prevent stretching
                            if (MIN_RE == -2.) MAX_RE = MIN_RE + reRange;
                            else if (MAX_RE == 1.) MIN_RE = MAX_RE - reRange;
                            if (MIN_IM == -1.) MAX_IM = MIN_IM + imRange;
                            else if (MAX_IM == 1.) MIN_IM = MAX_IM - imRange;
                            redraw = true;
                        }

                        else if (key == sf::Keyboard::Z) {
                            // Reset the coordinates
                            MIN_RE = -2., MAX_RE = 1., MIN_IM = -1., MAX_IM = 1.;
                            redraw = true;
                        }

                        else if (key == sf::Keyboard::S) {
                            // Take a screenshot and save to png file with timestamp
                            sf::Vector2u winSize {window.getSize()};
                            sf::Texture screenshotTexture;
                            screenshotTexture.create(winSize.x, winSize.y);
                            screenshotTexture.update(window);
                            sf::Image screenshot {screenshotTexture.copyToImage()};
                            std::chrono::time_point now {std::chrono::system_clock::now()};
                            long ms {std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()};
                            screenshot.saveToFile("screenshot-" + std::to_string(ms) + ".png");

                            // Feedback for screenshot
                            sf::RectangleShape flashRect{};
                            flashRect.setSize(static_cast<sf::Vector2f>(winSize));
                            flashRect.setFillColor(sf::Color::White);
                            window.draw(flashRect);
                            window.display(); sf::sleep(sf::milliseconds(10));
                            redraw = true;
                        }

                         break;
                    } default: {}
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
                std::vector<std::function<void(void)>> tasks;
                for (unsigned row {0}; row < HEIGHT; row++) {
                    tasks.emplace_back([WIDTH, MIN_RE, STEP_RE, MIN_IM, STEP_IM, row, MAX_ITER, &image]() {
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

                // Burst enqueue to minimize lock contention
                pool.enqueueAll(tasks);
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
