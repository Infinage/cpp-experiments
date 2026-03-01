#include "parser.h"
#include <gtest/gtest.h>

Calc::Parser parser;
using TT = Calc::Token::TokenType;

TEST(Parser, SingleNumber) {
    std::vector<Calc::Token> infix {{TT::Num, 3.0}};
    auto res = parser.parse(infix);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->size(), 1);
    ASSERT_EQ(res->at(0).number.value(), 3.0);
}

TEST(Parser, SimpleAdd) {
    std::vector<Calc::Token> infix {{TT::Num, 1.0}, {TT::Add}, {TT::Num, 2.0}};

    auto res = parser.parse(infix);
    ASSERT_TRUE(res.has_value());

    const auto& out = res.value();
    ASSERT_EQ(out.size(), 3);

    ASSERT_EQ(out[0].number.value(), 1.0);
    ASSERT_EQ(out[1].number.value(), 2.0);
    ASSERT_EQ(out[2].type, TT::Add);
}

TEST(Parser, Precedence) {
    std::vector<Calc::Token> infix{
        {TT::Num, 1}, {TT::Add}, {TT::Num, 2}, 
        {TT::Mul}, {TT::Num, 3}};

    auto res = parser.parse(infix);
    ASSERT_TRUE(res.has_value());

    auto& out = res.value();
    ASSERT_EQ(out.size(), 5);

    ASSERT_EQ(out[0].type, TT::Num);
    ASSERT_EQ(out[0].number.value(), 1.);

    ASSERT_EQ(out[1].type, TT::Num);
    ASSERT_EQ(out[1].number.value(), 2.);

    ASSERT_EQ(out[2].type, TT::Num);
    ASSERT_EQ(out[2].number.value(), 3.);

    ASSERT_EQ(out[3].type, TT::Mul);
    ASSERT_EQ(out[4].type, TT::Add);
}

TEST(Parser, Parentheses) {
    std::vector<Calc::Token> infix{
        {TT::Open},
        {TT::Num, 1}, {TT::Add}, {TT::Num, 2},
        {TT::Close},
        {TT::Mul},
        {TT::Num, 3}
    };

    auto res = parser.parse(infix);
    ASSERT_TRUE(res.has_value()) << "Error: " << res.error();

    auto& out = res.value();
    ASSERT_EQ(out.size(), 5);

    ASSERT_EQ(out[0].type, TT::Num);
    ASSERT_EQ(out[0].number.value(), 1.);

    ASSERT_EQ(out[1].type, TT::Num);
    ASSERT_EQ(out[1].number.value(), 2.);

    ASSERT_EQ(out[2].type, TT::Add);

    ASSERT_EQ(out[3].type, TT::Num);
    ASSERT_EQ(out[3].number.value(), 3.);

    ASSERT_EQ(out[4].type, TT::Mul);
}

TEST(Parser, ImbalancedParentheses) {
    std::vector<Calc::Token> infix {{TT::Open}, {TT::Num,1}, {TT::Add}, {TT::Num,2}};
    auto res = parser.parse(infix);
    ASSERT_FALSE(res.has_value());
    ASSERT_EQ(res.error(), "Imbalanced paranthesis");
}

TEST(Parser, ComplexExpression) {
    using T = Calc::Token;
    using TT = Calc::Token::TokenType;

    //  3 + 4 * 2 / ( 1 - 5 )
    std::vector<T> input{
        {TT::Num, 3.0},
        {TT::Add},
        {TT::Num, 4.0},
        {TT::Mul},
        {TT::Num, 2.0},
        {TT::Div},
        {TT::Open},
        {TT::Num, 1.0},
        {TT::Sub},
        {TT::Num, 5.0},
        {TT::Close}
    };

    auto res = parser.parse(input);
    ASSERT_TRUE(res.has_value());

    auto& out = res.value();
    ASSERT_EQ(out.size(), 9);

    // Expected: 3 4 2 * 1 5 - / +
    ASSERT_EQ(out[0].number.value(), 3.0);
    ASSERT_EQ(out[1].number.value(), 4.0);
    ASSERT_EQ(out[2].number.value(), 2.0);
    ASSERT_EQ(out[3].type, TT::Mul);
    ASSERT_EQ(out[4].number.value(), 1.0);
    ASSERT_EQ(out[5].number.value(), 5.0);
    ASSERT_EQ(out[6].type, TT::Sub);
    ASSERT_EQ(out[7].type, TT::Div);
    ASSERT_EQ(out[8].type, TT::Add);
}
