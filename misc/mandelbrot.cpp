/*
 * TODO:
 * 3. Add GPU support for fast rendering
 */

#include <algorithm>
#include <chrono>

#include <SFML/Graphics.hpp>
#include <cmath>

#include "../cli/argparse.hpp"

// Impl agnostic helpers
using BigFloat = long double;
constexpr BigFloat MIN_STEP = 5e-16;
BigFloat max(BigFloat f1, BigFloat f2) { return f1 > f2? f1: f2; }
BigFloat min(BigFloat f1, BigFloat f2) { return f1 < f2? f1: f2; }
bool equals(BigFloat f1, BigFloat f2) {
    constexpr double EPS = 1e-15;
    return std::fabs(f1 - f2) < EPS;
}

/*
 * Computes the escape-time for a point `c = cre + cim*i` in the Mandelbrot set.
 *
 * The Mandelbrot iteration is defined as:
 *   z₀ = 0
 *   zₙ₊₁ = zₙ² + c
 *
 * The function returns the number of iterations required for |zₙ| to exceed 2,
 * capped at `maxIterations`. If the cap is reached, the point is assumed to be
 * inside the Mandelbrot set.
 *
 * To improve performance, we first test whether `c` lies within regions that are
 * mathematically proven to be inside the Mandelbrot set:
 *
 *   1. The main cardioid (stable fixed point)
 *   2. The period-2 bulb (stable 2-cycle)
 *
 * Points inside these regions are guaranteed not to escape, so iteration can be
 * skipped entirely.
 */
