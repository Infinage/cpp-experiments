#include "test_config.hpp"
#include <exception>

int main() {
    // Expected to fail, see if browser still closes correctly
    int status {true};
    try {
        webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_TYPE, BROWSER_BINARY}, PORT};
        driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "#404");
    } catch(std::exception &ex) { 
        status &= std::strcmp(ex.what(), "Webdriver not in ready state") != 0;
    }

    return !status;
}
