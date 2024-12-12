#include <iostream>
#include <memory>
#include <string>

// Interface that defines all
class iNotifier {
    public:
        virtual ~iNotifier() = default;
        virtual void send(const std::string &msg) const = 0;
        virtual std::string getUserName() const = 0;
};

// Implementation over which we would like to improve the functionality with decorators
class ConsoleNotifier: public iNotifier {
    private:
        const std::string username;

    public:
        ConsoleNotifier(const std::string &username): username(username) {}
        std::string getUserName() const override { return username; }
        void send(const std::string &msg) const override {
            std::cout << "Sending message: \"" << msg << "\" over Console (from " << username << ").\n";
        }
};

// ---------------------- DECORATOR LOGIC STARTS ---------------------- //

// Abstract class not meant for object creation
class BaseNotifierDecorator: public iNotifier {
    protected:
        const iNotifier &notifier;

    public:
        BaseNotifierDecorator(const iNotifier &notifier): notifier(notifier) {}
        std::string getUserName() const override { return notifier.getUserName(); }
        virtual void send(const std::string &msg) const override = 0;
};

class SMSNotifierDecorator: public BaseNotifierDecorator {
    public:
        SMSNotifierDecorator(const iNotifier &notifier): BaseNotifierDecorator(notifier) {}
        void send(const std::string &msg) const override {
            notifier.send(msg);
            std::cout << "Sending message: \"" << msg << "\" over SMS (from " << notifier.getUserName() << " ).\n";
        }
};

class EmailNotifierDecorator: public BaseNotifierDecorator {
    public:
        EmailNotifierDecorator(const iNotifier &notifier): BaseNotifierDecorator(notifier) {}
        void send(const std::string &msg) const override {
            notifier.send(msg);
            std::cout << "Sending message: \"" << msg << "\" over Email (from " << notifier.getUserName() << " ).\n";
        }
};

// ---------------------- SAMPLE PROGRAM ---------------------- //

int main() {

    std::string user1 {"User 1"}, user2 {"User 2"}, message {"Hello world!"};

    std::shared_ptr<ConsoleNotifier> cnotifier1{std::make_shared<ConsoleNotifier>(user1)};
    std::shared_ptr<EmailNotifierDecorator> enotifier1{std::make_shared<EmailNotifierDecorator>(*cnotifier1)};
    std::shared_ptr<SMSNotifierDecorator> notifier1{std::make_shared<SMSNotifierDecorator>(*enotifier1)};
    notifier1->send(message);

    std::cout << "---------------------\n";

    std::shared_ptr<ConsoleNotifier> cnotifier2{std::make_shared<ConsoleNotifier>(user2)};
    std::shared_ptr<SMSNotifierDecorator> notifier2{std::make_shared<SMSNotifierDecorator>(*cnotifier2)};
    notifier2->send(message);

    return 0;
}
