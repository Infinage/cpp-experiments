#pragma once

#include "cpr/api.h"
#include "json.hpp"

#include <chrono>
#include <codecvt>
#include <locale>
#include <stdexcept>
#include <thread>

using json = nlohmann::json;

namespace webdriverxx {

    const cpr::Header HEADER_ACC_RECV_JSON {{"Content-Type", "application/json"}, {"Accept", "application/json"}};

    enum LOCATION_STRATEGY {CSS, TAGNAME, XPATH};
    enum WINDOW_TYPE {TAB, WINDOW};
    enum BROWSERS {MSEDGE, CHROME, FIREFOX};
    enum ORIENTATION {POTRAIT, LANDSCAPE};
    enum API_METHOD {GET, POST, DELETE};

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
            long long elapsed {(std::chrono::duration_cast<std::chrono::milliseconds>(now - start)).count()};
            if (timeoutMS >= 0 && elapsed >= timeoutMS) break;

            // Pause before polling again
            std::this_thread::sleep_for(std::chrono::milliseconds(pollIntervalMS));
        }

        return false;
    }

    inline json sendRequest(
        const API_METHOD &requestType, 
        const std::string &url, 
        const std::string &body = "{}", 
        const long OK = 200, bool ignoreError = false
    ) {
        cpr::Response response;
        switch (requestType) {
            case API_METHOD::GET:
                response = cpr::Get(cpr::Url(url), HEADER_ACC_RECV_JSON);
                break;
            case API_METHOD::POST:
                response = cpr::Post(cpr::Url(url), cpr::Body{body}, HEADER_ACC_RECV_JSON);
                break;
            case API_METHOD::DELETE:
                response = cpr::Delete(cpr::Url(url), HEADER_ACC_RECV_JSON);
                break;
        }

        if (response.status_code != OK && !ignoreError)
            throw std::runtime_error(response.text);
        else 
            return json::parse(response.text);
    }
}
