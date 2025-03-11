#include "webdriverxx.hpp"

#include <filesystem>

namespace fs = std::filesystem;
using namespace webdriverxx;

int main() {

    // Create a folder 'screens' if doesnt already exist
    if (!fs::exists("screens")) fs::create_directory("screens");

    // Mandatory fields
    auto browserType = CHROME;
    auto browserPath = "C:/Program Files/Google/Chrome/Application/chrome.exe";
    auto driverPort = "1000";

    // Create a new webdriver
    Capabilities caps{browserType, browserPath};
    caps.startMaximized(true);
    Driver driver{caps, driverPort};

    // Navigate to URL
    driver.navigateTo("https://www.screener.in/explore");

    // Click on 'screens'
    //driver.findElement(CSS, "a[href='/explore/']").click();

    // Wait until loaded
    waitUntil([&driver]() { return driver.getTitle() == "Explore stock screens - Screener"; });

    // Iterate through all screens
    auto screenLinks {driver.findElements(CSS, ".screen-items a")};
    for (auto screenLink: screenLinks) {
        auto screenName {screenLink.getElementText()};
        screenLink.click();

        // Print the page result
        driver.print("screens/" + screenName + ".pdf", PageOptions().orientation(POTRAIT));
        
        // Navigate back
        driver.back();
    }
}
