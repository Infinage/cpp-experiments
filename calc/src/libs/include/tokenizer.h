#pragma once

#include "token.h"

#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace Calc
{

/**
 * @brief Converts a raw input string into a sequence of Tokens.
 *
 * Responsibilities:
 * - Recognize numeric literals (including floating point)
 * - Recognize operators: + - * /
 * - Recognize parentheses: ( )
 * - Ignore whitespace
 *
 * On error (invalid character or malformed number),
 * returns std::unexpected with an explanatory message.
 */
class Tokenizer
{
public:
  /**
   * @brief Tokenizes an entire input expression.
   *
   * @param input Expression string to tokenize.
   * @return std::expected containing a vector of Tokens on success,
   *         or an error string on failure.
   */
  std::expected<std::vector<Token>, std::string>
  tokenize(std::string_view input) const;

  /**
   * @brief Processes a single raw token fragment.
   *
   * Attempts to interpret the input as either:
   * - A valid operator or parenthesis
   * - A valid floating-point number
   *
   * @param raw Raw token text.
   * @return Parsed Token or error string.
   */
  std::expected<Token, std::string> processToken(std::string_view raw) const;

private:
  /// Mapping of valid operator/parenthesis strings to token types.
  const std::unordered_map<std::string_view, Token::TokenType> validTokens{
      {"+", Token::TokenType::Add},  {"-", Token::TokenType::Sub},
      {"*", Token::TokenType::Mul},  {"/", Token::TokenType::Div},
      {"(", Token::TokenType::Open}, {")", Token::TokenType::Close}};
};

} // namespace Calc
