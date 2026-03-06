#pragma once

#include "token.h"

#include <expected>
#include <span>
#include <string>

namespace Calc
{

/**
 * @brief Evaluates a postfix (RPN) token sequence.
 *
 * Uses a stack-based evaluation algorithm:
 * - Push numbers onto stack
 * - On operator, pop two operands, apply operator, push result
 *
 * Errors:
 * - Stack underflow (invalid expression)
 * - Divide by zero
 * - Invalid final stack state
 */
class Evaluator
{
public:
  /**
   * @brief Evaluates a postfix expression.
   *
   * @param postfix Tokens in Reverse Polish Notation.
   * @return Computed double value or error string.
   */
  std::expected<double, std::string> eval(std::span<Token> postfix) const;
};

} // namespace Calc
