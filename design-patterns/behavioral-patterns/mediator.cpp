#include <iostream>
#include <string>
#include <unordered_map>

class Chatroom; 

class Participant {
    private:
        const std::string name;
        Chatroom *room;

    public:
        Participant(std::string &&name): name(name) {}
        std::string getName() const { return name; }

        void registerChatroom(Chatroom *room) { this->room = room; }

        void send(std::string &&message, Participant *to) const; 
        void send(std::string &&message) const;

        void receive(std::string &message, const Participant *from) const {
            std::cout << from->name << " -> " << name << ": " << message << "\n";
        }
};

class Chatroom {
    private:
        std::unordered_map<std::string, Participant*> users;
    
    public:
        void registerPartipant(Participant *user) { 
            users[user->getName()] = user;
            user->registerChatroom(this);
        }

        void send(std::string &message, const Participant *from, const Participant *to) const {
            std::unordered_map<std::string, Participant*>::const_iterator target = users.find(to->getName());
            if (target == users.cend())
                std::cout << "User doesn't exist in this chatroom.\n";
            else
                target->second->receive(message, from);
        }

        void send(std::string &message, const Participant *from) const {
            for (std::pair<const std::string, Participant*> p: users)
                p.second->receive(message, from); 
        }
};

void Participant::send(std::string &&message, Participant *to) const { 
    room->send(message, this, to); 
}

void Participant::send(std::string &&message) const { 
    room->send(message, this); 
}

int main() {
    Participant ironMan {"Tony Stark"}, blackWidow {"Natasha Romanoff"}, hulk {"Bruce Banner"};
    Chatroom sheildComms;

    sheildComms.registerPartipant(&ironMan);
    sheildComms.registerPartipant(&blackWidow);
    sheildComms.registerPartipant(&hulk);
    
    ironMan.send("Guys, I am bringing the party to you.");
    blackWidow.send("I don't see how that's a party.", &ironMan);
    hulk.send("Hulk smash?", &ironMan);
    
    return 0;
}
