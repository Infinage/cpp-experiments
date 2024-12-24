#pragma once

#include <fstream>
#include <memory>
#include <unordered_map>
#include <string>
#include <sys/poll.h>

#include "Node.hpp"

namespace Redis {

    // Cache functions
    class Cache {
        private:
            std::unordered_map<std::string, std::shared_ptr<RedisNode>> cache;
            std::unordered_map<std::string, unsigned long> ttl;
            void writeEncodedString(std::ofstream &ofs, const std::string &str);
            void readEncodedString(std::ifstream &ifs, std::string &placeholder);

        public:
            static unsigned long timeSinceEpoch();
            bool exists(const std::string &key) const;
            bool expired(const std::string &key) const;
            void erase(const std::string &key);
            std::shared_ptr<RedisNode> getValue(const std::string &key);
            void setValue(const std::string &key, const std::shared_ptr<RedisNode> &value);
            long     getTTL(const std::string &key) const;
            void    setTTLS(const std::string &key, const unsigned long   seconds);
            void   setTTLMS(const std::string &key, const unsigned long   millis);
            void  setTTLSAt(const std::string &key, const unsigned long secondsAt);
            void setTTLMSAt(const std::string &key, const unsigned long  millisAt);
            std::size_t size();
            bool save(const std::string &fname);
            bool load(const std::string &fname);
    };
}
