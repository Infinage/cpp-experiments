#pragma once

#include "../../json-parser/json.hpp"

namespace Bencode {
    [[nodiscard]] std::string encode(JSON::JSONNodePtr root, bool sortKeys = false, bool _skipKey = true);
    [[nodiscard]] JSON::JSONHandle decode(const std::string &encoded, bool ignoreSpaces = true);
}
