#include <iostream>
#include <memory>

class Expression {
    public:
        virtual int interpret() const = 0;
        virtual ~Expression() = default;
};

class Number: public Expression {
    private:
        const int N;

    public:
        Number(const int n): N(n) {}
        int interpret() const { return N; }
};

class BinaryExpression: public Expression {
    protected:
        std::shared_ptr<Expression> n1, n2;

    public:
        virtual ~BinaryExpression() = default;
        BinaryExpression(std::shared_ptr<Expression> n1, std::shared_ptr<Expression> n2) { 
            this->n1 = n1; this->n2 = n2; 
        }
};

class AddExpression: public BinaryExpression {
    public:
        using BinaryExpression::BinaryExpression;
        int interpret() const override { return n1->interpret() + n2->interpret(); }
};

class SubExpression: public BinaryExpression {
    public:
        using BinaryExpression::BinaryExpression;
        int interpret() const override { return n1->interpret() - n2->interpret(); }
};

int main() {
    std::shared_ptr<Number> 
        four {std::make_shared<Number>(4)},
        eight {std::make_shared<Number>(8)}, 
        nine {std::make_shared<Number>(9)}; 

    std::shared_ptr<Expression> finalExpr {
        std::make_shared<AddExpression>
            ( four, std::make_shared<SubExpression>(nine, eight) ) 
    };

    std::cout << "(9 - 8) + 4 => " << finalExpr->interpret() << "\n";
    return 0;
}
