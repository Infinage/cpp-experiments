#include "../include/Cache.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>

int main() {
    Redis::Cache cache;  

    // Test TTLS
    std::cout << "Testing TTLS..\n";
    cache.setValue("abc", std::make_shared<Redis::VariantRedisNode>("123"));
    std::cout << "Should be present:       " << (!cache.expired("abc")? "PASS": "FAIL") << "\n";
    cache.setTTLS("abc", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(995));
    std::cout << "Should be still present: " << (!cache.expired("abc")? "PASS": "FAIL") << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "Should be Absent:        " << (cache.expired("abc")? "PASS": "FAIL") << "\n\n";

    // Erase to remove the TTLS
    cache.erase("abc");

    // Test TTLSAt
    std::cout << "Testing TTLSAt..\n";
    cache.setValue("abc", std::make_shared<Redis::VariantRedisNode>("123"));
    std::cout << "Should be present:       " << (!cache.expired("abc")? "PASS": "FAIL") << "\n";
    cache.setTTLSAt("abc", static_cast<unsigned long>(std::ceil((Redis::Cache::timeSinceEpoch() + 1000) / 1000)));
    std::this_thread::sleep_for(std::chrono::milliseconds(995));
    std::cout << "Should be still present: " << (!cache.expired("abc")? "PASS": "FAIL") << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "Should be Absent:        " << (cache.expired("abc")? "PASS": "FAIL") << "\n\n";

    // Erase to remove the TTLS
    cache.erase("abc");

    // Test TTLMS
    std::cout << "Testing TTLMS..\n";
    cache.setValue("abc", std::make_shared<Redis::VariantRedisNode>("123"));
    std::cout << "Should be present:       " << (!cache.expired("abc")? "PASS": "FAIL") << "\n";
    cache.setTTLMS("abc", 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(95));
    std::cout << "Should be still present: " << (!cache.expired("abc")? "PASS": "FAIL") << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "Should be Absent:        " << (cache.expired("abc")? "PASS": "FAIL") << "\n\n";

    // Erase to remove the TTLS
    cache.erase("abc");

    // Test TTLMSAt
    std::cout << "Testing TTLMSAt..\n";
    cache.setValue("abc", std::make_shared<Redis::VariantRedisNode>("123"));
    std::cout << "Should be present:       " << (!cache.expired("abc")? "PASS": "FAIL") << "\n";
    cache.setTTLMSAt("abc", Redis::Cache::timeSinceEpoch() + 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(95));
    std::cout << "Should be still present: " << (!cache.expired("abc")? "PASS": "FAIL") << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::cout << "Should be Absent:        " << (cache.expired("abc")? "PASS": "FAIL") << "\n\n";
}
