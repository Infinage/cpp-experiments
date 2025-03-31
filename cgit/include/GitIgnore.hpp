#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// Contains combined rules of all gitignore files encountered
class GitIgnore {
    public:
        using BS_PAIR = std::pair<bool, std::string>;

        GitIgnore(
            const std::vector<std::pair<bool, std::string>> &absolute = {}, 
            const std::unordered_map<std::string, std::vector<std::pair<bool, std::string>>> &scoped = {}
        );

        // Check if a file matches any of the .gitignore rules
        // Return true if the file should be ignored, false otherwise.
        bool check(const std::string &path) const;

    private:
        std::vector<std::pair<bool, std::string>> absolute;
        std::unordered_map<std::string, std::vector<std::pair<bool, std::string>>> scoped;

        // Checks if a file matches any of the given .gitignore rules.
        // The last matching rule takes precedence, as later rules can override earlier ones.
        // Each rule specifies whether to ignore (true) or include (false) a matching file.
        // Rules is a list of patterns and their corresponding inclusion/exclusion flags.
        static std::optional<bool> checkIgnore(const std::vector<BS_PAIR> &rules, const std::string &path);
};

