#include "../include/Cache.hpp"
#include <chrono>
#include <memory>

namespace Redis {
    unsigned long Cache::timeSinceEpoch() {
        std::chrono::duration tse {std::chrono::system_clock::now().time_since_epoch()};
        long tseMs {std::chrono::duration_cast<std::chrono::milliseconds>(tse).count()};
        return static_cast<unsigned long>(tseMs);
    }

    bool Cache::exists(const std::string &key) const {
        return cache.find(key) != cache.end();
    }

    bool Cache::expired(const std::string &key) const {
        return ttl.find(key) != ttl.end() && ttl.at(key) < timeSinceEpoch();
    }

    std::shared_ptr<RedisNode> Cache::getValue(const std::string &key) {
        // Delete key if expired
        if (expired(key)) { cache.erase(key); ttl.erase(key); }

        // Check key and return as usual
        if (!exists(key)) return std::make_shared<Redis::VariantRedisNode>(nullptr);
        else return cache.at(key);
    }

    void Cache::setValue(const std::string &key, const std::shared_ptr<RedisNode> &value) {
        cache[key] = value;
    }

    long Cache::getTTL(const std::string &key) const {
        if (!exists(key) || expired(key)) 
            return -2l;
        else if (ttl.find(key) == ttl.end()) 
            return -1l;
        else
            return static_cast<long>(ttl.at(key)) - static_cast<long>(timeSinceEpoch());
    }

    void Cache::setTTLS(const std::string &key, const unsigned long seconds) {
        ttl[key] = timeSinceEpoch() + (seconds * 1000);
    }

    void Cache::setTTLMS(const std::string &key, const unsigned long millis) {
        ttl[key] = timeSinceEpoch() + millis;
    }

    void Cache::setTTLSAt(const std::string &key, const unsigned long secondsAt) {
        ttl[key] = secondsAt * 1000;
    }

    void Cache::setTTLMSAt(const std::string &key, const unsigned long millisAt) {
        ttl[key] = millisAt;
    }

    void Cache::erase(const std::string &key) { 
        ttl.erase(key); cache.erase(key); 
    }

    std::size_t Cache::size() { 
        return cache.size(); 
    }
}
