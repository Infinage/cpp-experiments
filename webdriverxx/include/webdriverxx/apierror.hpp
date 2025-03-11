#pragma once

#include <exception>
#include <sstream>
#include <string>

namespace webdriverxx {
    struct APIError: public std::exception {
        const std::string url, requestBody;
        const std::string &method;
        const long statusCode; 
        const std::string responseBody;
        std::string errMsg;

        APIError(
            const std::string &url, const std::string &requestBody, const std::string &method, 
            const long &statusCode, const std::string &responseBody
        ):
            url(url), requestBody(requestBody), method(method), 
            statusCode(statusCode), responseBody(responseBody) 
        {
            std::ostringstream oss;
            oss << "\n\nMethod: " << method << "\n"
                << "URL: " << url << "\n"
                << "Request Body: " << requestBody << "\n"
                << "Status Code: " << statusCode << "\n"
                << "Response Body: " << responseBody << "\n";

            errMsg = oss.str();
        }

        const char* what() const noexcept override {
            return errMsg.c_str();
        }
    };
}
