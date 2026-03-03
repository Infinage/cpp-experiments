#pragma once

#include <optional>

namespace Calc
{

/**
 * @brief Represents a lexical token in a mathematical expression.
 *
 * A Token can either represent:
 * - A numeric literal (TokenType::Num)
 * - An operator (+, -, *, /)
 * - A parenthesis '(' or ')'
 *
 * If the token type is Num, the `number` field contains the parsed value.
 * For all other token types, `number` is std::nullopt.
 */
struct Token
{
  /**
   * @brief Enumerates all supported token types.
   */
  enum class TokenType
  {
    Num,   ///< Numeric literal
    Open,  ///< '('
    Close, ///< ')'
    Mul,   ///< '*'
    Div,   ///< '/'
    Add,   ///< '+'
    Sub    ///< '-'
  } type;

  /// Holds numeric value if type == Num.
  std::optional<double> number{std::nullopt};
};

} // namespace Calc
