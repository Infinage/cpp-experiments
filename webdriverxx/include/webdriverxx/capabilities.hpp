#pragma once

#include "json.hpp"

#include <iostream>

using json = nlohmann::json;

namespace webdriverxx {

    enum BROWSERS {MSEDGE, CHROME, FIREFOX};

    class Capabilities {
        private:
            const BROWSERS browserType;
            const std::string binaryPath;

            std::optional<bool> _headless;
            std::optional<bool> _disableGPU;
            std::optional<bool> _startMaximized;
            std::optional<bool> _disableExtensions;
            std::optional<bool> _ignoreCertErrors;
            std::optional<bool> _disablePopupBlocking;

            std::optional<int>  _windowHeight;
            std::optional<int>  _windowWidth;

            std::optional<std::string> _userAgent;
            std::optional<std::string> _downloadDir;
            std::optional<std::string> _proxy;

        public:
            Capabilities(const BROWSERS &browserType, const std::string &binaryPath): 
                browserType(browserType), binaryPath(binaryPath) {}

            // Builder pattern for setting capabilities
            Capabilities &headless(bool flag) { _headless = flag; return *this; }
            Capabilities &disableGPU(bool flag) { _disableGPU = flag; return *this; }
            Capabilities &startMaximized(bool flag) { _startMaximized = flag; return *this; }
            Capabilities &disableExtensions(bool flag) { _disableExtensions = flag; return *this; }
            Capabilities &ignoreCertErrors(bool flag) { _ignoreCertErrors = flag; return *this; }
            Capabilities &disablePopupBlocking(bool flag) { _disablePopupBlocking = flag; return *this; }
            Capabilities &userAgent(const std::string &agent) { _userAgent = agent; return *this; }
            Capabilities &downloadDir(const std::string &directory) { _downloadDir = directory; return *this; }
            Capabilities &proxy(const std::string &proxyURL) { _proxy = proxyURL; return *this; }
            Capabilities &windowSize(int height, int width) { _windowHeight = height; _windowWidth = width; return *this; }

            operator json() const {

                // Warn unsupported opts
                if (browserType == BROWSERS::FIREFOX) {
                    if (_startMaximized && *_startMaximized)
                        std::cerr << "Start maximized is not supported in firefox. Please maximize by calling `driver.maximize` instead.\n";
                }

                // Base capabilities
                std::string optsId;
                switch (browserType) {
                    case BROWSERS::FIREFOX: optsId = "moz:firefoxOptions"; break;
                    case BROWSERS::CHROME: optsId = "goog:chromeOptions"; break;
                    case BROWSERS::MSEDGE: optsId = "ms:edgeOptions"; break;
                }

                json alwaysMatch = { 
                    { optsId, {  
                        { "args", json::array() },
                        { "binary", binaryPath }
                    }}
                };

                // Feed in the args
                if (_headless && *_headless) 
                    alwaysMatch[optsId]["args"].push_back("--headless");
                if (_disableGPU && *_disableGPU) 
                    alwaysMatch[optsId]["args"].push_back("--disable-gpu");
                if (_startMaximized && *_startMaximized) 
                    alwaysMatch[optsId]["args"].push_back("--start-maximized");
                if (_disablePopupBlocking && *_disablePopupBlocking && browserType != BROWSERS::FIREFOX) 
                    alwaysMatch[optsId]["args"].push_back("--disable-popup-blocking");

                // Handle Ignore Certificate Errors
                if (_ignoreCertErrors && *_ignoreCertErrors)
                    alwaysMatch["acceptInsecureCerts"] = true;

                // Firefox specific opts
                if (browserType == BROWSERS::FIREFOX) {
                    if (_windowHeight && _windowWidth) {
                        alwaysMatch[optsId]["args"].push_back("--height=" + std::to_string(*_windowHeight)); 
                        alwaysMatch[optsId]["args"].push_back("--width=" + std::to_string(*_windowWidth));
                    }
                    if (_userAgent)
                        alwaysMatch["moz:firefoxOptions"]["prefs"]["general.useragent.override"] = *_userAgent;
                    if (_disableExtensions && *_disableExtensions)
                        alwaysMatch["moz:firefoxOptions"]["prefs"]["extensions.enabled"] = false;
                    if (_downloadDir) {
                        alwaysMatch["moz:firefoxOptions"]["prefs"] = {
                            {"browser.download.dir", *_downloadDir},
                            {"browser.download.folderList", 2},
                            {"browser.helperApps.neverAsk.saveToDisk", "application/pdf"}
                        };
                    }
                } 

                // Chrome / MS Edge specific opts
                else {
                    if (_windowHeight && _windowWidth)
                        alwaysMatch[optsId]["args"].push_back("--window-size=" + std::to_string(*_windowHeight) + ',' + std::to_string(*_windowWidth));
                    if (_userAgent)
                        alwaysMatch[optsId]["args"].push_back("--user-agent=" + *_userAgent);
                    if (_disableExtensions && *_disableExtensions)
                        alwaysMatch[optsId]["args"].push_back("--disable-extensions");
                    if (_downloadDir) {
                        alwaysMatch[optsId]["prefs"] = {
                            {"download.default_directory", *_downloadDir},
                            {"download.prompt_for_download", false},
                            {"download.directory_upgrade", true},
                            {"safebrowsing.enabled", true}
                        };
                    }
                }

                // Final payload
                json payload = {{"capabilities", {{"alwaysMatch", alwaysMatch}}}};
                return payload;
            }
    };
}
