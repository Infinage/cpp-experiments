#pragma once

#include <memory>
#include <unordered_map>

#include "Node.hpp"

namespace Redis {
    class Cache {
        private:
            std::unordered_map<std::string, std::shared_ptr<RedisNode>> cache;

        public:
            bool exists(const std::string &key) const;
            std::shared_ptr<RedisNode>& operator[](const std::string &key);
            void erase(const std::string &key);
            std::size_t size();
    };
}
