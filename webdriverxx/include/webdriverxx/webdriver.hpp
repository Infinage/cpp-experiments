#pragma once

#include "capabilities.hpp"
#include "pageoptions.hpp"
#include "rect.hpp"
#include "cookie.hpp"
#include "timeout.hpp"
#include "element.hpp"

namespace webdriverxx {

    // Webdriver constants
    enum WINDOW_TYPE {TAB, WINDOW};

    class Driver {
        private:
            const Capabilities capabilities;
            const std::string port, baseURL; 
            const std::string sessionId, sessionURL;
            bool running {false};

        private:
            std::string startSession() {
                if (!status()) throw std::runtime_error("Webdriver not in ready state");
                json response = sendRequest(POST, baseURL + "/session", static_cast<json>(capabilities).dump());
                return response["value"]["sessionId"];
            }

        public:
            Driver(
                const Capabilities &cap_,
                const std::string  &port_,
                const std::string  &sessionId_  = ""
            ):
                capabilities(cap_), port(port_),
                baseURL("http://127.0.0.1:" + port), 
                sessionId(sessionId_.empty()? startSession(): sessionId_), 
                sessionURL(baseURL + "/session/" + sessionId) 
            { running = true; }

            ~Driver() { if (running) quit(); }

            bool status() {
                // Ignore errors and parse the errors as JSON
                json response = sendRequest(GET, baseURL + "/status", "{}", 200, true);
                if (response.value("status_code", 200) != 200) return false;
                else return response["value"]["ready"];
            }

            void quit() {
                sendRequest(DELETE, sessionURL);
                running = false;
            }

            Rect minimize() {
                json response = sendRequest(POST, sessionURL + "/window/minimize");
                return Rect{response["value"]};
            }

            Rect maximize() {
                json response = sendRequest(POST, sessionURL + "/window/maximize");
                return Rect{response["value"]};
            }

            Rect fullscreen() {
                json response = sendRequest(POST, sessionURL + "/window/fullscreen");
                return Rect{response["value"]};
            }

            Driver& navigateTo(const std::string &url) {
                json payload {{ "url", url }};
                sendRequest(POST, sessionURL + "/url", payload.dump());
                return *this;
            }

            Driver& back() {
                sendRequest(POST, sessionURL + "/back");
                return *this;
            }

            Driver& forward() {
                sendRequest(POST, sessionURL + "/forward");
                return *this;
            }

            Driver& refresh() {
                sendRequest(POST, sessionURL + "/refresh");
                return *this;
            }

            Timeout getTimeouts() const {
                json response = sendRequest(GET, sessionURL + "/timeouts");
                return { 
                    .script=response["value"]["script"], 
                    .pageLoad=response["value"]["pageLoad"], 
                    .implicit=response["value"]["implicit"] 
                };
            }
            
            Driver &setTimeouts(const Timeout &timeouts) {
                if (!timeouts.script && !timeouts.pageLoad && !timeouts.implicit)
                    throw std::invalid_argument("Atleast one timeout must be set.");

                json payload = json::object();
                if (timeouts.script) payload["script"] = *timeouts.script;
                if (timeouts.pageLoad) payload["pageLoad"] = *timeouts.pageLoad;
                if (timeouts.implicit) payload["implicit"] = *timeouts.implicit;

                sendRequest(POST, sessionURL + "/timeouts", payload.dump());
                return *this;
            }

            Driver &save_screenshot(const std::string &ofile) {
                json response = sendRequest(GET, sessionURL + "/screenshot");
                std::ofstream imageFS {ofile, std::ios::binary};
                std::string decoded {Base64::base64Decode(response["value"])};
                imageFS.write(decoded.data(), static_cast<long>(decoded.size()));
                return *this;
            }

            std::string getCurrentURL() const {
                json response = sendRequest(GET, sessionURL + "/url");
                return response["value"];
            }

            std::string getTitle() const {
                json response = sendRequest(GET, sessionURL + "/title");
                return response["value"];
            }

            std::string getPageSource() const {
                json response = sendRequest(GET, sessionURL + "/source");
                return response["value"];
            }

            Element findElement(const LOCATION_STRATEGY &strategy, const std::string &criteria) const {
                std::string strategyKeyword;
                switch (strategy) {
                    case CSS: strategyKeyword = "css selector"; break;
                    case TAGNAME: strategyKeyword = "tag name"; break;
                    case XPATH: strategyKeyword = "xpath"; break;
                }

                json payload {{"using", strategyKeyword}, {"value", criteria}};
                json response = sendRequest(POST, sessionURL + "/element", payload.dump());
                return Element(
                    response["value"].begin().key(), 
                    response["value"].begin().value(), 
                    sessionURL
                );
            }

