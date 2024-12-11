#include <iostream>
#include <memory>

// BASE CLASSES ------------------------------

class Device {
    public:
        virtual void info() = 0;
        virtual ~Device()   = default;
};

class Manufacturer {
    public:
        virtual std::shared_ptr<Device> buildMobile()   = 0;
        virtual std::shared_ptr<Device> buildComputer() = 0;
        virtual ~Manufacturer()                         = default;
};

// IMPLEMENT BASE CLASS ---------------------

class WindowsMobile: public Device {
    public: void info() override { std::cout << "This is a Windows Mobile Device.\n"; }
};

class WindowsComputer: public Device {
    public: void info() override { std::cout << "This is a Windows Computer.\n"; }
};

class AppleMobile: public Device {
    public: void info() override { std::cout << "This is an Apple Mobile Device (iPhone).\n"; }
};

class AppleComputer: public Device {
    public: void info() override { std::cout << "This is an Apple Computer (MAC).\n"; }
};

class Apple: public Manufacturer {
    public:
        std::shared_ptr<Device> buildMobile() override   { return std::make_shared<AppleMobile>();   }
        std::shared_ptr<Device> buildComputer() override { return std::make_shared<AppleComputer>(); }
};

class Microsoft: public Manufacturer {
    public:
        std::shared_ptr<Device> buildMobile() override   { return std::make_shared<WindowsMobile>();   }
        std::shared_ptr<Device> buildComputer() override { return std::make_shared<WindowsComputer>(); }
};

// SAMPLE PROGRAM ----------------------------

int main() {
    // Factory 1
    Microsoft msft;
    std::shared_ptr<Device> windowsPC     {msft.buildComputer()};
    std::shared_ptr<Device> windowsMobile {msft.buildMobile()};

    // Factory 2
    Apple aapl;
    std::shared_ptr<Device> macPC  {aapl.buildComputer()};
    std::shared_ptr<Device> iphone {aapl.buildMobile()};

    // Outputs
    windowsPC     -> info();
    windowsMobile -> info();
    macPC         -> info();
    iphone        -> info();

    return 0;
}
