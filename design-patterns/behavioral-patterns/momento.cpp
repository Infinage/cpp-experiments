#include <ctime>
#include <iostream>
#include <random>
#include <string>
#include <vector>

class Momento {
    private:
        std::string _state, _date;

    public:
        Momento(std::string &state): _state(state) {
            std::time_t now = std::time(0);
            _date = std::ctime(&now);
        }

        std::string date() { return _date; }
        std::string state() { return _state; }
        std::string getName() { return _date + " / (" + _state.substr(0, 9) + "...)"; }
};

class Editor {
    private:
        std::string text;

        std::mt19937 gen{ std::random_device{}() };
        std::uniform_int_distribution<std::size_t> dist{0, 61};
        const std::string chars {
            "abcdefghijklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "0123456789"
        };

    public:
        Editor(): text("") { display(); }
        void display() { std::cout << "\nEditor: " << text << "\n"; }
        Momento *save() { return new Momento(text); }

        void restore(Momento *momento) { 
            text = momento->state(); 
            display();
        }

        void updateText(int len = 30) {
            std::string randomText{""};
            while (len--) 
                randomText += chars[dist(gen)];
            text = randomText;
            display();
        }

};

class CareTaker {
    private:
        std::vector<Momento*> history; 
        Editor *originator;

    public:
        CareTaker(Editor *editor): originator(editor) {}
        ~CareTaker() { for (Momento *m: history) delete m; }
    
        void backup() {
            std::cout << "\nSaving backup...\n";
            history.push_back(originator->save()); 
        }
        
        void undo() {
            if (history.empty()) 
                std::cout << "\nNothing to undo.\n";

            else {
                Momento *snapshot {history.back()};
                std::cout << "\nRestoring state to " << snapshot->getName() << "\n";
                history.pop_back();
                originator->restore(snapshot);
                delete snapshot;
            }
        }

        void showHistory() {
            std::cout << "\nListing all version snapshots.\n";
            int version {0};
            for (Momento *m: history)
                std::cout << "Version #" << version++ << ": " << m->getName() << "\n"; 
        }
};

int main() {
    Editor editor;
    CareTaker historyManager {&editor};
    historyManager.backup();

    for (int i {0}; i < 3; i++) {
        editor.updateText();
        historyManager.backup();
    }

    historyManager.showHistory();

    historyManager.undo();
    historyManager.undo();

    return 0;
}
