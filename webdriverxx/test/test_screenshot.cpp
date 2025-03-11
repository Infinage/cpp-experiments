#include "test_config.hpp"

#include <filesystem>

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_TYPE, BROWSER_BINARY}, PORT};
    driver.navigateTo("https://github.com/Infinage");
    driver.save_screenshot("screenshot-full.png");
    driver.setTimeouts({.implicit=30 * 1000});
    webdriverxx::Element element {driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "img.avatar")};
    element.save_screenshot("screenshot.png");

    int status {std::filesystem::exists("screenshot.png") && std::filesystem::exists("screenshot-full.png")};
    return !status;
}
