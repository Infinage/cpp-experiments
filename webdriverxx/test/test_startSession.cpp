#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{BROWSER_NAME, BROWSER_BINARY, PORT};
    driver.quit();
    return 0;
}
