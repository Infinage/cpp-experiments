#include "webdriverxx.hpp"
#include <iostream>

int main() {
    std::string response {webdriverxx::startSession()};
    std::cout << response << '\n';
    return 0;
}
