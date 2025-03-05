#include "webdriverxx.hpp"

int main() {
    std::string sessionId {webdriverxx::startSession()};
    webdriverxx::navigateTo("https://google.com", sessionId);
    int status {webdriverxx::getTitle(sessionId) == "Google"};
    webdriverxx::stopSession(sessionId);
    return !status;
}
