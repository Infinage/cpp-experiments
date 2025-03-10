#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_NAME, BROWSER_BINARY}, PORT};
    driver.maximize();
    driver.minimize();
    return 0;
}
