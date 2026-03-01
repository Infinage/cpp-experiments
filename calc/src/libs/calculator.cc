#include "calculator.h"

std::expected<double, std::string> 
Calc::Calculator::compute(std::string_view expr) const {
    auto tokens = tokenizer.tokenize(expr);
    if (!tokens) return std::unexpected{tokens.error()};

    auto postfixTokens = parser.parse(*tokens);
    if (!postfixTokens) return std::unexpected{postfixTokens.error()};

    auto result = evaluator.eval(*postfixTokens);
    if (!result) return std::unexpected{result.error()};

    return result.value();
}
