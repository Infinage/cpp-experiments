#include "../include/Cache.hpp"

#include <chrono>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <utility>

namespace Redis {

    /* --------------- CACHE CLASS METHODS --------------- */

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

    void Cache::writeEncodedString(std::ofstream &ofs, const std::string &str) {
        std::size_t length {str.size()};
        ofs.write(reinterpret_cast<char *>(&length), sizeof(length));
        ofs.write(str.data(), static_cast<std::streamsize>(length));
    }

    void Cache::readEncodedString(std::ifstream &ifs, std::string &placeholder) {
        std::size_t strLength;
        ifs.read(reinterpret_cast<char *>(&strLength), sizeof (std::size_t));
        placeholder = std::string(strLength, '\0');
        ifs.read(placeholder.data(), static_cast<std::streamsize>(strLength));
    }

    bool Cache::save(const std::string &fname) {
        // Remove the expired keys - SNAPSHOT TIME
        unsigned long TS {timeSinceEpoch()};
        for (std::unordered_map<const std::string, std::shared_ptr<RedisNode>>::iterator it {cache.begin()}; it != cache.end();) {
            if (ttl.find(it->first) != ttl.end() && ttl[it->first] < TS) {
                ttl.erase(it->first);
                it = cache.erase(it);
            } else it = std::next(it);
        }

        // Open output file stream as binary
        std::ofstream ofs {fname, std::ios::binary};
        if (!ofs) return false;
        else {
            // Write the header
            ofs.write("REDIS0003", 9);

            // Auxilary field(s) - str, str
            ofs.put(0xFA);
            writeEncodedString(ofs, "Timestamp(ms)");
            writeEncodedString(ofs, std::to_string(TS));

            // Database selector - byte, byte
            ofs.put(0xFE);
            ofs.put(0);

            // Length encoded hashtable sizes - byte, size_t, size_t
            ofs.put(0xFB);
            std::size_t cacheSize{cache.size()}, ttlSize{ttl.size()};
            ofs.write(reinterpret_cast<char *>(&cacheSize), sizeof (std::size_t));
            ofs.write(reinterpret_cast<char *>(&ttlSize), sizeof (std::size_t));

            // Write key-value pairs
            for (const std::pair<const std::string, std::shared_ptr<RedisNode>> &kv: cache) {
                bool ttlExists {ttl.find(kv.first) != ttl.end()};
                // Add TTL
                if (ttlExists) {
                    ofs.put(0xFC);
                    ofs.write(reinterpret_cast<char *>(&ttl[kv.first]), sizeof (unsigned long));
                }

                // Value-Type, Key, Value
                const std::shared_ptr<Redis::RedisNode> &node {kv.second};
                ofs.put(node->getType() == Redis::NODE_TYPE::PLAIN? 'P': node->getType() == Redis::NODE_TYPE::VARIANT? 'V': 'A');
                writeEncodedString(ofs, kv.first);
                writeEncodedString(ofs, node->serialize());
            }

            // End of file
            ofs.put(0xFF);
            ofs.flush();
            return ofs.good();
        }
    }

    bool Cache::load(const std::string &fname) {
        std::ifstream ifs {fname, std::ios::binary};
        std::string corruptedSaveMsg {"Save file is corrupted.\n"};
        if (!ifs) return false;            
        else {
            // Verify header
            char header[9], nextByte;
            ifs.read(header, 9);            
            if (std::strcmp(header, "REDIS0003") != 0) {
                std::cerr << corruptedSaveMsg;
                return false;
            }

            // Read optional metadata
            ifs.get(nextByte);
            while (nextByte == static_cast<char>(0xFA)) {
                std::string key, value;
                readEncodedString(ifs, key);
                readEncodedString(ifs, value);
                ifs.get(nextByte);
                std::cout << key << ": " << value << "\n";
            }

            // Read database selector
            if (nextByte != static_cast<char>(0xFE)) {
                std::cerr << corruptedSaveMsg;
                return false;
            } else {
                ifs.get(nextByte);
            }

            // Read Cache size, TTL size
            std::size_t cacheSize{}, ttlSize{};
            ifs.get(nextByte);
            if (nextByte != static_cast<char>(0xFB)) {
                std::cerr << corruptedSaveMsg;
                return false;
            } else {
                ifs.read(reinterpret_cast<char *>(&cacheSize), sizeof (std::size_t));
                ifs.read(reinterpret_cast<char *>(&ttlSize), sizeof (std::size_t));
            }

            // Read the key value pairs
            for (std::size_t i{0}; i < cacheSize; i++) {
                // Does the key have an associated ttl?
                ifs.get(nextByte);
                unsigned long ttlVal {0};
                bool ttlExists {nextByte == static_cast<char>(0xFC)};
                if (ttlExists) {
                    ifs.read(reinterpret_cast<char *>(&ttlVal), sizeof(unsigned long));
                    ifs.get(nextByte);
                }

                // Read the key, value
                std::string key, value;
                readEncodedString(ifs, key);
                readEncodedString(ifs, value);

                // Set the value after converting into a RedisNode
                cache[key] = Redis::RedisNode::deserialize(value);
                if (ttlExists) ttl[key] = ttlVal;
            }

            // Read the last FF byte
            ifs.get(nextByte);
            if (nextByte != static_cast<char>(0xFF)) {
                std::cerr << corruptedSaveMsg;
                return false;
            }

            std::cout << "Cache Size: " << cacheSize << "; TTL Size: " << ttlSize << "\n";
            return true;
        }
    }
}
