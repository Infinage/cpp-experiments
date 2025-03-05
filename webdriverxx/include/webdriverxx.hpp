#pragma once

#include "cpr/api.h"
#include "json.hpp"

#include <stdexcept>
#include <string>

using json = nlohmann::json;

namespace webdriverxx {
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
           cpr::Url("http://localhost:4444/session"), 
           cpr::Body{payload.dump()}, 
           cpr::Header{{"Content-Type", "application/json"}}
        )};

       if (response.status_code != 200)
           throw std::runtime_error(response.text);

       // Parse response json and unwrap if neccessary
       json responseJson {json::parse(response.text)};
       if (responseJson.is_array() && !responseJson.empty()) 
           responseJson = responseJson[0];

       return responseJson["value"]["sessionId"];
   }
}
