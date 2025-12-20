#include <iostream>
#include <memory>
#include <vector>

#include "../include/Node.hpp"

int main() {
    // Sample Tests for Serialization
    std::unique_ptr<Redis::VariantRedisNode> 
        n1 {std::make_unique<Redis::VariantRedisNode>("ping")},
        n2 {std::make_unique<Redis::VariantRedisNode>("echo")},
        n3 {std::make_unique<Redis::VariantRedisNode>("hello world")},
        n4 {std::make_unique<Redis::VariantRedisNode>("get")},
        n5 {std::make_unique<Redis::VariantRedisNode>("key")};

    Redis::AggregateRedisNode aggN1, aggN2, aggN3;
    aggN1.push_back(std::move(n1)); aggN2.push_back(std::move(n2)), aggN2.push_back(std::move(n3));
    aggN3.push_back(std::move(n4)); aggN3.push_back(std::move(n5));

    std::vector<std::tuple<std::string, std::string, std::string>> serializationTests {
        {"NULLPTR", Redis::VariantRedisNode(nullptr).serialize(), "$-1\r\n"},
        {"123413213", Redis::VariantRedisNode(123413213).serialize(), ":123413213\r\n"},
        {"'OK'", Redis::PlainRedisNode("OK").serialize(), "+OK\r\n"},
        {"Error message", Redis::PlainRedisNode("Error message", false).serialize(), "-Error message\r\n"},
        {"''", Redis::VariantRedisNode("").serialize(), "$0\r\n\r\n"},
        {"'hello world'", Redis::PlainRedisNode("hello world").serialize(), "+hello world\r\n"},
        {"['ping']", aggN1.serialize(), "*1\r\n$4\r\nping\r\n"},
        {"['echo', 'hello world']", aggN2.serialize(), "*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n"},
        {"['get', 'key']", aggN3.serialize(), "*2\r\n$3\r\nget\r\n$3\r\nkey\r\n"},
    };

    // Sample Tests for Deserialization
    std::vector<std::tuple<std::string, std::string>> deserializationTests {
        {"NULLPTR1", "$-1\r\n"},
        {"NULLPTR2", "*-1\r\n"},
        {"['ping']", "*1\r\n$4\r\nping\r\n"},
        {"['echo', 'hello world']", "*2\r\n$4\r\necho\r\n$11\r\nhello world\r\n"},
        {"['get', 'key']", "*2\r\n$3\r\nget\r\n$3\r\nkey\r\n"},
        {"123413213", ":123413213\r\n"},
        {"'OK'", "+OK\r\n"},
        {"Error message", "-Error message\r\n"},
        {"''", "$0\r\n\r\n"},
        {"'hello world'", "+hello world\r\n"}
    };

    // Run the Serialization tests
    std::cout << "Serialization test...\n\n";
    for (std::size_t i {0}; i < serializationTests.size(); i++) {
        bool status {std::get<1>(serializationTests[i]) == std::get<2>(serializationTests[i])};
        std::string message {
            std::to_string(i + 1) + ". " + (status? "PASS": "FAIL") 
            + std::string{" -> "} + std::get<0>(serializationTests[i]) + "\n"
        };
        if (!status) std::cout << "\033[31m" + message + "\033[0m";
        else std::cout << "\033[32m" + message + "\033[0m";
    }

    // Run the Deserialization tests
    std::cout << "\nDeserialization test...\n\n";
    for (std::size_t i {0}; i < deserializationTests.size(); i++) {
        std::string serialized {std::get<1>(deserializationTests[i])};
        std::string reserialized {Redis::RedisNode::deserialize(serialized)->serialize()};
        bool status {serialized == reserialized};
        std::string message {
            std::to_string(i + 1) + ". " + (status? "PASS": "FAIL") 
            + std::string{" -> "} + std::get<0>(deserializationTests[i]) + "\n"
        };
        if (!status) std::cout << "\033[31m" + message + "\033[0m";
        else std::cout << "\033[32m" + message + "\033[0m";
    }

    return 0;
}
