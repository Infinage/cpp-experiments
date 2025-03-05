#include "webdriverxx.hpp"

int main() {
    const std::string sessionId {webdriverxx::startSession()};
    webdriverxx::navigateTo("https://google.com", sessionId);
    std::string elementId {webdriverxx::findElement(webdriverxx::LOCATION_STRATEGY::CSS, "textarea[title='Search']", sessionId)};
    webdriverxx::clear(elementId, sessionId);
    webdriverxx::sendKeys(elementId, "Hello world" + webdriverxx::Keys::Enter, sessionId);
    int status {webdriverxx::getTitle(sessionId) == "Hello world - Google Search"};
    webdriverxx::stopSession(sessionId);
    return !status;
}
