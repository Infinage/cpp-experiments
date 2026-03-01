#pragma once

#include "token.h"

#include <expected>
#include <unordered_map>
#include <vector>
#include <string>

namespace Calc {
    /*
     * Given input string, return back a vector of parsed tokens
     * Valid tokens: numbers, operators [+,-,*,/], paranthesis, whitespace
     */
    class Tokenizer {
        public:
            std::expected<std::vector<Token>, std::string> tokenize(std::string_view input) const;
            std::expected<Token, std::string> processToken(std::string_view raw) const;

        private:
            const std::unordered_map<std::string_view, Token::TokenType> 
            validTokens {
                {"+",   Token::TokenType::Add}, 
                {"-",   Token::TokenType::Sub}, 
                {"*",   Token::TokenType::Mul}, 
                {"/",   Token::TokenType::Div}, 
                {"(",  Token::TokenType::Open}, 
                {")", Token::TokenType::Close}
            };
    };
}
