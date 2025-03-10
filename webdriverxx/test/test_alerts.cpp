#include "test_config.hpp"
#include "webdriverxx.hpp"

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_NAME, BROWSER_BINARY}, PORT};

    driver.navigateTo("https://www.selenium.dev/documentation/webdriver/interactions/alerts");
    int status {true};

    // Simple Alert
    driver.findElement(webdriverxx::LOCATION_STRATEGY::XPATH, "//a[normalize-space()='See an example alert']").scrollIntoView().click();
    status &= driver.getAlertText() == "Sample alert";
    driver.dismissAlert(false);

    // Accept / Dismiss Alert
    auto confirmElement {driver.findElement(webdriverxx::LOCATION_STRATEGY::XPATH, "//a[normalize-space()='See a sample confirm']").scrollIntoView()};
    driver.execute<std::nullptr_t>("arguments[0].click()", confirmElement);
    status &= driver.getAlertText() == "Are you sure?";
    driver.dismissAlert(true);

    // Prompt
    auto promptElement {driver.findElement(webdriverxx::LOCATION_STRATEGY::XPATH, "//a[normalize-space()='See a sample prompt']").scrollIntoView()};
    driver.execute<std::nullptr_t>("arguments[0].click()", promptElement);
    status &= driver.getAlertText() == "What is your tool of choice?";
    driver.setAlertResponse("Webdriverxx");
    driver.dismissAlert(true);

    return !status;
}
