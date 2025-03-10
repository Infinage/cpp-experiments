#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_NAME, BROWSER_BINARY}, PORT};
    driver.navigateTo("https://httpbin.org/cookies/set/test_cookie/test_value");

    // Test getNamedCookie endpoint
    bool found {driver.getCookie("test_cookie").value == "test_value"};

    // Test getAllCookies endpoint
    for (const auto &cookie: driver.getAllCookies()) {
        if (cookie.name == "test_cookie" && cookie.value == "test_value") {
            found &= true; break;
        }
    }

    return !found;
}
