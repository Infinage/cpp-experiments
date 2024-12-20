#include "../include/Cache.hpp"

namespace Redis {
    bool Cache::exists(const std::string &key) const { 
        return cache.find(key) != cache.end(); 
    }

    std::shared_ptr<RedisNode>& Cache::operator[](const std::string &key) { 
        return cache[key]; 
    }

    void Cache::erase(const std::string &key) { 
        cache.erase(key); 
    }

    std::size_t Cache::size() { 
        return cache.size(); 
    }
}
