#include <charconv>
#include <deque>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "../include/Server.hpp"
#include "../include/Node.hpp"

namespace Redis {

    /* --------------- CONSTANTS --------------- */

    const std::string SEP {"\r\n"};

    /* --------------- BASE REDIS NODE --------------- */

    RedisNode::~RedisNode() = default;
    RedisNode::RedisNode(const NODE_TYPE &t): type(t) {} 

    const NODE_TYPE &RedisNode::getType() const { 
        return type; 
    }

    std::shared_ptr<VariantRedisNode> RedisNode::deserializeBulkStr(const std::string& serialized, std::size_t &currPos) {
        // Assert that it is indeed a bulk string
        if (serialized[currPos] != '$')
            throw std::make_shared<PlainRedisNode>("Invalid input", false);

        // Figure out the length of bulk str and check if valid
        std::size_t lenTokEnd {serialized.find("\r\n", currPos)};
        if (lenTokEnd == std::string::npos)
            throw std::make_shared<PlainRedisNode>("Invalid input", false);

        // If neg length (only -1 is possible)
        if (serialized[currPos + 1] == '-') 
            return std::make_shared<VariantRedisNode>(nullptr);

        // Parse the length using from_chars
        std::size_t strLength;
        std::from_chars_result parseResult {std::from_chars(serialized.c_str() + currPos + 1, serialized.c_str() + lenTokEnd, strLength)};
        if (parseResult.ec != std::errc())
            throw std::make_shared<PlainRedisNode>("Invalid input", false);

        currPos = lenTokEnd + 2 + strLength + 2;
        return std::make_shared<VariantRedisNode>(serialized.substr(lenTokEnd + 2, strLength));
    };

    std::shared_ptr<RedisNode> RedisNode::deserialize(const std::string &serialized) {
        // Dummy error node to return in case of error
        std::shared_ptr<RedisNode> errNode {std::make_shared<PlainRedisNode>("Invalid input", false)};

        // Simple data type parsing
        char ch {serialized[0]};
        if (ch == '+' || ch == '-' || ch == ':') {
            std::size_t tokEnd {serialized.find("\r\n")};
            if (tokEnd == std::string::npos) return errNode;
            else {
                std::string_view token {std::string_view(serialized.data() + 1, tokEnd - 1)};
                switch (ch) {
                    case '+':
                        return std::make_shared<PlainRedisNode>(token.data());
                    case '-':
                        return std::make_shared<PlainRedisNode>(token.data(), false);
                    default:
                        bool validInt {Redis::allDigitsSigned(token.begin(), token.end())};
                        return validInt? std::make_shared<VariantRedisNode>(std::stol(token.data())): errNode;
                }
            }
        } else if (serialized[0] == '$') {
            std::size_t currPos {0};
            std::shared_ptr<VariantRedisNode> varNode {deserializeBulkStr(serialized, currPos)};
            return currPos == serialized.size()? varNode: errNode;
        } else if (serialized[0] == '*') {
            std::size_t lenTokEnd {serialized.find("\r\n")};
            if (lenTokEnd == std::string::npos || !Redis::allDigitsSigned(serialized.begin() + 1, serialized.begin() + static_cast<long long>(lenTokEnd))) 
                return std::make_shared<PlainRedisNode>("Invalid input", false);

            // If neg length (only -1 is possible)
            if (serialized[1] == '-') 
                return std::make_shared<VariantRedisNode>(nullptr);

            // The length of aggregate node
            std::size_t arrLength {std::stoull(serialized.substr(1, lenTokEnd))}, currPos {lenTokEnd + 2};
            std::shared_ptr<AggregateRedisNode> aggNode {std::make_shared<AggregateRedisNode>()};
            for (std::size_t i{0}; i < arrLength; i++)
                aggNode->push_back(deserializeBulkStr(serialized, currPos));
            return currPos == serialized.size()? aggNode: errNode;
        } else {
            return errNode;
        }
    }

    /* --------------- PLAIN REDIS NODE --------------- */

    PlainRedisNode::PlainRedisNode(const std::string &message, bool notError): 
        RedisNode(NODE_TYPE::PLAIN), message(message), isNotError(notError) {}

    std::string PlainRedisNode::serialize() const {
        return (isNotError? "+": "-") + message + SEP;
    }

    const std::string &PlainRedisNode::getMessage() const { 
        return message; 
    }

    /* --------------- VARIANT REDIS NODE --------------- */

    VariantRedisNode::VariantRedisNode(const VARIANT_NODE_TYPE &value): 
        RedisNode(NODE_TYPE::VARIANT), value(value) {}

    const VARIANT_NODE_TYPE &VariantRedisNode::getValue() const { return value; }

    void VariantRedisNode::setValue(const VARIANT_NODE_TYPE &value) { 
        if (value.index() == this->value.index())
            this->value = value;
        else throw PlainRedisNode("Setting a different value type.", false);
    }

    std::string VariantRedisNode::str() const {
        if (std::holds_alternative<long>(value)) {
            return std::to_string(std::get<long>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        } else {
            return "nil";
        }
    }

    std::string VariantRedisNode::serialize() const {
        std::ostringstream oss;
        if (std::holds_alternative<long>(value)) {
            oss << ":" << std::get<long>(value);
        } else if (std::holds_alternative<std::string>(value)) {
            const std::string &val {std::get<std::string>(value)};
            oss << "$" << val.size() << SEP << val;
        } else {
            oss << "$-1";
        }

        oss << SEP;
        return oss.str();
    }

    /* --------------- AGGREGATE REDIS NODE --------------- */

    AggregateRedisNode::AggregateRedisNode(const std::deque<std::shared_ptr<VariantRedisNode>> &values): 
        RedisNode(NODE_TYPE::AGGREGATE), values(values) {}

    void AggregateRedisNode::push_back(const std::shared_ptr<VariantRedisNode> &node) { 
        values.emplace_back(node); 
    }

    void AggregateRedisNode::push_front(const std::shared_ptr<VariantRedisNode> &node) { 
        values.emplace_front(node); 
    }

    std::size_t AggregateRedisNode::size() const { 
        return values.size(); 
    }

    void AggregateRedisNode::pop_back() { 
        values.pop_back(); 
    }

    void AggregateRedisNode::pop_front() { 
        values.pop_front(); 
    }

    std::shared_ptr<VariantRedisNode> AggregateRedisNode::front() { 
        return values.front(); 
    }
    
    std::shared_ptr<VariantRedisNode> AggregateRedisNode::back() { 
        return values.back(); 
    }

    const std::shared_ptr<VariantRedisNode> &AggregateRedisNode::operator[](long idx_) const {
        long N {static_cast<long>(values.size())};
        long idx {idx_ >= 0? idx_: N + idx_};
        if (idx < 0 || idx >= N)
            throw PlainRedisNode("Index out of bounds", false);
        else {
            return values[static_cast<std::size_t>(idx)];
        }
    }

    std::vector<std::string> AggregateRedisNode::vector() const {
        std::vector<std::string> result(size());
        for (std::size_t i{0}; i < size(); i++)
            result[i] = values[i]->str();
        return result;
    }

    std::string AggregateRedisNode::serialize() const {
        std::ostringstream serialized; 
        serialized << "*" << values.size() << "\r\n";
        for (const std::shared_ptr<VariantRedisNode> &value: values)
            serialized << value->serialize();
        return serialized.str();
    }
}
