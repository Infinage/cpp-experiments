#pragma once

#include <optional>

namespace Calc
{
struct Token
{
  enum class TokenType
  {
    Num,
    Open,
    Close,
    Mul,
    Div,
    Add,
    Sub
  } type;
  std::optional<double> number{std::nullopt};
};
} // namespace Calc
