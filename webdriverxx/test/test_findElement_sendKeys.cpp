#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{BROWSER_NAME, BROWSER_BINARY, PORT};
    driver.navigateTo("https://duckduckgo.com");
    webdriverxx::Element element {driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "#searchbox_input")};
    element.clear().sendKeys("Hello world").submit();
    int status {webdriverxx::waitUntil([&driver]{ return driver.getTitle() == "Hello world at DuckDuckGo"; }, 5000)};
    return !status;
}
