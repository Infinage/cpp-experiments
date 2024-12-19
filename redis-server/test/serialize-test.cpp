#include <iostream>
#include <vector>
#include "../Node.hpp"

int main() {
    // Sample Tests
    std::vector<std::tuple<std::string, std::string, std::string>> tests {
        {"NULLPTR", Redis::VariantRedisNode(nullptr).serialize(), "$-1\r\n"},

        {"['ping']", Redis::AggregateRedisNode({std::make_shared<Redis::VariantRedisNode>("ping")}).serialize(), "*1\r\n$4\r\nping\r\n"},

        {"['echo', 'hello world']", Redis::AggregateRedisNode({
            std::make_shared<Redis::VariantRedisNode>("echo"), 
            std::make_shared<Redis::VariantRedisNode>("hello world")
        }).serialize(), "*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n"},

        {"['get', 'key']", Redis::AggregateRedisNode({
            std::make_shared<Redis::VariantRedisNode>("get"), 
            std::make_shared<Redis::VariantRedisNode>("key")
        }).serialize(), "*2\r\n$3\r\nget\r\n$3\r\nkey\r\n"},

        {"'OK'", Redis::PlainRedisNode("OK").serialize(), "+OK\r\n"},

        {"Error message", Redis::PlainRedisNode("Error message", false).serialize(), "-Error message\r\n"},

        {"''", Redis::VariantRedisNode("").serialize(), "$0\r\n\r\n"},

        {"'hello world'", Redis::PlainRedisNode("hello world").serialize(), "+hello world\r\n"}
    };

    // Run the tests
    std::cout << "Serialization test...\n\n";
    for (std::size_t i {0}; i < tests.size(); i++) {
        bool status {std::get<1>(tests[i]) == std::get<2>(tests[i])};
        std::string message {std::to_string(i + 1) + ". " + (status? "PASS": "FAIL") + std::string{" -> "} + std::get<0>(tests[i]) + "\n"};
        if (!status) std::cout << "\033[31m" + message + "\033[0m";
        else std::cout << "\033[32m" + message + "\033[0m";
    }

    return 0;
}
