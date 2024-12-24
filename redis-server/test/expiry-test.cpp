#include "../include/Cache.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>

const std::string GREEN{"\033[32m"};
const std::string RED{"\033[31m"};
const std::string RESET{"\033[0m"};

void printResult(bool condition, const std::string& message) {
    std::cout << message << (condition ? GREEN + "PASS" + RESET : RED + "FAIL" + RESET) << "\n";
}

int main() {
    Redis::Cache cache;  

    // Test TTLS
    std::cout << "Testing TTLS..\n";
    cache.setValue("abc", std::make_shared<Redis::VariantRedisNode>("123"));
    printResult(!cache.expired("abc"), "Should be present:       ");
    cache.setTTLS("abc", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(995));
    printResult(!cache.expired("abc"), "Should be still present: ");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    printResult(cache.expired("abc"), "Should be Absent:        ");

    // Erase to remove the TTLS
    cache.erase("abc");

    // Test TTLSAt
    std::cout << "Testing TTLSAt..\n";
    cache.setValue("abc", std::make_shared<Redis::VariantRedisNode>("123"));
    printResult(!cache.expired("abc"), "Should be present:       ");
    cache.setTTLSAt("abc", static_cast<unsigned long>(std::ceil((Redis::Cache::timeSinceEpoch() + 1000) / 1000)));
    std::this_thread::sleep_for(std::chrono::milliseconds(995));
    printResult(!cache.expired("abc"), "Should be still present: ");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    printResult(cache.expired("abc"), "Should be Absent:        ");

    // Erase to remove the TTLS
    cache.erase("abc");

    // Test TTLMS
    std::cout << "Testing TTLMS..\n";
    cache.setValue("abc", std::make_shared<Redis::VariantRedisNode>("123"));
    printResult(!cache.expired("abc"), "Should be present:       ");
    cache.setTTLMS("abc", 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(95));
    printResult(!cache.expired("abc"), "Should be still present: ");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    printResult(cache.expired("abc"), "Should be Absent:        ");

    // Erase to remove the TTLS
    cache.erase("abc");

    // Test TTLMSAt
    std::cout << "Testing TTLMSAt..\n";
    cache.setValue("abc", std::make_shared<Redis::VariantRedisNode>("123"));
    printResult(!cache.expired("abc"), "Should be present:       ");
    cache.setTTLMSAt("abc", Redis::Cache::timeSinceEpoch() + 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(95));
    printResult(!cache.expired("abc"), "Should be still present: ");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    printResult(cache.expired("abc"), "Should be Absent:        ");
}
