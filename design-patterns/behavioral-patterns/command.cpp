#include <cstdlib>
#include <iostream>
#include <stack>

class Command {
    protected:
        double value;

    public:
        Command(double val): value(val) {}
        virtual ~Command() = default;
        virtual double execute(double state) const = 0;
        virtual double    undo(double state) const = 0;
};

class AddCommand: public Command {
    public:
        // Inheriting all of the parent constructors
        using Command::Command;

        double execute(double state) const override { return state + value; }
        double    undo(double state) const override { return state - value; }
};

class SubCommand: public Command {
    public:
        using Command::Command;
        double execute(double state) const override { return state - value; }
        double    undo(double state) const override { return state + value; }
};

class MulCommand: public Command {
    public:
        using Command::Command;
        double execute(double state) const override { return state * value; }
        double    undo(double state) const override { 
            if (value == 0) { std::cout << "Div by Zero.\n"; std::exit(1); }
            return state / value; 
        }
};

class DivCommand: public Command {
    public:
        using Command::Command;
        double    undo(double state) const override { return state * value; }
        double execute(double state) const override {
            if (value == 0) { std::cout << "Div by Zero.\n"; std::exit(1); }
            return state / value; 
        }
};

class Calculator {
    private:
        double state;
        std::stack<Command*> history;

    public:
        Calculator(): state(0) {}
        ~Calculator() {
            while (!history.empty()) {
                Command *c {history.top()};
                history.pop();
                delete c;
            }
        }

        void execute(Command *c) {
            state = c->execute(state);
            history.push(c);
            std::cout << state << "\n";
        }

        void undo() {
            if (history.empty()) std::cout << "Nothing to undo.\n";
            else {
                Command *c {history.top()};
                history.pop();
                state = c->undo(state);
                delete c;
                std::cout << state << "\n";
            }
        }
};

int main() {
    Calculator calc;
    calc.undo();
    calc.execute(new AddCommand(1));
    calc.execute(new AddCommand(1));
    calc.execute(new MulCommand(5));
    calc.execute(new DivCommand(2));
    calc.undo();
    return 0;
}
