#pragma once

#include "json.hpp"

#include <optional>
#include <string>
#include <stdexcept>

using json = nlohmann::json;

namespace webdriverxx {
    struct Cookie {
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
}
