#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>

class fnmatch {
public:
    static bool match(const std::string& pattern, const std::string& str) {
        // Unbounded and can be problematic, fine for simple use cases; also not thread safe
        static std::unordered_map<std::string, std::regex> cache;

        auto it = cache.find(pattern);
        if (it == cache.end()) {
            std::regex regexPattern = compilePattern(pattern);
            cache.emplace(pattern, regexPattern);
            it = cache.find(pattern);
        }

        return std::regex_match(str, it->second);
    }

private:
    static std::regex compilePattern(std::string_view pattern) {
        std::ostringstream regex;
        regex << "^";

        for (std::size_t i {0}; i < pattern.size(); i++) {
            switch (pattern[i]) {
                case '*':
                    regex << ".*"; break;

                case '?':
                    regex << '.'; break;

                case '[': {
                    regex << '['; size_t j = i + 1; bool negate = false;
                    if (j < pattern.size() && pattern[j] == '!') {
                        negate = true;
                        j++;
                    }

                    if (negate) regex << '^';

                    while (j < pattern.size() && pattern[j] != ']') {
                        if (pattern[j] == '^' || pattern[j] == ']' || pattern[j] == '-' || pattern[j] == '\\')
                            regex << '\\';
                        regex << pattern[j];
                        j++;
                    }

                    if (j < pattern.size()) {
                        regex << ']';
                        i = j;
                    } else {
                        regex << "\\[";
                        if (negate) regex << '^';
                    }

                    break;
                }

                case '\\':
                    if (i + 1 < pattern.size()) {
                        regex << '\\' << pattern[++i];
                    } else {
                        regex << "\\\\";
                    }
                    break;

                case '.': case '+': case '(': case ')': case '{': case '}': case '|': case '^': case '$':
                    regex << '\\' << pattern[i]; break;

                default:
                    regex << pattern[i];
            }
        }

        regex << "$";
        return std::regex(regex.str(), std::regex::icase);
    }
};
