#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_TYPE, BROWSER_BINARY}, PORT};
    driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "#404");
}
