#pragma once

#include <memory>
namespace JSON { 
    struct JSONHandle; class JSONNode;
    using JSONNodePtr = std::shared_ptr<JSONNode>;
}

namespace Bencode {
    [[nodiscard]] std::string encode(JSON::JSONNodePtr root, bool sortKeys = false, bool _skipKey = true);
    [[nodiscard]] JSON::JSONHandle decode(const std::string &encoded, bool ignoreSpaces = true);
}
