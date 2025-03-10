#include "test_config.hpp"

int main() {
    // Expected to fail, see if browser still closes correctly
    int status {true};
    try {
        webdriverxx::Driver driver{BROWSER_NAME, BROWSER_BINARY, PORT};
        driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "#404");
    } catch(...) { }

    return !status;
}