template<typename Float>
unsigned check(Float cre, Float cim, unsigned maxIterations) {
    // --- Interior tests (early exit) ---
    // Main cardioid test
    Float x = cre - Float(0.25);
    Float q = x * x + cim * cim;
    if (q * (q + x) < 0.25 * cim * cim)
        return maxIterations;

    // Period-2 bulb test
    if ((cre + 1.0) * (cre + 1.0) + cim * cim < 0.0625)
        return maxIterations;

    // --- Escape iteration ---
    Float re = 0.0, im = 0.0;
    Float old_re = 0.0, old_im = 0.0;
    unsigned iters = 0;
    while (re * re + im * im < 4.0 && iters < maxIterations) {
        Float new_im = Float(2.0) * re * im + cim;
        Float new_re = re * re - im * im + cre;
        re = new_re; im = new_im;

        // Check if periodically repeating
        if (!(++iters & 31)) {
            if (equals(re, old_re) && equals(im, old_im))
                return maxIterations;
            old_re = re, old_im = im;
        }
    }
    return iters;
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

        // Parse the args
        parser.parseArgs(argc, argv);
        int nIters {parser.get<int>("n_iters")}; 
        double rRate {parser.get<double>("refresh_rate")};

        // Info on console
        std::cout << "n_iters: " << nIters << "; "
                  << "refresh_rate: " << rRate << "\n";

        // Start with predetermined height and width, change on user interaction
        auto MAX_ITER = static_cast<unsigned>(nIters);

        // Create SFML Window and a pool of threads
        sf::RenderWindow window{sf::VideoMode{{800, 600}}, "Mandelbrot Set Explorer"};

        // Define variables to be used inside event loop
        BigFloat MIN_RE = -2., MAX_RE = 1., MIN_IM = -1., MAX_IM = 1.;
        bool isMouseDragged {false}; int oldMouseX {-1}, oldMouseY {-1};

        // Cache image & redraw only on demand
        sf::Texture texture{{800, 600}};
        sf::Sprite mandelSprite {texture};
        sf::Clock clk; bool redraw {true}; 
        auto normalCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Arrow);
        auto handCursor = sf::Cursor::createFromSystem(sf::Cursor::Type::Hand);
        if (!normalCursor || !handCursor) throw std::runtime_error{"Cursors not supported"};
        window.setMouseCursor(*normalCursor);

        // Placeholder to hold the pixels
        auto winSize = window.getSize();
        std::vector<std::uint8_t> image(winSize.x * winSize.y * 4);

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
                    auto winSize = window.getSize();
                    image.resize(winSize.x * winSize.y * 4);
                    if (!texture.resize(ev->size)) std::cerr << "Texture resize event failed\n";
                    mandelSprite.setTexture(texture, true);
                    redraw = true;
                } else if (const auto *ev = event->getIf<sf::Event::MouseWheelScrolled>()) {
                    // Compute percentage from top left
                    sf::Vector2u winSize = window.getSize();
                    double scrollXP = static_cast<double>(ev->position.x) / winSize.x;
                    double scrollYP = static_cast<double>(ev->position.y) / winSize.y;

                    // Convert in terms of RE, IM
                    BigFloat currRE = MIN_RE + (MAX_RE - MIN_RE) * scrollXP;
                    BigFloat currIM = MIN_IM + (MAX_IM - MIN_IM) * scrollYP;

                    // Determine zoom factor - {0.9: zoom in, 1.1: zoom out}
                    double zoom = ev->delta > 0 ? 0.9 : 1.1;
                    BigFloat reRange = MAX_RE - MIN_RE, imRange = MAX_IM - MIN_IM;

                    // Prevent zoom in beyond the point it gets blurry
                    if ((reRange < MIN_STEP || imRange < MIN_STEP) && zoom < 1.0) continue;

                    // Compute new bounds - ensuring we don't go out of bounds
                    MIN_RE = max(currRE - (currRE - MIN_RE) * zoom, BigFloat(-2.0));
                    MAX_RE = min(MIN_RE + reRange * zoom, BigFloat(1.0));
                    MIN_IM = max(currIM - (currIM - MIN_IM) * zoom, BigFloat(-1.0));
                    MAX_IM = min(MIN_IM + imRange * zoom, BigFloat(1.0));
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
                    BigFloat reRange = MAX_RE - MIN_RE;
                    BigFloat imRange = MAX_IM - MIN_IM;

                    // Compute new bounds - ensuring we don't go out of bounds
                    MIN_RE = max(MIN_RE - deltaX * reRange, BigFloat(-2.0));
                    MAX_RE = min(MAX_RE - deltaX * reRange, BigFloat(1.0));
                    MIN_IM = max(MIN_IM - deltaY * imRange, BigFloat(-1.0));
                    MAX_IM = min(MAX_IM - deltaY * imRange, BigFloat(1.0));

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
                    BigFloat reRange = MAX_RE - MIN_RE, imRange = MAX_IM - MIN_IM;
                    if (isInKeys(key, {sf::Keyboard::Key::Add, sf::Keyboard::Key::Equal,
                                       sf::Keyboard::Key::Subtract, sf::Keyboard::Key::Hyphen})) {
                        // Prevent zoom in beyond the point it gets blurry
                        double zoom = isInKeys(key, {sf::Keyboard::Key::Add, sf::Keyboard::Key::Equal}) ? 0.9 : 1.1;
                        if ((reRange < 5e-14 || imRange < 5e-14) && zoom < 1.0) continue;

                        // Compute new bounds - ensuring we don't go out of bounds
                        BigFloat centerRE = (MIN_RE + MAX_RE) * 0.5;
                        BigFloat centerIM = (MIN_IM + MAX_IM) * 0.5;
                        MIN_RE = max(centerRE - (centerRE - MIN_RE) * zoom, BigFloat(-2.0));
                        MAX_RE = min(MIN_RE + reRange * zoom, BigFloat(1.0));
                        MIN_IM = max(centerIM - (centerIM - MIN_IM) * zoom, BigFloat(-1.0));
                        MAX_IM = min(MIN_IM + imRange * zoom, BigFloat(1.0));
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
                        MIN_RE = max(MIN_RE - dx * reRange, BigFloat(-2.0));
                        MAX_RE = min(MAX_RE - dx * reRange, BigFloat(1.0));
                        MIN_IM = max(MIN_IM - dy * imRange, BigFloat(-1.0));
                        MAX_IM = min(MAX_IM - dy * imRange, BigFloat(1.0));

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
                        auto ms {std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()};
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

                // Get the window height and width
                sf::Vector2u winSize {window.getSize()};
                const unsigned HEIGHT {winSize.y}, WIDTH {winSize.x};
                const BigFloat STEP_IM {(MAX_IM - MIN_IM) / HEIGHT}, 
                       STEP_RE {(MAX_RE - MIN_RE) / WIDTH};

                // Compute step values
                constexpr unsigned TILE = 32;
                unsigned tilesX = ( WIDTH + (TILE - 1)) / TILE;
                unsigned tilesY = (HEIGHT + (TILE - 1)) / TILE;
                # pragma omp parallel for simd schedule(guided)
                for (unsigned tile = 0; tile < tilesX * tilesY; ++tile) {
                    auto tileRow = tile / tilesX, tileCol = tile % tilesX;
                    auto cstart = tileCol * TILE, rstart = tileRow * TILE;
                    auto cend = std::min(cstart + TILE,  WIDTH);
                    auto rend = std::min(rstart + TILE, HEIGHT);
                    for (unsigned row = rstart; row < rend; ++row) {
                        BigFloat im {MIN_IM + row * STEP_IM};
                        for (unsigned col = cstart; col < cend; ++col) {
                            BigFloat re {MIN_RE + col * STEP_RE};
                            unsigned iters {check(re, im, MAX_ITER)};
                            std::size_t start {(row * WIDTH + col) * 4};
                            auto [R, G, B] {getRGBColor(iters, MAX_ITER)};
                            image[start + 0] = R; image[start + 1] = G;
                            image[start + 2] = B; image[start + 3] = 255;
                        }
                    }
                }

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
