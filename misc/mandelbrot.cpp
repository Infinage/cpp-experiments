/*
 * TODO:
 * 1. SFML + zoom to render appropriate drawings
 * 2. Colorize outputs
 * 4. Save output to disk
 * 3. Implement `bigfloat` to support infinite zooms?
 */

#include <complex>
#include <iostream>
#include <sstream>
#include "../cli/argparse.hpp"

using namespace std::complex_literals;

bool check(const std::complex<double> &C, int maxIterations = 100, double boundary = 2.0) {
    std::complex<double> curr{C}; int iterations {0};
    while (std::abs(curr) < boundary && ++iterations < maxIterations)
        curr = curr * curr + C;
    return std::abs(curr) < boundary;
}

int main(int argc, char **argv) {

    // Specify command line args
    argparse::ArgumentParser parser{"mandelbrot"};
    parser.description("Draws mandelbrot set on to the console.");
    parser.addArgument("height", argparse::ARGTYPE::POSITIONAL).defaultValue(50).help("Height of mandelbrot.");
    parser.addArgument("width", argparse::ARGTYPE::POSITIONAL).defaultValue(100).help("Width of mandelbrot.");
    parser.addArgument("n_iters", argparse::ARGTYPE::NAMED).alias("n").defaultValue(100)
        .help("Iterations to run for checking divergence.");
    parser.addArgument("xmin").help("Minimum Real value - X Axis").defaultValue(-2.);
    parser.addArgument("xmax").help("Maximum Real value - X Axis").defaultValue( 1.);
    parser.addArgument("ymin").help("Minimum Imag value - Y Axis").defaultValue(-1.);
    parser.addArgument("ymax").help("Maximum Imag value - Y Axis").defaultValue( 1.);
    parser.parseArgs(argc, argv);


    // Get the values from CLI
    const int HEIGHT = parser.get<int>("height"), 
              WIDTH = parser.get<int>("width"), 
              MAX_ITER = parser.get<int>("n_iters");

    const double MIN_RE {parser.get<double>("xmin")}, MAX_RE {parser.get<double>("xmax")}, 
                 MIN_IM {parser.get<double>("ymin")}, MAX_IM {parser.get<double>("ymax")};

    // Determine steps to make at each iteration when selecting the `C`
    const double STEP_IM {(MAX_IM - MIN_IM) / HEIGHT}, 
                           STEP_RE {(MAX_RE - MIN_RE) / WIDTH};

    // Accumulate the result
    std::ostringstream oss;
    for (double row {MIN_IM}; row <= MAX_IM; row += STEP_IM) {
        for (double col {MIN_RE}; col <= MAX_RE; col += STEP_RE)
            oss << (check({col, row}, MAX_ITER)? '@': ' ');                
        oss << '\n';
    }

    // Display on console
    std::cout << oss.str();

    return 0;
}
