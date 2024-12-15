#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

class PaymentStrategy {
    public:
        virtual ~PaymentStrategy() = default;
        virtual bool validateDetails() const = 0;
        virtual void pay(double total) const = 0;
};

class CardPaymentStrategy: public PaymentStrategy {
    private:
        const unsigned long long cardNo;
        const unsigned short expiryYear;

    public:
        CardPaymentStrategy(unsigned long long cardNo, unsigned short expiry): 
            cardNo(cardNo), expiryYear(expiry) {}

        bool validateDetails() const override {
            const std::chrono::time_point now{std::chrono::system_clock::now()};
            const std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(now)};
            int currYear {static_cast<int>(ymd.year())};
            return std::ceil(std::log10(cardNo)) == 16 && expiryYear >= currYear;
        }

        void pay(double total) const override {
            if (validateDetails())
                std::cout << "Payment of amount Rs." << total << " processed via Card# " 
                          << std::to_string(cardNo).substr(0, 5) + std::string(11, '*') << ".\n";
            else
                std::cout << "Invalid card details entered.\n";
        }
};

class UPIPaymentStrategy: public PaymentStrategy {
    private:
        std::string UPINo;

    public:
        UPIPaymentStrategy(std::string && UPINo): UPINo(UPINo) {}

        bool validateDetails() const override {
            return std::count(UPINo.cbegin(), UPINo.cend(), '@') == 1; 
        }

        void pay(double total) const override {
            if (validateDetails())
                std::cout << "Payment of amount Rs." << total << " processed via UPI# " 
                          << UPINo.substr(0, 5) + std::string(UPINo.size() - 5, '*') << ".\n";
            else
                std::cout << "UPI details is not valid.\n";
        }
};

class PaymentService {
    private:
        double price;
        PaymentStrategy *strategy;

    public:
        PaymentService(double price): price(price), strategy(nullptr) {}
        void setPaymentMethod(PaymentStrategy *strategy) { this->strategy = strategy; }
        void processOrder() { 
            if (strategy == nullptr)
                std::cout << "No payment method selected.\n";
            else
                strategy->pay(price); 
        }
};

int main() {
    PaymentService service(200.);
    service.processOrder();

    PaymentStrategy *upi {new UPIPaymentStrategy("abc@okicici")};
    service.setPaymentMethod(upi);
    service.processOrder();
    delete upi;

    PaymentStrategy *card {new CardPaymentStrategy(1234567898765432, 2024)};
    service.setPaymentMethod(card);
    service.processOrder();
    delete card;

    return 0;
}
