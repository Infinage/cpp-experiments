#include "tokenizer.h"
#include <gtest/gtest.h>

Calc::Tokenizer tokenizer;

TEST(Tokenizer, Empty) {
    auto tok = tokenizer.processToken("");
    ASSERT_TRUE(!tok.has_value());
    ASSERT_EQ(tok.error(), "Empty token");

    auto toks = tokenizer.tokenize("");
    ASSERT_TRUE(toks.has_value());
    ASSERT_TRUE(toks.value().empty());
}

TEST(Tokenizer, OnlySpaces) {
    for (auto inp: {" ", "  ", "   "}) {
        auto tok = tokenizer.processToken(inp);
        ASSERT_TRUE(!tok.has_value());
        ASSERT_EQ(tok.error(), "Not a valid number: '" + std::string{inp} + "'");

        auto toks = tokenizer.tokenize(inp);
        ASSERT_TRUE(toks.has_value());
        ASSERT_TRUE(toks.value().empty());
    }
}

TEST(Tokenizer, NoSpaces) {
    auto toks = tokenizer.tokenize("1+2");
    ASSERT_TRUE(toks.has_value());
    ASSERT_EQ(toks.value().size(), 3);

    auto tok1 = toks.value().at(0);
    ASSERT_EQ(tok1.type, Calc::Token::TokenType::Num);
    ASSERT_EQ(tok1.number, 1.);

    auto tok2 = toks.value().at(1);
    ASSERT_EQ(tok2.type, Calc::Token::TokenType::Add);

    auto tok3 = toks.value().at(2);
    ASSERT_EQ(tok3.type, Calc::Token::TokenType::Num);
    ASSERT_EQ(tok3.number, 2.);
}

TEST(Tokenizer, MixedSpaces) {
    {
        auto toks = tokenizer.tokenize("   12 91 + 1 3  ");
        ASSERT_TRUE(toks.has_value());
        auto &vals = toks.value();

        ASSERT_EQ(vals.size(), 5);
        ASSERT_EQ(vals.at(0).type, Calc::Token::TokenType::Num);
        ASSERT_TRUE(vals.at(0).number.has_value());
        ASSERT_EQ(vals.at(0).number.value(), 12.);

        ASSERT_EQ(vals.at(1).type, Calc::Token::TokenType::Num);
        ASSERT_TRUE(vals.at(1).number.has_value());
        ASSERT_EQ(vals.at(1).number.value(), 91.);

        ASSERT_EQ(vals.at(2).type, Calc::Token::TokenType::Add);

        ASSERT_EQ(vals.at(3).type, Calc::Token::TokenType::Num);
        ASSERT_TRUE(vals.at(3).number.has_value());
        ASSERT_EQ(vals.at(3).number.value(), 1.);

        ASSERT_EQ(vals.at(4).type, Calc::Token::TokenType::Num);
        ASSERT_TRUE(vals.at(4).number.has_value());
        ASSERT_EQ(vals.at(4).number.value(), 3.);
    }

    {
        auto toks = tokenizer.tokenize(" 12\t+\n3 "); 
        ASSERT_TRUE(toks.has_value()); 
        auto &vals = toks.value(); 

        ASSERT_EQ(vals.size(), 3); 
        ASSERT_EQ(vals[0].type, Calc::Token::TokenType::Num); 
        ASSERT_EQ(vals[0].number.value(), 12.); 

        ASSERT_EQ(vals[1].type, Calc::Token::TokenType::Add); 
        ASSERT_EQ(vals[2].type, Calc::Token::TokenType::Num); 
        ASSERT_EQ(vals[2].number.value(), 3.);
    }
}

TEST(Tokenizer, InvalidTokens) {
    for (auto inp: {"123 ", "123..", "1.2.0", "123@", "!@12", "."}) {
        auto tok = tokenizer.processToken(inp);
        ASSERT_TRUE(!tok.has_value()) << "Unexpectedly ok for: " << inp;
        ASSERT_EQ(tok.error(), "Not a valid number: '" + std::string{inp} + "'");
    }

    for (auto inp: {"123@", "!@12", ".._", " @", "12 #"}) {
        auto toks = tokenizer.tokenize(inp);
        ASSERT_TRUE(!toks.has_value()) << "Unexpectedly ok for: " << inp;
        ASSERT_TRUE(toks.error().starts_with("Invalid character: '"));
    }
}

TEST(Tokenizer, Parentheses) {
    auto toks = tokenizer.tokenize("(1+2)*3");
    ASSERT_TRUE(toks.has_value());
    auto &vals = toks.value();
    ASSERT_EQ(vals.size(), 7);

    ASSERT_EQ(vals[0].type, Calc::Token::TokenType::Open);
    ASSERT_EQ(vals[1].type, Calc::Token::TokenType::Num);
    ASSERT_EQ(vals[1].number.value(), 1.);

    ASSERT_EQ(vals[2].type, Calc::Token::TokenType::Add);
    ASSERT_EQ(vals[3].type, Calc::Token::TokenType::Num);
    ASSERT_EQ(vals[3].number.value(), 2.);

    ASSERT_EQ(vals[4].type, Calc::Token::TokenType::Close);
    ASSERT_EQ(vals[5].type, Calc::Token::TokenType::Mul);
    ASSERT_EQ(vals[6].type, Calc::Token::TokenType::Num);
    ASSERT_EQ(vals[6].number.value(), 3.);
}

TEST(Tokenizer, FloatingPointNumbers) {
    auto toks = tokenizer.tokenize("3.14+0.001-2.0");
    ASSERT_TRUE(toks.has_value());
    auto &vals = toks.value();
    ASSERT_EQ(vals.size(), 5);

    ASSERT_EQ(vals[0].type, Calc::Token::TokenType::Num);
    ASSERT_DOUBLE_EQ(vals[0].number.value(), 3.14);

    ASSERT_EQ(vals[1].type, Calc::Token::TokenType::Add);

    ASSERT_EQ(vals[2].type, Calc::Token::TokenType::Num);
    ASSERT_DOUBLE_EQ(vals[2].number.value(), 0.001);

    ASSERT_EQ(vals[3].type, Calc::Token::TokenType::Sub);

    ASSERT_EQ(vals[4].type, Calc::Token::TokenType::Num);
    ASSERT_DOUBLE_EQ(vals[4].number.value(), 2.0);
}

TEST(Tokenizer, EdgeNumbers) {
    for (auto inp : {"0", "0.0", ".5", "5."}) {
        auto toks = tokenizer.tokenize(inp);
        ASSERT_TRUE(toks.has_value()) << "Input: " << inp;
        ASSERT_EQ(toks.value().size(), 1);
        ASSERT_EQ(toks.value()[0].type, Calc::Token::TokenType::Num);
    }
}

TEST(Tokenizer, EmptyParentheses) {
    auto toks = tokenizer.tokenize("()");
    ASSERT_TRUE(toks.has_value()) << "Error: " << toks.error();
    auto &vals = toks.value();
    ASSERT_EQ(vals.size(), 2);

    ASSERT_EQ(vals[0].type, Calc::Token::TokenType::Open);
    ASSERT_EQ(vals[1].type, Calc::Token::TokenType::Close);

    toks = tokenizer.tokenize("(())");
    ASSERT_TRUE(toks.has_value()) << "Error: " << toks.error();
    ASSERT_EQ(toks.value().size(), 4);
}
