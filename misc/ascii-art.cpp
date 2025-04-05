// g++ ascii-art.cpp -o ascii-art -std=c++23 -O2 -lpng

#include "../cli/argparse.hpp"
#include "png_reader.hpp"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

// Define density as a seq of chars
constexpr std::string_view DENSITY {" _.,-=+:;cba!?0123456789$W#@"};

// Map each cell to a corresponding character in the Density string
inline char mapPixel(const double val, const double min, const double max) {
    double scaled {max > min? (val - min) / (max - min): 0.};
    std::size_t pos {static_cast<std::size_t>(std::round(scaled * DENSITY.size() - 1))};
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

// Apply max of all the pixels
std::array<std::uint8_t, 4> poolMax(std::span<std::array<std::uint8_t, 4>> pixels) {
    std::array<std::uint8_t, 4> result{};
    for (const std::array<std::uint8_t, 4> &pixel: pixels)
        for (std::size_t i {0}; i < 4; i++)
            result[i] = std::max(result[i], pixel[i]);
    return result;
}

int main(int argc, char **argv) {
    // Define the cli args
    argparse::ArgumentParser parser{"ascii-art"};
    parser.description("Generates ASCII visualizations from PNG image input.");
    parser.addArgument("image", argparse::ARGTYPE::POSITIONAL)
        .help("PNG image path").required();
    parser.addArgument("downscale", argparse::ARGTYPE::NAMED).alias("d")
        .help("Downscale the input image by specified factor").defaultValue(short{1});
    parser.addArgument("pool-func", argparse::ARGTYPE::NAMED).alias("f")
        .help("Function to apply for downscaling - avg/max").defaultValue("avg");
    parser.parseArgs(argc, argv);

    // Extract args from CLI
    short downscale {parser.get<short>("downscale")};
    std::string poolFunc {parser.get("pool-func")};
    std::size_t factor {static_cast<std::size_t>(downscale)};
    std::string filePath {parser.get("image")};

    // Throw err if wrong args passed
    if (downscale <= 0) throw std::runtime_error("Downscale factor cannot be negative.");
    if (poolFunc != "avg" && poolFunc != "max") 
        throw std::runtime_error("Unsupported downscaling function: " + poolFunc);

    // Read the image
    png::Image image {png::read(filePath.c_str())};

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
            std::array<std::uint8_t, 4> pixel {poolFunc == "avg"? poolAvg(pixels): poolMax(pixels)};
            double grey {(pixel[3] / 255.) * (.299 * pixel[0] + .587 * pixel[1] + .114 * pixel[2])};
            grayscaled[i][j] = grey; min = std::min(min, grey); max = std::max(max, grey);
        }
    }

    // Map each pixel to char post normalization
    std::string result{};
    result.reserve(grayscaled.size() * (grayscaled[0].size() + 1));
    for (const std::vector<double> &row: grayscaled) {
        for (const double &pixel: row)
            result.push_back(mapPixel(pixel, min, max)) ;
        result.push_back('\n');
    }

    // Print to console
    std::cout << result;
}
