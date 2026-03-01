#include "evaluator.h"
#include <stack>

std::expected<double, std::string>
Calc::Evaluator::eval(std::span<Token> postfix) const {
    using TT = Token::TokenType;

    std::stack<double> stack;
    for (auto &token: postfix) {
        if (token.type == TT::Num)
            stack.push(token.number.value());
        else if (stack.size() < 2)
            return std::unexpected{"Stack underflow, invalid input sequence"};
        else {
            auto v2 = stack.top(); stack.pop();
            auto v1 = stack.top(); stack.pop();
            switch (token.type) {
                case TT::Add: stack.push(v1 + v2); break;
                case TT::Sub: stack.push(v1 - v2); break;
                case TT::Mul: stack.push(v1 * v2); break;
                case TT::Div:
                    if (v2 == 0) return std::unexpected{"Divide by zero"};
                    stack.push(v1 / v2);
                    break;
                default:
                    return std::unexpected{"Encountered invalid operator"};
            }
        }
    }

    if (stack.size() != 1) 
        return std::unexpected{"Invalid postfix sequence"};

    return stack.top();
}
