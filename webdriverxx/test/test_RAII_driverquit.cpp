#include "webdriverxx.hpp"

int main() {
    // Expected to fail, see if browser still closes correctly
    int status {true};
    try {
        webdriverxx::Driver driver;
        driver.findElement(webdriverxx::CSS, "#404");
    } catch(...) {
        status = webdriverxx::Driver::status("http://127.0.0.1:4444"); 
    }

    return !status;
}
