#pragma once

#include "cpr/api.h"
#include "json.hpp"
#include "base64.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

// Note: Braced init doesn't work well, use `= init`
using json = nlohmann::json;

namespace webdriverxx {

    const cpr::Header HEADER_ACC_RECV_JSON {{"Content-Type", "application/json"}, {"Accept", "application/json"}};

    enum LOCATION_STRATEGY {CSS, TAGNAME, XPATH};
    enum WINDOW_TYPE {TAB, WINDOW};
    enum BROWSERS {MSEDGE, CHROME, FIREFOX, AUTO};

    const std::unordered_map<BROWSERS, std::unordered_map<std::string, std::string>> defaultConfigs {
        {BROWSERS::FIREFOX, {{"browserSpecificOptsId", "moz:firefoxOptions"}}},
        {BROWSERS::CHROME,  {{"browserSpecificOptsId", "goog:chromeOptions"}}},
        {BROWSERS::MSEDGE,  {{"browserSpecificOptsId",     "ms:edgeOptions"}}},
    };

    enum class Keys: char16_t {
        Cancel = u'\uE001', Help = u'\uE002', Backspace = u'\uE003', Tab = u'\uE004',
        Clear = u'\uE005', Return = u'\uE006', Enter = u'\uE007', Pause = u'\uE00B',
        Escape = u'\uE00C', Space = u'\uE00D', Semicolon = u'\uE018', Equals = u'\uE019',

        NUM0 = u'\uE01A', NUM1 = u'\uE01B', NUM2 = u'\uE01C', NUM3 = u'\uE01D',
        NUM4 = u'\uE01E', NUM5 = u'\uE01F', NUM6 = u'\uE020', NUM7 = u'\uE021',
        NUM8 = u'\uE022', NUM9 = u'\uE023', Asterik = u'\uE024',

        Plus = u'\uE025', Comma = u'\uE026', Minus = u'\uE027', Dot = u'\uE028',
        FSlash = u'\uE029',

        F1 = u'\uE031', F2 = u'\uE032', F3 = u'\uE033', F4 = u'\uE034',
        F5 = u'\uE035', F6 = u'\uE036', F7 = u'\uE037', F8 = u'\uE038',
        F9 = u'\uE039', F10 = u'\uE03A', F11 = u'\uE03B', F12 = u'\uE03C',

        ZenkakuHankaku = u'\uE040', Shift = u'\uE050', Control = u'\uE051',
        Alt = u'\uE052', Meta = u'\uE053', PageUp = u'\uE054', PageDown = u'\uE055',
        End = u'\uE056', Home = u'\uE057', ArrowLeft = u'\uE058', ArrowUp = u'\uE059',
        ArrowRight = u'\uE05A', ArrowDown = u'\uE05B', Insert = u'\uE05C',
        Delete = u'\uE05D'
    };

    inline std::string utf16_to_utf8(const std::u16string& utf16_str) {
        static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
        return converter.to_bytes(utf16_str);
    }

    inline std::string operator+ (const std::string &str, const Keys& key) {
        return str + utf16_to_utf8(std::u16string(1, static_cast<char16_t>(key)));
    }

