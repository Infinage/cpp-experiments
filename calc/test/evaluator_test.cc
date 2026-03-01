#include "evaluator.h"
#include <gtest/gtest.h>

Calc::Evaluator evaluator;
using TT = Calc::Token::TokenType;

TEST(Evaluator, SimpleAdd) {
    std::vector<Calc::Token> postfix{{TT::Num, 1.}, {TT::Num, 2.}, {TT::Add}};
    auto res = evaluator.eval(postfix);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value(), 3.0);
}

TEST(Evaluator, SubtractionOrder) {
    std::vector<Calc::Token> postfix{{TT::Num, 3.0}, {TT::Num, 5.0}, {TT::Sub}};
    auto res = evaluator.eval(postfix);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value(), -2.0);
}

TEST(Evaluator, DivisionOrder) {
    std::vector<Calc::Token> postfix{{TT::Num, 8.0}, {TT::Num, 4.0}, {TT::Div}};
    auto res = evaluator.eval(postfix);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value(), 2.0);
}

TEST(Evaluator, ComplexExpression) {
    std::vector<Calc::Token> postfix{
        {TT::Num, 3},
        {TT::Num, 4},
        {TT::Num, 2},
        {TT::Mul},
        {TT::Num, 1},
        {TT::Num, 5},
        {TT::Sub},
        {TT::Div},
        {TT::Add}
    };

    auto res = evaluator.eval(postfix);
    ASSERT_TRUE(res.has_value());
    ASSERT_DOUBLE_EQ(res.value(), 1.0);
}

TEST(Evaluator, DivideByZero) {
    std::vector<Calc::Token> postfix{{TT::Num, 5}, {TT::Num, 0}, {TT::Div}};
    auto res = evaluator.eval(postfix);
    ASSERT_FALSE(res.has_value());
    ASSERT_EQ(res.error(), "Divide by zero");
}

TEST(Evaluator, StackUnderflow) {
    std::vector<Calc::Token> postfix{{TT::Num, 1}, {TT::Add}};
    auto res = evaluator.eval(postfix);
    ASSERT_FALSE(res.has_value());
    ASSERT_EQ(res.error(), "Stack underflow, invalid input sequence");
}

TEST(Evaluator, InvalidFinalStack) {
    std::vector<Calc::Token> postfix{{TT::Num, 1}, {TT::Num, 2}};
    auto res = evaluator.eval(postfix);
    ASSERT_FALSE(res.has_value());
    ASSERT_EQ(res.error(), "Invalid input sequence");
}
