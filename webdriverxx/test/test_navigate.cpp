#include "webdriverxx.hpp"

int main() {
    webdriverxx::Driver driver;
    int status {driver.navigateTo("https://google.com").getTitle() == "Google"};
    return !status;
}