    template<typename Condition>
    inline bool waitUntil(const Condition &condition, long timeoutMS = -1, long pollIntervalMS = 500) {
        std::chrono::time_point start {std::chrono::steady_clock::now()};
        while (true) {
            // Return immediately if condition is met
            try {
                if (condition()) return true;
            } catch (...) {}

            // Current time
            std::chrono::time_point now {std::chrono::steady_clock::now()};
            long elapsed {(std::chrono::duration_cast<std::chrono::milliseconds>(now - start)).count()};
            if (timeoutMS >= 0 && elapsed >= timeoutMS) break;

            // Pause before polling again
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMS));
        }

        return false;
    }

    class Cookie {
        public:
            std::string name, value;
            std::optional<std::string> domain, path, sameSite;
            std::optional<bool> secureFlag, httpOnlyFlag;
            std::optional<unsigned long long> expiry, size;

            Cookie(const std::string &name, const std::string &value): 
                name(name), value(value) {}

            Cookie(const json &json_) {
                if (json_.is_array() || !json_.contains("name") || !json_.contains("value"))
                    throw std::runtime_error("Not a valid cookie: " + json_.dump());

                else {
                    name  = json_.at("name");
                    value = json_.at("value");

                    // Optional values
                    if (json_.contains("domain"))   domain = json_.at("domain");
                    if (json_.contains("path"))     path = json_.at("path");
                    if (json_.contains("sameSite")) sameSite = json_.at("sameSite");
                    if (json_.contains("secure"))   secureFlag = json_.at("secure");
                    if (json_.contains("httpOnly")) httpOnlyFlag = json_.at("httpOnly");
                    if (json_.contains("expiry"))   expiry = json_.at("expiry");
                    if (json_.contains("Size"))   size = json_.at("Size");
                }
            }

            operator json() const {
                json object = json{{"name", name}, {"value", value}};

                // Optional values
                if (domain)       object["domain"] = *domain;
                if (path)         object["path"] = *path;
                if (sameSite)     object["sameSite"] = *sameSite;
                if (secureFlag)   object["secure"] = *secureFlag;
                if (httpOnlyFlag) object["httpOnly"] = *httpOnlyFlag;
                if (expiry)       object["expiry"] = *expiry;
                if (size)         object["Size"] = *size;

                return object;
            }
    };

    class Element {
        private:
            const std::string elementRef, elementId, sessionURL, elementURL;
            friend class Driver;

        public:
            explicit operator bool() const { 
                return !elementId.empty(); 
            }

            operator json() const {
                return json{{elementRef, elementId}};
            }

            Element(const std::string &elementRef, const std::string &elementId, const std::string &sessionURL): 
                elementRef(elementRef), 
                elementId(elementId), 
                sessionURL(sessionURL),
                elementURL(sessionURL + "/element/" + elementId) 
            { }

            Element &click() {
                cpr::Response response {cpr::Post(
                    cpr::Url(elementURL + "/click"), 
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Element &sendKeys(const std::string &text) {
                json payload {{ "text", text }};
                cpr::Response response {cpr::Post(
                    cpr::Url(elementURL + "/value"), 
                    cpr::Body{payload.dump()}, 
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Element &submit() { 
                sendKeys("" + Keys::Enter); 
                return *this;
            }

            Element &clear() {
                cpr::Response response {cpr::Post(
                    cpr::Url(elementURL + "/clear"),
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Element &scrollIntoView() {
                json payload = {
                    { "script", "arguments[0].scrollIntoView({behavior: 'smooth', block: 'center'});" }, 
                    { "args", json::array({{{elementRef, elementId}}}) }
                };

                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/execute/sync"), 
                    cpr::Body{payload.dump()}, 
                    HEADER_ACC_RECV_JSON)
                };

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            std::string getElementAttribute(const std::string &name) const {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/attribute/" + name),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            std::string getElementProperty(const std::string &name) const {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/property/" + name),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            std::string getElementCSSValue(const std::string &name) const {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/css/" + name),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            std::string getElementText() const {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/text"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            std::string getElementTagName() const {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/name"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            bool isEnabled() const {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/enabled"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            bool isSelected() const {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/selected"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            Element &save_screenshot(const std::string &ofile) {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/screenshot"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                std::ofstream imageFS {ofile, std::ios::binary};
                std::string decoded {Base64::base64Decode(json::parse(response.text)["value"])};
                imageFS.write(decoded.data(), static_cast<long>(decoded.size()));

                return *this;
            }

            Element getActiveElement() const {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/element/active"),
                    HEADER_ACC_RECV_JSON
                )}; 

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text)["value"];
                return Element(responseJson.begin().key(), responseJson.begin().value(), sessionURL);
            }

            Element findElement(const LOCATION_STRATEGY &strategy, const std::string &criteria) const {
                std::string strategyKeyword;
                switch (strategy) {
                    case CSS: strategyKeyword = "css selector"; break;
                    case TAGNAME: strategyKeyword = "tag name"; break;
                    case XPATH: strategyKeyword = "xpath"; break;
                }

                json payload {{"using", strategyKeyword}, {"value", criteria}};
                cpr::Response response {cpr::Post(
                    cpr::Url(elementURL + "/element"),
                    cpr::Body{payload.dump()},
                    HEADER_ACC_RECV_JSON
                )}; 

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text)["value"];
                return Element(responseJson.begin().key(), responseJson.begin().value(), sessionURL);
            }

            std::vector<Element> findElements(const LOCATION_STRATEGY &strategy, const std::string &criteria) const {
                std::string strategyKeyword;
                switch (strategy) {
                    case CSS: strategyKeyword = "css selector"; break;
                    case TAGNAME: strategyKeyword = "tag name"; break;
                    case XPATH: strategyKeyword = "xpath"; break;
                }

                json payload {{"using", strategyKeyword}, {"value", criteria}};
                cpr::Response response {cpr::Post(
                    cpr::Url(elementURL + "/elements"),
                    cpr::Body{payload.dump()},
                    HEADER_ACC_RECV_JSON
                )}; 

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text)["value"];
                std::vector<Element> elements;
                std::transform(responseJson.begin(), responseJson.end(), std::back_inserter(elements), 
                    [&](const json::value_type &ele) {
                        return Element{ele.begin().key(), ele.begin().value(), sessionURL};
                    }
                );
                return elements;
            }
    };

    class Driver {
        private:
            const BROWSERS browserName;
            const std::string port, binaryPath, baseURL; 
            const std::string sessionId, sessionURL;
            bool running {false};

        private:
            std::string startSession() {
                if (!status()) 
                    throw std::runtime_error("Webdriver not in ready state");

                json payload {{
                    "capabilities", {{
                        "alwaysMatch", {{
                            defaultConfigs.at(browserName).at("browserSpecificOptsId"), 
                                {{ "binary", binaryPath }}
                        }}
                    }}
                }};

                cpr::Response response {cpr::Post(
                    cpr::Url(baseURL + "/session"), 
                    cpr::Body{payload.dump()},
                    HEADER_ACC_RECV_JSON)
                };

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text);
                return responseJson["value"]["sessionId"];
            }

            std::string getBinaryPath() const {
                std::string envKey;
                switch (browserName) {
                    case (FIREFOX): envKey = "FIREFOX"; break;
                    case (CHROME): envKey = "CHROME"; break;
                    case (MSEDGE): envKey = "MSEDGE"; break;
                    default: envKey = "FIREFOX"; break;
                }
                envKey += "_BINARY";
                char *path {std::getenv(envKey.c_str())};
                if (!path)
                    throw std::runtime_error('`' + envKey + "` env variable not set.");
                return path;
            }

            static BROWSERS getBrowser() {
                char *browser {std::getenv("BROWSER")};
                if (!browser)
                    throw std::runtime_error("`BROWSER` env variable not set.");
                else if (std::strcmp(browser, "firefox") == 0)
                    return BROWSERS::FIREFOX;
                else if (std::strcmp(browser, "chrome") == 0)
                    return BROWSERS::CHROME;
                else if (std::strcmp(browser, "msedge") == 0)
                    return BROWSERS::MSEDGE;
                else
                    throw std::runtime_error('`' + std::string{browser} + "` not supported.");
            }

            static std::string getPort() {
                char *port {std::getenv("PORT")};
                if (!port)
                    throw std::runtime_error("`PORT` env variable not set.");
                return port;
            }

        public:
            Driver(
                const BROWSERS &browserName_ = BROWSERS::AUTO,
                const std::string &binaryPath_ = "",
                const unsigned short port_ = 0,
                const std::string &sessionId_  = ""
            ):
                browserName(browserName_ == BROWSERS::AUTO? getBrowser(): browserName_), 
                port(port_ == 0? getPort(): "4444"),
                binaryPath(binaryPath_.empty()? getBinaryPath(): binaryPath_),
                baseURL("http://127.0.0.1:" + port), 
                sessionId(sessionId_.empty()? startSession(): sessionId_), 
                sessionURL(baseURL + "/session/" + sessionId) 
            { running = true; }

            ~Driver() { if (running) quit(); }

            bool status() {
                cpr::Response response {cpr::Get(
                    cpr::Url(baseURL + "/status"), 
                    HEADER_ACC_RECV_JSON)
                };

                if (response.status_code != 200)
                    return false;
                else 
                    return json::parse(response.text)["value"]["ready"];
            }
            void quit() {
                cpr::Delete(cpr::Url(sessionURL)); 
                running = false;
            }

            Driver& minimize() {
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/window/minimize"),
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Driver& maximize() {
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/window/maximize"),
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Driver& navigateTo(const std::string &url) {
                json payload {{ "url", url }};
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/url"), 
                    cpr::Body{payload.dump()}, 
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Driver& back() {
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/back"), 
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                
                return *this;
            }

            Driver& forward() {
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/forward"), 
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Driver& refresh() {
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/refresh"), 
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            std::tuple<unsigned int, unsigned int, unsigned int> getTimeouts() const {
                cpr::Response response {cpr::Get(
                    cpr::Url(sessionURL + "/timeouts"),
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json timeouts = json::parse(response.text)["value"];
                return { timeouts["script"], timeouts["pageLoad"], timeouts["implicit"] };
            }
            
            Driver &setTimeouts(const std::tuple<unsigned int, unsigned int, unsigned int> &timeouts) {
                json payload {
                    {"script",   std::get<0>(timeouts)},
                    {"pageLoad", std::get<1>(timeouts)},
                    {"implicit", std::get<2>(timeouts)}
                };

                if (payload["script"] < 0 || payload["pageLoad"] < 0 || payload["implicit"] < 0)
                    throw std::runtime_error("Timeouts cannot be negative");

                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/timeouts"),
                    cpr::Body(payload.dump()),
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Driver& setImplicitTimeoutMS(unsigned int timeout) {
                json payload {{"implicit", timeout }};

                if (payload["implicit"] < 0)
                    throw std::runtime_error("Timeouts cannot be negative");

                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/timeouts"),
                    cpr::Body(payload.dump()),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Driver &save_screenshot(const std::string &ofile) {
                cpr::Response response {cpr::Get(
                    cpr::Url(sessionURL + "/screenshot"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                std::ofstream imageFS {ofile, std::ios::binary};
                std::string decoded {Base64::base64Decode(json::parse(response.text)["value"])};
                imageFS.write(decoded.data(), static_cast<long>(decoded.size()));

                return *this;
            }

            std::string getCurrentURL() const {
                cpr::Response response {cpr::Get(
                    cpr::Url(sessionURL + "/url"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text);
                return responseJson["value"];
            }

            std::string getTitle() const {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/title"))};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text);
                return responseJson["value"];
            }

            std::string getPageSource() const {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/source"))};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text);
                return responseJson["value"];
            }

            Element findElement(const LOCATION_STRATEGY &strategy, const std::string &criteria) const {
                std::string strategyKeyword;
                switch (strategy) {
                    case CSS: strategyKeyword = "css selector"; break;
                    case TAGNAME: strategyKeyword = "tag name"; break;
                    case XPATH: strategyKeyword = "xpath"; break;
                }

                json payload {{"using", strategyKeyword}, {"value", criteria}};
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/element"),
                    cpr::Body{payload.dump()},
                    HEADER_ACC_RECV_JSON
                )}; 

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text)["value"];
                return Element(responseJson.begin().key(), responseJson.begin().value(), sessionURL);
            }

            std::vector<Element> findElements(const LOCATION_STRATEGY &strategy, const std::string &criteria) const {
                std::string strategyKeyword;
                switch (strategy) {
                    case CSS: strategyKeyword = "css selector"; break;
                    case TAGNAME: strategyKeyword = "tag name"; break;
                    case XPATH: strategyKeyword = "xpath"; break;
                }

                json payload {{"using", strategyKeyword}, {"value", criteria}};
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/elements"),
                    cpr::Body{payload.dump()},
                    HEADER_ACC_RECV_JSON
                )}; 

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text)["value"];
                std::vector<Element> elements;
                std::transform(responseJson.begin(), responseJson.end(), std::back_inserter(elements), 
                    [&](const json::value_type &ele) {
                        return Element{ele.begin().key(), ele.begin().value(), sessionURL};
                    }
                );
                return elements;
            }

            std::string getWindowHandle() const {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/window"),  HEADER_ACC_RECV_JSON)}; 
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return json::parse(response.text)["value"];
            }

            Driver &closeWindow() {
                cpr::Response response {cpr::Delete(cpr::Url(sessionURL + "/window"))};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }
            
            Driver &switchWindow(const std::string &windowId) {
                json payload {{ "handle", windowId }};
                cpr::Response response {cpr::Post(cpr::Url(sessionURL + "/window"), cpr::Body{payload.dump()}, HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }

            std::vector<std::string> getWindowHandles() const {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/window/handles"),  HEADER_ACC_RECV_JSON)}; 
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                json handlesJSON = json::parse(response.text)["value"];
                return {handlesJSON.begin(), handlesJSON.end()};
            }

            std::string newWindow(const WINDOW_TYPE &type) {
                json payload {{ "type", type == WINDOW_TYPE::WINDOW? "window": "tab" }};
                cpr::Response response {cpr::Post(cpr::Url(sessionURL + "/window/new"), cpr::Body{payload.dump()}, HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return json::parse(response.text)["value"]["handle"];
            }

            Driver &switchToParentFrame() {
                cpr::Response response {cpr::Post(cpr::Url(sessionURL + "/frame/parent"), cpr::Body{"{}"}, HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }

            Driver &switchFrame(const int index = -1) {
                json payload;
                if (index < 0) payload = {{ "id", nullptr }};
                else payload = {{ "id", index }};
                cpr::Response response {cpr::Post(cpr::Url(sessionURL + "/frame"), cpr::Body{payload.dump()}, HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }

            Driver &switchFrame(const Element& element) {
                json payload {{ 
                    "id", {{ element.elementRef, element.elementId }}
                }};
                cpr::Response response {cpr::Post(cpr::Url(sessionURL + "/frame"), cpr::Body{payload.dump()}, HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }

            Driver &dismissAlert(bool accept) {
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/alert/" + (accept? "accept": "dismiss")), 
                    cpr::Body{"{}"}, HEADER_ACC_RECV_JSON)
                };
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }

            std::string getAlertText() const {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/alert/text"), HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return json::parse(response.text)["value"];
            }

            Driver &setAlertResponse(const std::string &text) {
                json payload = {{ "text", text }};
                cpr::Response response {cpr::Post(cpr::Url(sessionURL + "/alert/text"), cpr::Body{payload.dump()}, HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }

            template<typename T>
            T execute(const std::string &code, const json &args = json::array()) {
                json payload = {{ "script", code }, { "args", args.is_array()? args: json::array({args}) }};

                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/execute/sync"),
                    cpr::Body{payload.dump()}, 
                    HEADER_ACC_RECV_JSON)
                };

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"].get<T>();
            }

            std::vector<Cookie> getAllCookies() const {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/cookie"), HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json cookiesJson = json::parse(response.text)["value"];
                std::vector<Cookie> cookies;
                std::transform(cookiesJson.begin(), cookiesJson.end(), std::back_inserter(cookies), 
                    [&](const json::value_type &cookieJson) { return Cookie{cookieJson}; }
                );

                return cookies;
            }

            Driver &deleteAllCookies() {
                cpr::Response response {cpr::Delete(cpr::Url(sessionURL + "/cookie"), HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }

            Cookie getCookie(const std::string &name) const {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/cookie/" + name), HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return Cookie{json::parse(response.text)["value"]};
            }

            Driver &addCookie(const Cookie &cookie) {
                json payload = {{"cookie", static_cast<json>(cookie) }};
                cpr::Response response {cpr::Post(
                    cpr::Url(sessionURL + "/cookie"), 
                    cpr::Body{payload.dump()},
                    HEADER_ACC_RECV_JSON)
                };
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }

            Driver &deleteCookie(const std::string &name) {
                cpr::Response response {cpr::Delete(cpr::Url(sessionURL + "/cookie/" + name), HEADER_ACC_RECV_JSON)};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);
                return *this;
            }
    };
}
