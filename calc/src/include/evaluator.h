#pragma once

#include "token.h"

#include <expected>
#include <span>
#include <string>

namespace Calc {
    class Evaluator {
        public:
            std::expected<double, std::string> eval(std::span<Token> postfix) const;    
    };
}
