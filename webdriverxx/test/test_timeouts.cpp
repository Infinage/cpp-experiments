#include "webdriverxx.hpp"

int main() {
    const std::string sessionId {webdriverxx::startSession()};
    webdriverxx::setTimeouts({0, 0, 0}, sessionId);
    const auto [scriptTO, pageLoadTO, implicitTO] {webdriverxx::getTimeouts(sessionId)};
    int status {scriptTO == 0 && pageLoadTO == 0 && implicitTO == 0};
    webdriverxx::stopSession(sessionId);
    return !status;
}
