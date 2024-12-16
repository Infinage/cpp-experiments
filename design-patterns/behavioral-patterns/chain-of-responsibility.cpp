#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <string>

// ------------------------- Base Handler & its implementations ------------------------- //

class Handler {
    protected:
        std::shared_ptr<Handler> next;

    public:
        Handler(std::shared_ptr<Handler> next = nullptr): next(next) {}
        virtual ~Handler() = default;
        void setNext(const std::shared_ptr<Handler> &next) { this->next = next; }
        virtual bool check(const std::string &password) const = 0;
        bool handle(bool status, std::string &&messageOnFail, const std::string &password) const {
            if (!status) {
                std::cout << messageOnFail;
                std::cout << password << " is rejected.\n\n";
                return false;
            } else {
                if (next == nullptr) std::cout << "Password " << password << " is accepted.\n\n";
                return next == nullptr? true: next->check(password);
            }
        }
};

// Ensure Password length is between [MIN_LENGTH, MAX_LENGTH]
class LengthChecker: public Handler {
    private:
        const std::size_t MIN_LENGTH, MAX_LENGTH;

    public:
        LengthChecker(std::size_t min_len = 8, std::size_t max_len = 16):
            MIN_LENGTH(min_len), MAX_LENGTH(max_len) {}

        bool check(const std::string &password) const override {
            bool status {password.size() >= MIN_LENGTH && password.size() <= MAX_LENGTH};
            return handle(status, "Password length must lie between " + std::to_string(MIN_LENGTH) + " & " + std::to_string(MAX_LENGTH) + ".\n", password);
        }
};

// Ensure Password has atleast MIN_NUMS count of Digits
class NumberChecker: public Handler {
    private:
        const int MIN_NUMS;

    public:
        NumberChecker(int min_nums = 1): MIN_NUMS(min_nums) {}

        bool check(const std::string &password) const override {
            bool status {std::count_if(password.begin(), password.end(), [](const char ch) { return std::isdigit(ch); }) >= MIN_NUMS};
            return handle(status, "Password must atleast contain " + std::to_string(MIN_NUMS) + " digit(s).\n", password); 
        }
};

// Ensure Password has atleast MIN_SPLCHARS count of Special Chars
class SpecialCharChecker: public Handler {
    private:
        const int MIN_SPLCHARS;

    public:
        SpecialCharChecker(int min_spl_chars = 1): MIN_SPLCHARS(min_spl_chars) {}

        bool check(const std::string &password) const override {
            bool status {std::count_if(password.begin(), password.end(), [](const char ch) { return !std::isalnum(ch); }) >= MIN_SPLCHARS};
            return handle(status, "Password must atleast contain " + std::to_string(MIN_SPLCHARS) + " spl char(s).\n", password); 
        }
};

// Ensure Password has atleast MIN_LCASE count of lower case & MIN_UCASE count of upper case chars
class AlphaChecker: public Handler {
    private:
        const int MIN_LCASE, MIN_UCASE;

    public:
        AlphaChecker(int min_lcase = 1, int min_ucase = 1): MIN_LCASE(min_lcase), MIN_UCASE(min_ucase) {}

        bool check(const std::string &password) const override {
            int ucase {0}, lcase {0};
            for (const char ch: password) {
                if (std::isupper(ch)) ucase++;
                else if (std::islower(ch)) lcase++;
            }

            bool status {ucase >= MIN_UCASE && lcase >= MIN_LCASE};
            return handle(status, "Password must atleast contain " + std::to_string(MIN_UCASE) + " upper & " + std::to_string(MIN_LCASE) + " lower chars.\n", password); 
        }
};

// ------------------------- Client Code ------------------------- //

int main() {

    // Create handlers
    std::shared_ptr<Handler> pwdChecker {std::make_shared<LengthChecker>()};
    std::shared_ptr<Handler> alphaChecker {std::make_shared<AlphaChecker>()};
    std::shared_ptr<Handler> numChecker {std::make_shared<NumberChecker>()};
    std::shared_ptr<Handler> splCharChecker {std::make_shared<SpecialCharChecker>()};

    // Assign chain of command
    pwdChecker->setNext(alphaChecker);
    alphaChecker->setNext(numChecker);
    numChecker->setNext(splCharChecker);

    // Sample password checks
    pwdChecker->check("adsaddsa");
    pwdChecker->check("Abaac12");
    pwdChecker->check("Abaas2@");
    pwdChecker->check("Abaasdd2@aa");
    pwdChecker->check("Abaasdd@@@@sadasd2@aa");

    return 0;
}
