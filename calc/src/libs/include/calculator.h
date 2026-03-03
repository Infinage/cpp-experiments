#pragma once

#include "evaluator.h"
#include "parser.h"
#include "tokenizer.h"

namespace Calc
{

/**
 * @brief High-level calculator interface.
 *
 * Coordinates:
 * 1. Tokenization
 * 2. Parsing (infix → postfix)
 * 3. Evaluation
 *
 * Provides a single entry point for computing expressions.
 */
class Calculator
{
public:
  /**
   * @brief Computes the result of a mathematical expression.
   *
   * @param expr Input expression string.
   * @return Computed value or error string.
   */
  std::expected<double, std::string> compute(std::string_view expr) const;

private:
  Tokenizer tokenizer;
  Parser parser;
  Evaluator evaluator;
};

} // namespace Calc
