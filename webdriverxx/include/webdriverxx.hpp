#pragma once

#include "cpr/api.h"
#include "json.hpp"
#include "base64.hpp"

#include <fstream>
#include <stdexcept>
#include <string>
#include <tuple>

// Note: Braced init doesn't work well, use `= init`
using json = nlohmann::json;

namespace webdriverxx {

    const cpr::Header HEADER_ACC_RECV_JSON {{"Content-Type", "application/json"}, {"Accept", "application/json"}};

    enum LOCATION_STRATEGY {CSS, TAGNAME, XPATH};

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

    class Element {
        private:
            const std::string elementId, elementURL;

        public:
            Element(const std::string &elementId, const std::string &sessionURL): 
                elementId(elementId), 
                elementURL(sessionURL + "/element/" + elementId) 
            { }

            Element& click() {
                cpr::Response response {cpr::Post(
                    cpr::Url(elementURL + "/click"), 
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            Element& sendKeys(const std::string &text) {
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

            Element& submit() { 
                sendKeys("" + Keys::Enter); 
                return *this;
            }

            Element& clear() {
                cpr::Response response {cpr::Post(
                    cpr::Url(elementURL + "/clear"),
                    cpr::Body{"{}"}, 
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return *this;
            }

            std::string getElementAttribute(const std::string &name) {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/attribute/" + name),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            std::string getElementProperty(const std::string &name) {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/property/" + name),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            std::string getElementCSSValue(const std::string &name) {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/css/" + name),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            std::string getElementText() {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/text"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            std::string getElementTagName() {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/name"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            bool isElementEnabled() {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/enabled"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                return json::parse(response.text)["value"];
            }

            void save_screenshot(const std::string &ofile) {
                cpr::Response response {cpr::Get(
                    cpr::Url(elementURL + "/screenshot"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                std::ofstream imageFS {ofile, std::ios::binary};
                std::string decoded {Base64::base64Decode(json::parse(response.text)["value"])};
                imageFS.write(decoded.data(), static_cast<long>(decoded.size()));
            }
    };

    class Driver {
        private:
            const std::string binaryPath, baseURL, sessionId, sessionURL;
            bool running {false};

        private:
            std::string startSession() {
                json payload {{
                    "capabilities", {{
                        "alwaysMatch", {{
                            "moz:firefoxOptions", {{ "binary", binaryPath }}
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

        public:
            Driver(
                const std::string &binaryPath = "C:\\Program Files\\Mozilla Firefox\\firefox.exe", 
                const std::string &baseURL = "http://127.0.0.1:4444"
            ): 
                binaryPath(binaryPath), 
                baseURL(baseURL), 
                sessionId(startSession()), 
                sessionURL(baseURL + "/session/" + sessionId) 
            { running = true; }

            ~Driver() { if (running) quit(); }

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
                cpr::Post(
                    cpr::Url(sessionURL + "/url"), 
                    cpr::Body{payload.dump()}, 
                    HEADER_ACC_RECV_JSON
                );

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

            std::tuple<unsigned int, unsigned int, unsigned int> getTimeouts() {
                cpr::Response response {cpr::Get(
                    cpr::Url(sessionURL + "/timeouts"),
                    HEADER_ACC_RECV_JSON
                )};
                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json timeouts = json::parse(response.text)["value"];
                return { timeouts["script"], timeouts["pageLoad"], timeouts["implicit"] };
            }
            
            void setTimeouts(const std::tuple<unsigned int, unsigned int, unsigned int> &timeouts) {
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

            void save_screenshot(const std::string &ofile) {
                cpr::Response response {cpr::Get(
                    cpr::Url(sessionURL + "/screenshot"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                std::ofstream imageFS {ofile, std::ios::binary};
                std::string decoded {Base64::base64Decode(json::parse(response.text)["value"])};
                imageFS.write(decoded.data(), static_cast<long>(decoded.size()));
            }

            std::string getCurrentURL() {
                cpr::Response response {cpr::Get(
                    cpr::Url(sessionURL + "/url"),
                    HEADER_ACC_RECV_JSON
                )};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text);
                return responseJson["value"];
            }

            std::string getTitle() {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/title"))};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text);
                return responseJson["value"];
            }

            std::string getPageSource() {
                cpr::Response response {cpr::Get(cpr::Url(sessionURL + "/source"))};

                if (response.status_code != 200)
                    throw std::runtime_error(response.text);

                json responseJson = json::parse(response.text);
                return responseJson["value"];
            }

            Element findElement(const LOCATION_STRATEGY &strategy, const std::string &criteria) {
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

                json responseJson = json::parse(response.text);
                return Element(responseJson["value"].begin().value(), sessionURL);
            }

    };
}
