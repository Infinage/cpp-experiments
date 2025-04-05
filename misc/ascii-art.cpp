// g++ ascii-art.cpp -o ascii-art -std=c++23 -O2 -lpng

#include "../cli/argparse.hpp"
#include "png_reader.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

// Define density as a seq of chars
constexpr std::string_view DENSITY {" _.,-=+:;cba!?0123456789$W#@Ñ"};

// Map each cell to a corresponding character in the Density string
inline char mapPixel(const double val, const double min, const double max, bool invert = false) {
    double scaled {max > min? (val - min) / (max - min): 0.};
    std::size_t pos {static_cast<std::size_t>(std::round(scaled * (DENSITY.size() - 1)))};
    if (invert) pos = DENSITY.size() - pos - 1;
    return DENSITY[pos];
}

// Apply avg of all the pixels
std::array<std::uint8_t, 4> poolAvg(std::span<std::array<std::uint8_t, 4>> pixels) {
    double R{}, G{}, B{}, A{};
    for (const std::array<std::uint8_t, 4> &pixel: pixels) {
        R += pixel[0]; G += pixel[1];
        B += pixel[2]; A += pixel[3];
    }

    // Avg the result
    double factor {static_cast<double>(pixels.size())};
    return {
        static_cast<std::uint8_t>(R / factor),
        static_cast<std::uint8_t>(G / factor),
        static_cast<std::uint8_t>(B / factor),
        static_cast<std::uint8_t>(A / factor),
    };
}

int main(int argc, char **argv) {
    try {
        // Define the cli args
        argparse::ArgumentParser parser{"ascii-art"};
        parser.description("Generates ASCII visualizations from PNG image input.");
        parser.addArgument("image", argparse::ARGTYPE::POSITIONAL).help("PNG image path. If not provided, reads from STDIN.");
        parser.addArgument("downscale", argparse::ARGTYPE::NAMED).alias("d")
            .help("Downscale the input image by specified factor").defaultValue(short{1});
        parser.addArgument("invert", argparse::ARGTYPE::NAMED).alias("i")
            .help("Invert density mapping").implicitValue(true).defaultValue(false);
        parser.parseArgs(argc, argv);

        // Extract args from CLI
        short downscale {parser.get<short>("downscale")};
        if (downscale <= 0) throw std::runtime_error("Downscale factor cannot be negative.");
        bool invertMapping {parser.get<bool>("invert")};
        std::size_t factor {static_cast<std::size_t>(downscale)};
        std::string filePath {parser.exists("image")? parser.get("image"): ""};

        // Read the image from path if provided, else read from console
        png::Image image;
        if (!filePath.empty()) {
            std::ifstream ifs {filePath.c_str(), std::ios::binary};
            if (!ifs) throw std::runtime_error("File open failed");
             image = png::read(ifs);
        } else {
            std::ostringstream oss; int ch;
            while ((ch = std::cin.get()) != EOF)
                oss.put(static_cast<char>(ch));
            std::istringstream iss {oss.str()};
            image = png::read(iss);
        }

        // Convert RGBA into a single valued 2D vector, store min - max for normalization
        std::size_t opHeight {image.height / factor}, opWidth {image.width / factor};
        std::vector<std::vector<double>> grayscaled(opHeight, std::vector<double>(opWidth));
        double min {std::numeric_limits<double>::max()}, max {std::numeric_limits<double>::min()};
        for (std::size_t i {0}; i < opHeight; i++) {
            for (std::size_t j {0}; j < opWidth; j++) {
                // Gather the pixels within the filter range
                std::vector<std::array<std::uint8_t, 4>> pixels;
                for (std::size_t di {0}; di < factor; di++) {
                    for (std::size_t dj {0}; dj < factor; dj++) {
                        pixels.emplace_back(image(i * factor + di, j * factor + dj));
                    }
                }

                // Apply aggregate function to pool pixels, luma weighted average for grayscaling
                std::array<std::uint8_t, 4> pixel {poolAvg(pixels)};
                double grey {(pixel[3] / 255.) * (.299 * pixel[0] + .587 * pixel[1] + .114 * pixel[2])};
                grayscaled[i][j] = grey; min = std::min(min, grey); max = std::max(max, grey);
            }
        }

        // Map each pixel to char post normalization
        std::string result{};
        result.reserve(grayscaled.size() * (grayscaled[0].size() + 1));
        for (const std::vector<double> &row: grayscaled) {
            for (const double &pixel: row)
                result.push_back(mapPixel(pixel, min, max, invertMapping)) ;
            result.push_back('\n');
        }

        // Print to console
        std::cout << result;
        return 0;
    } 

    catch (std::exception &ex) {
        std::cerr << "ASCII Art Error: " << ex.what() << '\n';
        return 1;
    }
}
