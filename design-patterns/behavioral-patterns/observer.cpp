#include <iostream>
#include <unordered_map>

class Listener {
    private:
        const long id;
        static std::size_t count;

    public:
        Listener(): id(static_cast<long>(count++)) {}
        virtual void update(std::string &message) const = 0;
        virtual ~Listener() = default;
        long getID() const { return id; }
};

class EmailListener: public Listener {
    private:
        const std::string email;

    public:
        EmailListener(std::string &&email): Listener(), email(email) {}
        void update(std::string &message) const override {
            std::cout << "*** Sending email notification to " << email << ": " << message << " ***\n";
        }
};

class SMSListener: public Listener {
    private:
        const std::string mobile;

    public:
        SMSListener(std::string &&mobile): Listener(), mobile(mobile) {}
        void update(std::string &message) const override {
            std::cout << "*** Sending SMS notification to " << mobile << ": " << message << " ***\n";
        }
};

class NotificationService {
    private:
        std::unordered_map<long, Listener*> subscribers;

    public:
        ~NotificationService() {
            for (std::pair<long, Listener*> s: subscribers)
                delete s.second;
        }

        void notify(std::string &message) const {
            for (std::pair<long, Listener*> s: subscribers)
                s.second->update(message);
        }

        void subscribe(Listener *listener) {
            std::cout << "ID #" << listener->getID() << " is now subscribed.\n";
            subscribers[listener->getID()] = listener;
        }

        void unsubscribe(long id) {
            if (subscribers.find(id) == subscribers.end())
                std::cout << "Already not subscribed.\n";
            else {
                std::cout << "ID #" << id << " is now unsubscribed.\n";
                delete subscribers[id];
                subscribers.erase(id);
            }
        }
};

class Store {
    private:
        NotificationService *service; 

    public:
        NotificationService *getNotificationService() { return service; }
        void setNotificationService(NotificationService *service) { 
            this->service = service; 
        }

        void update(std::string &&message) { service->notify(message); }
};

// Init Static varible outside
std::size_t Listener::count {0};

int main() {

    Store store;
    NotificationService notificationService;
    store.setNotificationService(&notificationService);

    notificationService.subscribe(new SMSListener("897654321"));
    notificationService.subscribe(new EmailListener("user1@gmail.com"));
    notificationService.subscribe(new EmailListener("user2@gmail.com"));

    std::cout << "\n";
    store.update("Product XYZ is now available at a discount of 10%!");
    std::cout << "\n";
    store.update("Product ABC is now available in stock!");
    std::cout << "\n";

    notificationService.unsubscribe(1);

    std::cout << "\n";
    store.update("Product ABC is now available at a steep discount of 30%!");

    return 0;
}
