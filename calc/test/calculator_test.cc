#include "calculator.h"
#include <gtest/gtest.h>

Calc::Calculator calculator;

TEST(Calculator, SimpleAdd) {
    auto res = calculator.compute("1+2");
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value(), 3.0);
}

TEST(Calculator, OperatorPrecedence) {
    auto res = calculator.compute("1+2*3");
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value(), 7.0);
}

TEST(Calculator, Parentheses) {
    auto res = calculator.compute("(1+2)*3");
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value(), 9.0);
}

TEST(Calculator, ComplexExpression) {
    auto res = calculator.compute("3+4*2/(1-5)");
    ASSERT_TRUE(res.has_value());
    ASSERT_DOUBLE_EQ(res.value(), 1.0);
}

TEST(Calculator, FloatingPoint) {
    auto res = calculator.compute("3.14 + 0.86");
    ASSERT_TRUE(res.has_value());
    ASSERT_DOUBLE_EQ(res.value(), 4.0);
}

TEST(Calculator, Whitespace) {
    auto res = calculator.compute("  12 \t + \n 3 ");
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value(), 15.0);
}

TEST(Calculator, DivideByZero) {
    auto res = calculator.compute("5/0");
    ASSERT_FALSE(res.has_value());
    ASSERT_EQ(res.error(), "Divide by zero");
}

TEST(Calculator, InvalidCharacter) {
    auto res = calculator.compute("1+@");
    ASSERT_FALSE(res.has_value());
    ASSERT_TRUE(res.error().starts_with("Invalid character: '"));
}

TEST(Calculator, ImbalancedParentheses) {
    auto res = calculator.compute("(1+2");
    ASSERT_FALSE(res.has_value());
    ASSERT_EQ(res.error(),  "Imbalanced paranthesis");
}
