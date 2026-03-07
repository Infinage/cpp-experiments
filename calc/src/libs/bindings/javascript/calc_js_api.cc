// $ em++ calc_js_api.cc ../../core/*.cc -I../../core/include -std=c++23 -O3 --bind -o calculator.js
#include <string>
#include <emscripten/bind.h>
#include "calc/calculator.h"

struct CalculationResult {
    double value;
    std::string error;
    bool success;
};

class JSCalculator {
public:
    JSCalculator() = default;

    CalculationResult compute(const std::string &expr) const {
        auto result = calc.compute(expr);
        if (!result) return {0.0, result.error(), false};
        return {*result, "", true};
    }

private:
    Calc::Calculator calc;
};

EMSCRIPTEN_BINDINGS(calculator_module) {
    emscripten::value_object<CalculationResult>("CalculationResult")
        .field("value", &CalculationResult::value)
        .field("error", &CalculationResult::error)
        .field("success", &CalculationResult::success);

    emscripten::class_<JSCalculator>("Calculator")
        .constructor<>()
        .function("compute", &JSCalculator::compute);
}
