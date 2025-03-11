#pragma once

#include "utils.hpp"
#include "rect.hpp"
#include "base64.hpp"

using json = nlohmann::json;

namespace webdriverxx {
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
                sendRequest(POST, elementURL + "/click");
                return *this;
            }

            Element &sendKeys(const std::string &text) {
                json payload {{ "text", text }};
                sendRequest(POST, elementURL + "/value", payload.dump());
                return *this;
            }

            Element &submit() { 
                sendKeys("" + Keys::Enter); 
                return *this;
            }

            Element &clear() {
                sendRequest(POST, elementURL + "/clear");
                return *this;
            }

            Element &scrollIntoView(unsigned short pauseMS = 0) {
                json payload = {
                    { "script", "arguments[0].scrollIntoView({behavior: 'smooth', block: 'center'});" }, 
                    { "args", json::array({{{elementRef, elementId}}}) }
                };
                sendRequest(POST, sessionURL + "/execute/sync", payload.dump());
                std::this_thread::sleep_for(std::chrono::milliseconds(pauseMS));
                return *this;
            }

            std::string getElementAttribute(const std::string &name) const {
                json response = sendRequest(GET, elementURL + "/attribute/" + name);
                return response["value"];
            }

            std::string getElementProperty(const std::string &name) const {
                json response = sendRequest(GET, elementURL + "/property/" + name);
                return response["value"];
            }

            std::string getElementCSSValue(const std::string &name) const {
                json response = sendRequest(GET, elementURL + "/css/" + name);
                return response["value"];
            }

            std::string getElementText() const {
                json response = sendRequest(GET, elementURL + "/text");
                return response["value"];
            }

            std::string getElementTagName() const {
                json response = sendRequest(GET, elementURL + "/name");
                return response["value"];
            }

            bool isEnabled() const {
                json response = sendRequest(GET, elementURL + "/enabled");
                return response["value"];
            }

            bool isSelected() const {
                json response = sendRequest(GET, elementURL + "/selected");
                return response["value"];
            }

            Element &save_screenshot(const std::string &ofile) {
                json response = sendRequest(GET, elementURL + "/screenshot");
                std::ofstream imageFS {ofile, std::ios::binary};
                std::string decoded {Base64::base64Decode(response["value"])};
                imageFS.write(decoded.data(), static_cast<long>(decoded.size()));
                return *this;
            }

            Element getActiveElement() const {
                json response = sendRequest(GET, elementURL + "/element/active");
                return Element(
                    response["value"].begin().key(), 
                    response["value"].begin().value(), 
                    sessionURL
                );
            }

            Element findElement(const LOCATION_STRATEGY &strategy, const std::string &criteria) const {
                std::string strategyKeyword;
                switch (strategy) {
                    case CSS: strategyKeyword = "css selector"; break;
                    case TAGNAME: strategyKeyword = "tag name"; break;
                    case XPATH: strategyKeyword = "xpath"; break;
                }
                json payload {{"using", strategyKeyword}, {"value", criteria}};
                json response = sendRequest(POST, elementURL + "/element", payload.dump());
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
                json response = sendRequest(POST, elementURL + "/elements", payload.dump());
                std::vector<Element> elements;
                std::transform(response["value"].begin(), response["value"].end(), std::back_inserter(elements), 
                    [&](const json::value_type &ele) {
                        return Element{ele.begin().key(), ele.begin().value(), sessionURL};
                    }
                );
                return elements;
            }

            Rect getElementRect() const {
                json response = sendRequest(GET, elementURL + "/rect");
                return Rect{response["value"]};
            }
    };
}
