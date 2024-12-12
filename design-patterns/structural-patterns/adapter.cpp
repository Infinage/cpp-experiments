#include <algorithm>
#include <iostream>

class Square {
    private:
        const double sideLength;

    public:
        // Use of explicit prevents implict type conversions
        // Square sq = 5;
        explicit Square(double length = 0): sideLength(length) {}
        virtual double getSideLength() const { return sideLength; }
        virtual ~Square() = default;
};

class Circle {
    private:
        const double radius;

    public:
        explicit Circle(double radius): radius(radius) {}
        double getRadius() const { return radius; }
};

class Rectangle {
    private:
        const double side1, side2;

    public:
        explicit Rectangle(double s1, double s2): side1(s1), side2(s2) {}
        double getMaxSideLength() const { return std::max(side1, side2); }
};

class SquareHole {
    private:
        const double sideLength;
        
    public:
        explicit SquareHole(double length): sideLength(length) {}
        bool canFit(const Square &sq) const { return sq.getSideLength() <= sideLength; }
};

// ---------------- ADAPTER CLASSES ---------------- //

class CircleToSquareAdapter: public Square {
    private:
        Circle circle;

    public:
        // Explicit disallows `CircleToSquareAdapter adapter = Circle{10};`
        explicit CircleToSquareAdapter(const Circle &circle): circle(circle) {}
        double getSideLength() const override { return 2 * circle.getRadius(); }
};

class RectangleToSquareAdapter: public Square {
    private:
        Rectangle rectangle;

    public:
        // Explicit disallows `RectangleToSquareAdapter adapter = Rectangle{10};`
        explicit RectangleToSquareAdapter(const Rectangle &rectangle): rectangle(rectangle) {}
        double getSideLength() const override { return rectangle.getMaxSideLength(); }
};

// ---------------- SAMPLE PROGRAM ---------------- //

int main() {

    SquareHole sqh {10};
    Square sq {6};
    std::cout << "Can the square fit inside the square hole    : " 
              << (sqh.canFit(sq)? "True": "False") << "\n";

    Circle ci {6};
    CircleToSquareAdapter ciAdapter {ci};
    std::cout << "Can the circle fit inside the square hole    : " 
              << (sqh.canFit(ciAdapter)? "True": "False") << "\n";

    Rectangle rec {5, 5};
    RectangleToSquareAdapter recAdapter {rec};
    std::cout << "Can the rectangle fit inside the square hole : " 
              << (sqh.canFit(recAdapter)? "True": "False") << "\n";

    return 0;
}
