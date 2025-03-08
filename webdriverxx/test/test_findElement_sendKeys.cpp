#include "webdriverxx.hpp"

int main() {
    webdriverxx::Driver driver;
    driver.navigateTo("https://duckduckgo.com");
    webdriverxx::Element element {driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "#searchbox_input")};
    element.clear().sendKeys("Hello world").submit();
    int status {webdriverxx::waitUntil([&driver]{ return driver.getTitle() == "Hello world at DuckDuckGo"; }, 5000)};
    return !status;
}
