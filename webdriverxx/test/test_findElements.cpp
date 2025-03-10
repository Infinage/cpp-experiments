#include "test_config.hpp"

int main() {
    webdriverxx::Driver driver{BROWSER_NAME, BROWSER_BINARY, PORT};
    driver.navigateTo("https://github.com/Infinage/cpp-experiments/tree/main/webdriverxx");
    auto xpathFiltered {driver.findElements(webdriverxx::LOCATION_STRATEGY::XPATH, "//td[contains(text(), 'Done')]")};

    auto rows {driver.findElements(webdriverxx::LOCATION_STRATEGY::CSS, "markdown-accessiblity-table tbody tr")};
    std::size_t completedEndpoints {0};
    for (const auto &row: rows) {
        std::string text {row.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "td:nth-child(4)").getElementText()};
        if (text == "Done") completedEndpoints++;
    }

    int status {xpathFiltered.size() == completedEndpoints};
    return !status;
}
