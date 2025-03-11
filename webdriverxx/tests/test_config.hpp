#pragma once

#include "webdriverxx.hpp"

#include <cstdlib>
#include <stdexcept>

inline std::string getEnv(const std::string &key) {
    const char* envVal {std::getenv(key.c_str())};
    if (!envVal)
        throw std::runtime_error('`' + key + "` env variable not set.");
    return envVal;
}

inline webdriverxx::BROWSERS resolveBrowser(const std::string &browserName) {
    if (browserName == "FIREFOX") return webdriverxx::BROWSERS::FIREFOX;
    else if (browserName == "CHROME") return webdriverxx::BROWSERS::CHROME;
    else if (browserName == "MSEDGE") return webdriverxx::BROWSERS::MSEDGE;
    else throw std::runtime_error('`' + browserName + "` is not supported.");
}

const std::string BROWSER_NAME {getEnv({"BROWSER"})};
const webdriverxx::BROWSERS BROWSER_TYPE {resolveBrowser(BROWSER_NAME)};
const std::string BROWSER_BINARY {getEnv("BROWSER_BINARY")};
const std::string PORT {getEnv(BROWSER_NAME + "_PORT")};
