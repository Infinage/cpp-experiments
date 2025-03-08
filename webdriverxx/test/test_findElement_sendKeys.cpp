#include "webdriverxx.hpp"

int main() {
    webdriverxx::Driver driver;
    driver.navigateTo("https://google.com");
    webdriverxx::Element element {driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "textarea[title='Search']")};
    element.clear().sendKeys("Hello world").submit();
    int status {driver.getTitle() == "Hello world - Google Search"};
    return !status;
}
