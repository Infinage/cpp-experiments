#pragma once

#include "token.h"

#include <expected>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Calc
{

/**
 * @brief Converts an infix token sequence into postfix (RPN) form.
 *
 * Implements a variation of Dijkstra's Shunting Yard algorithm.
 *
 * Handles:
 * - Operator precedence
 * - Left associativity
 * - Parentheses grouping
 *
 * On success, returns a postfix-ordered vector of Tokens.
 * On error (e.g., imbalanced parentheses), returns an error string.
 */
class Parser
{
public:
  /**
   * @brief Parses infix tokens into postfix (Reverse Polish Notation).
   *
   * @param tokens Infix token sequence.
   * @return Postfix token sequence or error string.
   */
  std::expected<std::vector<Token>, std::string>
  parse(std::span<Token> tokens) const;

private:
  /// Operator precedence table (higher value = higher precedence).
  const std::unordered_map<Token::TokenType, short> oPriority{
      {Token::TokenType::Add, 2},   {Token::TokenType::Sub, 2},
      {Token::TokenType::Mul, 3},   {Token::TokenType::Div, 3},
      {Token::TokenType::Open, 99}, {Token::TokenType::Close, 99},
  };
};

} // namespace Calc
