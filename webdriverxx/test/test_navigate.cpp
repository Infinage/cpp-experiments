#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_NAME, BROWSER_BINARY}, PORT};
    int status {driver.navigateTo("https://google.com").getTitle() == "Google"};
    return !status;
}
