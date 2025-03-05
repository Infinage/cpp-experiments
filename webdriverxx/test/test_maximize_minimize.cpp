#include "webdriverxx.hpp"

int main() {
    const std::string sessionId {webdriverxx::startSession()};
    webdriverxx::maximize(sessionId);
    webdriverxx::minimize(sessionId);
    webdriverxx::stopSession(sessionId);
    return 0;
}
