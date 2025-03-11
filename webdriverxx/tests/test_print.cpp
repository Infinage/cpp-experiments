#include "test_config.hpp"

#include <filesystem>

int main() {
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_TYPE, BROWSER_BINARY}, PORT};
    driver.navigateTo("https://github.com/Infinage");
    webdriverxx::PageOptions pageopts;
    driver.print("infinage.pdf");

    int status {std::filesystem::exists("infinage.pdf")};
    return !status;
}
