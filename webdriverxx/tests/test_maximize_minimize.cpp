#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_TYPE, BROWSER_BINARY}, PORT};
    driver.maximize();
    driver.minimize();
    return 0;
}
