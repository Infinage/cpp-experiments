#pragma once

#include "token.h"

#include <expected>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Calc
{
class Parser
{
public:
  std::expected<std::vector<Token>, std::string>
  parse(std::span<Token> tokens) const;

private:
  const std::unordered_map<Token::TokenType, short> oPriority{
      {Token::TokenType::Add, 2},   {Token::TokenType::Sub, 2},
      {Token::TokenType::Mul, 3},   {Token::TokenType::Div, 3},
      {Token::TokenType::Open, 99}, {Token::TokenType::Close, 99},
  };
};
} // namespace Calc
