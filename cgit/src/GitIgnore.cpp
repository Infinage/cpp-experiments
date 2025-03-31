#include "../include/GitIgnore.hpp"
#include "../../misc/fnmatch.hpp"

#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

GitIgnore::GitIgnore(
    const std::vector<std::pair<bool, std::string>> &absolute, 
    const std::unordered_map<std::string, std::vector<std::pair<bool, std::string>>> &scoped
): absolute(absolute), scoped(scoped) {}

bool GitIgnore::check(const std::string &path) const {
    fs::path curr {path};
    if (!curr.is_relative())
        throw std::runtime_error("Input path must be relative to the repo's root, got: " + curr.string());

    // Scoped rule check: Start from the immediate folder's 
    // .gitignore and move up. More specific rules take precedence 
    // Eg: `./abc/def/.gitignore` applies before `./abc/.gitignore`
    std::optional<bool> result;
    while (true) {
        fs::path parent {curr.parent_path()};
        auto it {scoped.find(parent.string())};
        if (it != scoped.end() && (result = checkIgnore(it->second, path)).has_value())
            return result.value();

        if (curr == "") break;
        else curr = parent;
    }

    // If no scoped rule matched, check global rules 
    // and default to false if unmatched.
    return checkIgnore(absolute, path).value_or(false);
}

std::optional<bool> GitIgnore::checkIgnore(const std::vector<BS_PAIR> &rules, const std::string &path) {
    std::optional<bool> result;
    std::string fileName {fs::path(path).filename()};
    for (const BS_PAIR &rule: rules)
        if (fnmatch::match(rule.second, path) || fnmatch::match(rule.second, fileName))
            result = rule.first;

    return result;
}
