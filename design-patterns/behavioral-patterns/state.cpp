#include <iostream>
#include <string>

class Document;

class State {
    protected:
        std::string name;
        Document *context;
            
    public:
        virtual ~State() = default;
        virtual void publish() = 0;

        State(std::string name): name(name) {}
        std::string getName() { return name; }

        void setContext(Document *context) { 
            this->context = context; 
        }
};

class Draft: public State {
    public:
        Draft(): State("Draft") {}
        void publish();
};

class Review: public State {
    public:
        Review(): State("Review") {}
        void publish();
};

class Published: public State {
    public:
        Published(): State("Published") {}
        void publish();
};

class Document {
    private:
        State *state;
        std::string text;

    public:
        Document(std::string &&text): state(new Draft()), text(text) {
            state->setContext(this); 
        }

        void render() { 
            std::cout << "(" << state->getName() << "): " << text << "\n\n"; 
        }

        void publish() { 
            std::cout << "Publishing document...\n";
            state->publish(); 
        }

        void transition(State *state) {
            if (this->state != nullptr)
                delete this->state;
            this->state = state;
            this->state->setContext(this);
        }
};

void Draft::publish() {
    std::cout << "Document state transitioned to 'Review'\n\n";
    this->context->transition(new Review());
}

void Review::publish() {
    std::cout << "Document state transitioned to 'Published'\n\n";
    this->context->transition(new Published());
}

void Published::publish() {
    std::cout << "Already published.\n";
}

int main() {
    Document doc{"Some sample text"};
    for (int i{0}; i < 3; i++) {
        doc.render();
        doc.publish();
    }

    return 0;
}
