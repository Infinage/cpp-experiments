#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_TYPE, BROWSER_BINARY}, PORT};

    driver.navigateTo("https://www.selenium.dev/documentation/webdriver/interactions/alerts");
    int status {true};

    // Simple Alert
    driver.findElement(webdriverxx::LOCATION_STRATEGY::XPATH, "//a[normalize-space()='See an example alert']").scrollIntoView(100).click();
    status &= driver.getAlertText() == "Sample alert";
    driver.dismissAlert(false);

    // Accept / Dismiss Alert
    auto confirmElement {driver.findElement(webdriverxx::LOCATION_STRATEGY::XPATH, "//a[normalize-space()='See a sample confirm']").scrollIntoView()};
    driver.execute<std::nullptr_t>("arguments[0].click()", confirmElement);
    status &= driver.getAlertText() == "Are you sure?";
    driver.dismissAlert(true);

    // Prompt
    driver.findElement(webdriverxx::LOCATION_STRATEGY::XPATH, "//a[normalize-space()='See a sample prompt']").scrollIntoView().clickJS();
    status &= driver.getAlertText() == "What is your tool of choice?";
    driver.setAlertResponse("Webdriverxx");
    driver.dismissAlert(true);

    return !status;
}
