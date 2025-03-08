#include "webdriverxx.hpp"

int main() {
    webdriverxx::Driver driver;
    driver.setTimeouts({0, 0, 0});
    auto [scriptTO, pageLoadTO, implicitTO] {driver.getTimeouts()};
    int status {scriptTO == 0 && pageLoadTO == 0 && implicitTO == 0};
    return !status;
}
