#include <iostream>
#include <memory>
#include <unordered_map>

// --------------------------- Define Base class --------------------------- //

class Listener {
    private:
        // Uniquely tagging a listener for deletion
        const long id;
        static std::size_t count;

    public:
        Listener(): id(static_cast<long>(count++)) {}
        virtual void update(const std::string &message) const = 0;
        virtual ~Listener() = default;
        long getID() const { return id; }
};

// Init Static varible outside
std::size_t Listener::count {0};

// ----------------------- Base Class Implementations ----------------------- //

class EmailListener: public Listener {
    private:
        const std::string email;

    public:
        EmailListener(std::string &&email): Listener(), email(email) {}
        void update(const std::string &message) const override {
            std::cout << "*** Sending email notification to " << email << ": " << message << " ***\n";
        }
};

class SMSListener: public Listener {
    private:
        const std::string mobile;

    public:
        SMSListener(std::string &&mobile): Listener(), mobile(mobile) {}
        void update(const std::string &message) const override {
            std::cout << "*** Sending SMS notification to " << mobile << ": " << message << " ***\n";
        }
};

// --------------------------- Decouple notification logic --------------------------- //

class NotificationService {
    private:
        std::unordered_map<long, std::unique_ptr<Listener>> subscribers;

    public:
        void notify(const std::string &message) const {
            for (const std::pair<const long, std::unique_ptr<Listener>> &s: subscribers)
                s.second->update(message);
        }

        void subscribe(std::unique_ptr<Listener> listener) {
            std::cout << "ID #" << listener->getID() << " is now subscribed.\n";
            subscribers[listener->getID()] = std::move(listener);
        }

        void unsubscribe(long id) {
            if (subscribers.find(id) == subscribers.end())
                std::cout << "User not subscribed.\n";
            else {
                std::cout << "ID #" << id << " is now unsubscribed.\n";
                subscribers.erase(id);
            }
        }
};

// --------------------------- STORE Implementation --------------------------- //

class Store {
    private:
        NotificationService *service; 

    public:
        NotificationService *getNotificationService() { return service; }
        void setNotificationService(NotificationService *service) { 
            this->service = service; 
        }

        void update(const std::string &message) { 
            if (service == nullptr) 
                std::cout << "No Notification service available.\n";
            else 
                service->notify(message); 
        }
};

// ------------------------------- SAMPLE PROGRAM -------------------------------- //

int main() {

    Store store;
    NotificationService notificationService;
    store.setNotificationService(&notificationService);

    notificationService.subscribe(std::make_unique<SMSListener>("897654321"));
    notificationService.subscribe(std::make_unique<EmailListener>("user1@gmail.com"));
    notificationService.subscribe(std::make_unique<EmailListener>("user2@gmail.com"));

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
