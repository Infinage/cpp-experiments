#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{BROWSER_NAME, BROWSER_BINARY, PORT};
    driver.navigateTo("https://github.com/Infinage");
    driver.save_screenshot("screenshot-full.png");
    driver.setImplicitTimeoutMS(30 * 1000);
    webdriverxx::Element element {driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "img.avatar")};
    element.save_screenshot("screenshot.png");
    return 0;
}
