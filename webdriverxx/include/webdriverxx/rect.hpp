#pragma once

#include <optional>

#include "json.hpp"

using json = nlohmann::json;

namespace webdriverxx {
    struct Rect {
        std::optional<int> x, y, width, height; 

        Rect(const json &json_) {
            if (json_.contains("x")) x = json_.at("x");
            if (json_.contains("y")) y = json_.at("y");
            if (json_.contains( "width"))  width = json_.at( "width");
            if (json_.contains("height")) height = json_.at("height");
        }

        operator json() const {
            json object = json::object();
            if (x) object["x"] = *x;
            if (y) object["y"] = *y;
            if (width)  object[ "width"] = *width;
            if (height) object["height"] = *height;
            return object;
        }
    };
}
