#include "calc/parser.h"
#include <stack>

std::expected<std::vector<Calc::Token>, std::string>
Calc::Parser::parse(std::span<Token> tokens) const
{
  std::vector<Token> result;
  std::stack<Token> stack;
  for (auto &token : tokens)
  {
    if (token.type == Token::TokenType::Num)
      result.push_back(token);
    else if (token.type == Token::TokenType::Close)
    {
      bool matched = false;
      while (!stack.empty() && !matched)
      {
        auto curr = stack.top();
        stack.pop();
        matched = curr.type == Token::TokenType::Open;
        if (!matched) result.push_back(curr);
      }
      if (!matched) return std::unexpected{"Imbalanced parentheses"};
    }
    else
    {
      while (!stack.empty() && stack.top().type != Token::TokenType::Open &&
             oPriority.at(stack.top().type) >= oPriority.at(token.type))
      {
        result.push_back(stack.top());
        stack.pop();
      }
      stack.push(token);
    }
  }

  while (!stack.empty())
  {
    result.push_back(stack.top());
    stack.pop();
    if (result.back().type == Token::TokenType::Open)
      return std::unexpected{"Imbalanced parentheses"};
  }

  return result;
}
