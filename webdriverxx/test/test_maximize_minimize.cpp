#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{BROWSER_NAME, BROWSER_BINARY, PORT};
    driver.maximize();
    driver.minimize();
    return 0;
}
