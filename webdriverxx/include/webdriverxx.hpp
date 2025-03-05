#pragma once

#include "cpr/api.h"
#include "json.hpp"

#include <stdexcept>
#include <string>

using json = nlohmann::json;

namespace webdriverxx {

    const std::string BASE_URL {"http://localhost:4444"};
    enum LOCATION_STRATEGY {CSS, TAGNAME, XPATH};
    const cpr::Header HEADER_ACC_RECV_JSON {{"Content-Type", "application/json"}, {"Accept", "application/json"}};

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

    inline std::string startSession() {
        json payload {{
            "capabilities", {{
                "alwaysMatch", {{
                    "moz:firefoxOptions", {{
                        "binary", "C:\\Program Files\\Mozilla Firefox\\firefox.exe"
                    }}
                }}
            }}
        }};

        cpr::Response response {cpr::Post(
            cpr::Url(BASE_URL + "/session"),
            cpr::Body{payload.dump()}, 
            HEADER_ACC_RECV_JSON
        )};

        if (response.status_code != 200)
            throw std::runtime_error(response.text);

        // Parse response json and unwrap if neccessary
        json responseJson {json::parse(response.text)};
        if (responseJson.is_array() && !responseJson.empty()) 
            responseJson = responseJson[0];

        return responseJson["value"]["sessionId"];
    }

    inline void stopSession(const std::string &sessionId) {
        cpr::Delete(cpr::Url(BASE_URL + "/session/" + sessionId));
    }

    inline void minimize(const std::string &sessionId) {
        cpr::Response response {cpr::Post(
            cpr::Url(BASE_URL + "/session/" + sessionId + "/window/minimize"),
            cpr::Body{"{}"}, 
            HEADER_ACC_RECV_JSON
        )};
        if (response.status_code != 200)
            throw std::runtime_error(response.text);
    }

    inline void maximize(const std::string &sessionId) {
        cpr::Response response {cpr::Post(
            cpr::Url(BASE_URL + "/session/" + sessionId + "/window/maximize"),
            cpr::Body{"{}"}, 
            HEADER_ACC_RECV_JSON
        )};
        if (response.status_code != 200)
            throw std::runtime_error(response.text);
    }

    inline void navigateTo(const std::string &url, const std::string &sessionId) {
        json payload {{ "url", url }};
        cpr::Post(
            cpr::Url(BASE_URL + "/session/" + sessionId + "/url"), 
            cpr::Body{payload.dump()}, 
            HEADER_ACC_RECV_JSON
        );
    }

    inline std::string getTitle(const std::string &sessionId) {
        cpr::Response response {cpr::Get(cpr::Url(BASE_URL + "/session/" + sessionId + "/title"))};

        if (response.status_code != 200)
            throw std::runtime_error(response.text);

        json responseJson {json::parse(response.text)};
        if (responseJson.is_array() && !responseJson.empty()) 
            responseJson = responseJson[0];

        return responseJson["value"];
    }

    inline std::string getPageSource(const std::string &sessionId) {
        cpr::Response response {cpr::Get(cpr::Url(BASE_URL + "/session/" + sessionId + "/source"))};

        if (response.status_code != 200)
            throw std::runtime_error(response.text);

        json responseJson {json::parse(response.text)};
        if (responseJson.is_array() && !responseJson.empty()) 
            responseJson = responseJson[0];

        return responseJson["value"];
    }

    inline std::string findElement(const LOCATION_STRATEGY &strategy, const std::string &criteria, const std::string &sessionId) {
        std::string strategyKeyword;
        switch (strategy) {
            case CSS: strategyKeyword = "css selector"; break;
            case TAGNAME: strategyKeyword = "tag name"; break;
            case XPATH: strategyKeyword = "xpath"; break;
        }

        json payload {{"using", strategyKeyword}, {"value", criteria}};
        cpr::Response response {cpr::Post(
            cpr::Url(BASE_URL + "/session/" + sessionId + "/element"),
            cpr::Body{payload.dump()},
            HEADER_ACC_RECV_JSON
        )}; 

        if (response.status_code != 200)
            throw std::runtime_error(response.text);

        json responseJson {json::parse(response.text)};
        if (responseJson.is_array() && !responseJson.empty()) 
            responseJson = responseJson[0];

        return responseJson["value"].begin().value();
    }

    inline void click(const std::string &elementId, const std::string &sessionId) {
        cpr::Response response {cpr::Post(
            cpr::Url(BASE_URL + "/session/" + sessionId + "/element/" + elementId + "/click"), 
            cpr::Body{"{}"}, 
            HEADER_ACC_RECV_JSON
        )};
        if (response.status_code != 200)
            throw std::runtime_error(response.text);
    }

    inline void sendKeys(const std::string &elementId, const std::string &text, const std::string &sessionId) {
        json payload {{ "text", text }};
        cpr::Response response {cpr::Post(
            cpr::Url(BASE_URL + "/session/" + sessionId + "/element/" + elementId + "/value"), 
            cpr::Body{payload.dump()}, 
            HEADER_ACC_RECV_JSON
        )};
        if (response.status_code != 200)
            throw std::runtime_error(response.text);
    }

    inline void clear(const std::string &elementId, const std::string &sessionId) {
        cpr::Response response {cpr::Post(
            cpr::Url(BASE_URL + "/session/" + sessionId + "/element/" + elementId + "/clear"),
            cpr::Body{"{}"}, 
            HEADER_ACC_RECV_JSON
        )};
        if (response.status_code != 200)
            throw std::runtime_error(response.text);
    }
}
