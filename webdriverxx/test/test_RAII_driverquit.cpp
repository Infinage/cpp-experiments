#include "webdriverxx.hpp"

int main() {
    // Expected to fail, see if browser still closes correctly
    int status {true};
    try {
        webdriverxx::Driver driver;
        driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "#404");
    } catch(...) { }

    return !status;
}
