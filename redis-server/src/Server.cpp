#include <algorithm>

#include "../include/Server.hpp"

namespace Redis {
    /* --------------- HELPER FUNCTIONS --------------- */

    void lower(std::string &inpStr) {
        std::transform(inpStr.begin(), inpStr.end(), inpStr.begin(), [](const char ch) { 
            return std::tolower(ch); 
        });
    }

    bool allDigitsUnsigned(const std::string::const_iterator& begin, const std::string::const_iterator& end) {
        return std::all_of(begin, end, [](const char ch){
            return std::isdigit(ch);
        });
    }

    bool allDigitsSigned(const std::string::const_iterator& begin, const std::string::const_iterator& end) {
        return begin != end && allDigitsUnsigned(begin + 1, end) && (*begin == '-' || std::isdigit(*begin));
    }

    std::size_t countSubstring(const std::string& str, const std::string& sub) {
        std::size_t count = 0;
        if (sub.size() != 0) {
            std::size_t offset = str.find(sub);
            while (offset != std::string::npos) {
                ++count;
                offset = str.find(sub, offset + sub.length());
            }
        }
        return count;
    }
}
