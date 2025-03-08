#include "webdriverxx.hpp"

int main() {
    std::string sessionId {webdriverxx::startSession()};
    webdriverxx::navigateTo("https://github.com/Infinage", sessionId);
    webdriverxx::save_screenshot("screenshot-full.png", sessionId);
    auto [script, pageLoad, implicit] = webdriverxx::getTimeouts(sessionId);
    webdriverxx::setTimeouts({script, pageLoad, 30}, sessionId);
    std::string elementId {webdriverxx::findElement(webdriverxx::LOCATION_STRATEGY::CSS, "img.avatar", sessionId)};
    webdriverxx::save_screenshot("screenshot.png", elementId, sessionId);
    webdriverxx::stopSession(sessionId);
    return 0;
}
