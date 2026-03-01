#include "tokenizer.h"
#include <charconv>

std::expected<Calc::Token, std::string>
Calc::Tokenizer::processToken(std::string_view raw) const {
    if (raw.empty()) 
        return std::unexpected{"Empty token"};

    auto it = validTokens.find(raw);
    if (it != validTokens.end())
        return Token{it->second};

    double value;
    auto [ptr, ec] = std::from_chars(raw.begin(), raw.end(), value);
    if (ec != std::errc{} || ptr != raw.end()) 
        return std::unexpected{"Not a valid number: '" + std::string{raw} + "'"};

    return Token{Token::TokenType::Num, value};
}

std::expected<std::vector<Calc::Token>, std::string>
Calc::Tokenizer::tokenize(std::string_view input) const {
    std::vector<Token> tokens;
    std::string acc;

    for (auto idx = 0ul; idx < input.size(); ++idx) {
        char ch = input.at(idx);
        switch (ch) {
            case '0': case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': case '.': {
                acc += ch; break;
            }

            case '(': case ')': case '+': case '-': case '*': case '/': {
                // Process pending chars in accumulator
                if (!acc.empty()) {
                    auto tok = processToken(acc);
                    if (!tok) return std::unexpected{tok.error()};
                    tokens.push_back(tok.value());
                }

                // Process current char
                acc = ch; auto tok = processToken(acc);
                if (!tok) return std::unexpected{tok.error()};
                tokens.push_back(tok.value());

                acc.clear(); break;
            }

            case '\t': case '\n': case '\r': case ' ': {
                if (!acc.empty()) {
                    auto tok = processToken(acc);
                    if (!tok) return std::unexpected{tok.error()};
                    tokens.push_back(tok.value());
                    acc.clear();
                }
                break;
            }

            default:
                return std::unexpected{"Invalid character: '" + std::string{ch} 
                    + "' @ index: " + std::to_string(idx)};
        }
    }

    if (!acc.empty()) {
        auto token = processToken(acc);
        if (!token) return std::unexpected{token.error()};
        tokens.push_back(token.value());
    }

    return tokens;
}