            std::vector<Element> findElements(const LOCATION_STRATEGY &strategy, const std::string &criteria) const {
                std::string strategyKeyword;
                switch (strategy) {
                    case CSS: strategyKeyword = "css selector"; break;
                    case TAGNAME: strategyKeyword = "tag name"; break;
                    case XPATH: strategyKeyword = "xpath"; break;
                }

                json payload {{"using", strategyKeyword}, {"value", criteria}};
                json response = sendRequest(POST, sessionURL + "/elements", payload.dump());
                std::vector<Element> elements;
                std::transform(response["value"].begin(), response["value"].end(), std::back_inserter(elements), 
                    [&](const json::value_type &ele) {
                        return Element{ele.begin().key(), ele.begin().value(), sessionURL};
                    }
                );
                return elements;
            }

            std::string getWindowHandle() const {
                json response = sendRequest(GET, sessionURL + "/window");
                return response["value"];
            }

            Driver &closeWindow() {
                sendRequest(DELETE, sessionURL + "/window");
                return *this;
            }
            
            Driver &switchWindow(const std::string &windowId) {
                json payload {{ "handle", windowId }};
                sendRequest(POST, sessionURL + "/window", payload.dump());
                return *this;
            }

            std::vector<std::string> getWindowHandles() const {
                json response = sendRequest(GET, sessionURL + "/window/handles");
                return {response["value"].begin(), response["value"].end()};
            }

            std::string newWindow(const WINDOW_TYPE &type) {
                json payload {{ "type", type == WINDOW_TYPE::WINDOW? "window": "tab" }};
                json response = sendRequest(POST, sessionURL + "/window/new", payload.dump());
                return response["value"]["handle"];
            }

            Driver &switchToParentFrame() {
                sendRequest(POST, sessionURL + "/frame/parent");
                return *this;
            }

            Driver &switchFrame(const int index = -1) {
                json payload;
                if (index < 0) payload = {{ "id", nullptr }};
                else payload = {{ "id", index }};
                sendRequest(POST, sessionURL + "/frame", payload.dump());
                return *this;
            }

            Driver &switchFrame(const Element& element) {
                json payload {{ 
                    "id", {{ element.elementRef, element.elementId }}
                }};
                sendRequest(POST, sessionURL + "/frame", payload.dump());
                return *this;
            }

            Driver &dismissAlert(bool accept) {
                sendRequest(POST, sessionURL + "/alert/" + (accept? "accept": "dismiss"));
                return *this;
            }

            std::string getAlertText() const {
                json response = sendRequest(GET, sessionURL + "/alert/text");
                return response["value"];
            }

            Driver &setAlertResponse(const std::string &text) {
                json payload = {{ "text", text }};
                sendRequest(POST, sessionURL + "/alert/text", payload.dump());
                return *this;
            }

            template<typename T>
            T execute(const std::string &code, const json &args = json::array()) {
                json payload = {{ "script", code }, { "args", args.is_array()? args: json::array({args}) }};
                json response = sendRequest(POST, sessionURL + "/execute/sync", payload.dump());
                return response["value"].get<T>();
            }

            std::vector<Cookie> getAllCookies() const {
                json response = sendRequest(GET, sessionURL + "/cookie");
                std::vector<Cookie> cookies;
                std::transform(response["value"].begin(), response["value"].end(), std::back_inserter(cookies), 
                    [&](const json::value_type &cookieJson) { return Cookie{cookieJson}; }
                );
                return cookies;
            }

            Driver &deleteAllCookies() {
                sendRequest(DELETE, sessionURL + "/cookie");
                return *this;
            }

            Cookie getCookie(const std::string &name) const {
                json response = sendRequest(GET, sessionURL + "/cookie/" + name);
                return Cookie{response["value"]};
            }

            Driver &addCookie(const Cookie &cookie) {
                json payload = {{"cookie", static_cast<json>(cookie) }};
                sendRequest(POST, sessionURL + "/cookie", payload.dump());
                return *this;
            }

            Driver &deleteCookie(const std::string &name) {
                sendRequest(DELETE, sessionURL + "/cookie/" + name);
                return *this;
            }

            Rect getWindowRect() const {
                json response = sendRequest(GET, sessionURL + "/window/rect");
                return Rect{response["value"]};
            }

            Driver &setWindowRect(const Rect& rect) {
                sendRequest(POST, sessionURL + "/window/rect", static_cast<json>(rect).dump());
                return *this;
            }

            Driver &print(const std::string &ofile, const PageOptions &opts = PageOptions{}) {
                json optsJson = static_cast<json>(opts);
                json response = sendRequest(POST, sessionURL + "/print", optsJson.empty()? "{}": optsJson.dump());
                std::ofstream imageFS {ofile, std::ios::binary};
                std::string decoded {Base64::base64Decode(response["value"])};
                imageFS.write(decoded.data(), static_cast<long>(decoded.size()));
                return *this;
            }
    };
}
