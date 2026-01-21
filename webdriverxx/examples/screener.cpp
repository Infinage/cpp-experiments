#include "webdriverxx.hpp"

namespace fs = std::filesystem;
using namespace webdriverxx;

// Specify Browser type, Location and driver port
auto browserType = BROWSERS::CHROME;
auto browserPath = "C:/Program Files/Google/Chrome/Application/chrome.exe";
auto driverPort = "1000";

int main() {
    // Remove screens if already exists, create new
    if (fs::exists("screens")) fs::remove_all("screens");
    fs::create_directory("screens");

    // Create a new webdriver
    auto caps = Capabilities(browserType, browserPath).startMaximized(true);
    Driver driver{caps, driverPort};

    // Navigate to URL
    driver.navigateTo("https://www.screener.in");

    // Click on 'screens' (clickJS work around for element not interactable)
    driver.findElement(CSS, "a[href='/explore/']").clickJS();

    // Wait until loaded
    waitUntil([&]{ return driver.getTitle() == "Explore stock screens - Screener"; });

    // Iterate through all screens
    auto screenLinks {driver.findElements(CSS, ".screen-items a")};
    for (auto screenLink: screenLinks) {
        // Extract screen name from url
        auto screenName {screenLink.getElementAttribute("href")};
        screenName = screenName.substr(screenName.rfind('/', screenName.size() - 2) + 1);
        screenName.pop_back();

        // Click the link
        screenLink.click();

        // Print the page result
        auto pageopts = PageOptions().orientation(LANDSCAPE).shrinkToFit(false).pageScale(0.75);
        driver.print("screens/" + screenName + ".pdf", pageopts);
        
        // Navigate back
        driver.back();
    }
}
