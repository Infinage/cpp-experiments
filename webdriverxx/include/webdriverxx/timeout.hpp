#pragma once

#include <optional>

namespace webdriverxx {
    struct Timeout { 
        std::optional<unsigned int> script = std::nullopt; 
        std::optional<unsigned int> pageLoad = std::nullopt; 
        std::optional<unsigned int> implicit = std::nullopt; 
    };
}
