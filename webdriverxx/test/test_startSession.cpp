#include "webdriverxx.hpp"

int main() {
    std::string sessionID {webdriverxx::startSession()};
    webdriverxx::stopSession(sessionID);
    return 0;
}
