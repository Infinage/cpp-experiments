#include "calculator.h"
#include <iostream>

int main(int argc, char **argv) {
    if (argc > 2) {
        std::cerr << "Usage: calc '<expression>'\n";
        return 0;
    }

    Calc::Calculator calculator;
    std::string expr;
    while (true) {
        if (argc == 2) expr = argv[1];
        else {
            std::cout << ">> ";
            std::getline(std::cin, expr);
            if (expr.empty()) break;
        }

        auto result = calculator.compute(expr);
        if (!result) std::cerr << result.error() << '\n';
        else std::cout << *result << '\n';

        if (argc == 2) break;
    }
}
