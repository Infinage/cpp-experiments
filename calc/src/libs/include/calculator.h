#pragma once

#include "evaluator.h"
#include "parser.h"
#include "tokenizer.h"

namespace Calc
{
class Calculator
{
public:
  std::expected<double, std::string> compute(std::string_view expr) const;

private:
  Tokenizer tokenizer;
  Parser parser;
  Evaluator evaluator;
};
} // namespace Calc
