#pragma once

#include <algorithm>
#include <string>

namespace Redis {
    // Helper functions
    void lower(std::string &inpStr);
    std::size_t countSubstring(const std::string& str, const std::string& sub);

    template<typename Iterator>
    bool allDigitsUnsigned(const Iterator& begin, const Iterator& end) {
        return std::all_of(begin, end, [](const char &ch){
            return std::isdigit(ch);
        });
    }

    template<typename Iterator>
    bool allDigitsSigned(const Iterator& begin, const Iterator& end) {
        return begin != end && allDigitsUnsigned(begin + 1, end) && (*begin == '-' || std::isdigit(*begin));
    }

}
