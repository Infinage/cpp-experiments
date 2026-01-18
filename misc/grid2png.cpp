#include <SFML/Graphics.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/System/Vector2.hpp>
#include <cctype>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

void charGrid2Image(const std::vector<std::vector<char>> &grid, const std::string &fname) {
    // Dimensions & font setup
    constexpr int cellSize {35}, cellBorderWidth {1};
    constexpr char fontPath[] {"/usr/share/fonts/TTF/FiraCodeNerdFontMono-Regular.ttf"};
    const std::size_t rows {grid.size()}, cols {grid[0].size()};
    const std::size_t imageHeight {rows * cellSize}, imageWidth {cols * cellSize};

    // Create a render texture
    sf::Vector2u textureSize {static_cast<unsigned int>(imageWidth), static_cast<unsigned int>(imageHeight)};
    sf::RenderTexture renderTexture{textureSize};

    // Setup font
    sf::Font font;
    if (!font.openFromFile(fontPath))
        throw std::runtime_error{"Failed to load font, please reconfigure."};

    // Clear the texture
    renderTexture.clear(sf::Color::White);

    // Draw the grid
    for (std::size_t i {}; i < rows; ++i) {
        for (std::size_t j {}; j < cols; ++j) {
            // Draw cell background + borders
            sf::RectangleShape cell(sf::Vector2f(cellSize, cellSize));
            cell.setPosition({static_cast<float>(j * cellSize), static_cast<float>(i * cellSize)});
            cell.setFillColor(sf::Color::White);
            cell.setOutlineThickness(cellBorderWidth);
            cell.setOutlineColor(sf::Color::Black);
            renderTexture.draw(cell);

            // Draw the text
            sf::Text letter {font, grid[i][j], cellSize - 10};
            letter.setFillColor(sf::Color::Black);
            letter.setPosition({
                static_cast<float>(j * cellSize) + static_cast<float>(cellSize) / 4,
                static_cast<float>(i * cellSize)
            });
            renderTexture.draw(letter);
        }
    }

    // Flust & save to output file
    renderTexture.display();
    sf::Image img {renderTexture.getTexture().copyToImage()};
    if (!img.saveToFile(fname))
        throw std::runtime_error{"Failed to write to file."};
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cout << "Write a grid of chars to a PNG file, all spaces are stripped.\n"
                  << "Usage: grid2png <inputFname> <outputFname>\n";
    } else {
        std::ifstream ifs{argv[1]};
        if (!ifs) { std::cerr << "Error reading input text file.\n"; return 1; }

        // Read the chars from file
        std::vector<std::vector<char>> grid;
        std::string line;
        while (std::getline(ifs, line)) {
            std::vector<char> lineVec;
            for (char ch: line)
                if (!std::isspace(ch))
                    lineVec.emplace_back(ch);
            grid.emplace_back(lineVec);
        }

        // Write to specified output filename
        charGrid2Image(grid, argv[2]);
        std::cout << "Successfully written to file: '" << argv[2] << "'\n";
    }

    return 0;
}
