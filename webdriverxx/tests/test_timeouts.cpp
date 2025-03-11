#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_TYPE, BROWSER_BINARY}, PORT};
    driver.setTimeouts({.script=0, .pageLoad=0, .implicit=0});
    const auto &timeouts {driver.getTimeouts()};
    int status {timeouts.script == 0 && timeouts.pageLoad == 0 && timeouts.implicit == 0};
    return !status;
}
