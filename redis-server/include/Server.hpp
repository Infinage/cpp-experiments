#pragma once

#include <string>

namespace Redis {
    // Helper functions
    void lower(std::string &inpStr);
    bool allDigitsUnsigned(const std::string::const_iterator& begin, const std::string::const_iterator& end);
    bool allDigitsSigned(const std::string::const_iterator& begin, const std::string::const_iterator& end);
    std::size_t countSubstring(const std::string& str, const std::string& sub);
}
