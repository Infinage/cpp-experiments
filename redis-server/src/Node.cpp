#include <charconv>
#include <deque>
#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

#include "../include/Node.hpp"
#include "../include/Utils.hpp"

namespace Redis {

    /* --------------- CONSTANTS --------------- */

    const std::string SEP {"\r\n"};

    /* --------------- BASE REDIS NODE --------------- */

    RedisNode::~RedisNode() = default;
    RedisNode::RedisNode(const NODE_TYPE &t): type(t) {} 

    const NODE_TYPE &RedisNode::getType() const { 
        return type; 
    }

    std::unique_ptr<VariantRedisNode> RedisNode::deserializeBulkStr(const std::string& serialized, std::size_t &currPos) {
        // Assert that it is indeed a bulk string
        if (serialized[currPos] != '$')
            throw PlainRedisNode("Invalid input", false);

        // Figure out the length of bulk str and check if valid
        std::size_t lenTokEnd {serialized.find("\r\n", currPos)};
        if (lenTokEnd == std::string::npos)
            throw PlainRedisNode("Invalid input", false);

        // If neg length (only -1 is possible)
        if (serialized[currPos + 1] == '-') {
            currPos = serialized.size();
            return std::make_unique<VariantRedisNode>(nullptr);
        }

        // Parse the length using from_chars
        std::size_t strLength;
        std::from_chars_result parseResult {std::from_chars(serialized.c_str() + currPos + 1, serialized.c_str() + lenTokEnd, strLength)};
        if (parseResult.ec != std::errc())
            throw PlainRedisNode("Invalid input", false);

        // Extract the actual string along with serialized str
        std::string serializedSubstr{serialized.substr(currPos, strLength + (lenTokEnd - currPos) + 4)}, 
                    token{serialized.substr(lenTokEnd + 2, strLength)};

        currPos = lenTokEnd + 2 + strLength + 2;
        return std::make_unique<VariantRedisNode>(token, serializedSubstr);
    };

    std::unique_ptr<RedisNode> RedisNode::deserialize(const std::string &serialized) {
        // Dummy error node to return in case of error
        std::unique_ptr<RedisNode> errNode {std::make_unique<PlainRedisNode>("Invalid input", false)};

        // Simple data type parsing
        char ch {serialized[0]};
        if (ch == '+' || ch == '-' || ch == ':') {
            std::size_t tokEnd {serialized.find("\r\n")};
            if (tokEnd == std::string::npos) return errNode;
            else {
                std::string token {serialized.substr(1, tokEnd - 1)};
                switch (ch) {
                    case '+':
                        return std::make_unique<PlainRedisNode>(token);
                    case '-':
                        return std::make_unique<PlainRedisNode>(token, false);
                    default:
                        bool validInt {Redis::allDigitsSigned(token.begin(), token.end())};
                        return validInt? std::make_unique<VariantRedisNode>(std::stol(token.data())): std::move(errNode);
                }
            }
        } else if (serialized[0] == '$') {
            std::size_t currPos {0};
            std::unique_ptr<RedisNode> varNode {deserializeBulkStr(serialized, currPos)};
            return currPos == serialized.size()? std::move(varNode): std::move(errNode);
        } else if (serialized[0] == '*') {
            std::size_t lenTokEnd {serialized.find("\r\n")};
            if (lenTokEnd == std::string::npos)
                return std::make_unique<PlainRedisNode>("Invalid input", false);

            // If neg length (only -1 is possible)
            if (serialized[1] == '-') 
                return std::make_unique<VariantRedisNode>(nullptr);

            // Try parsing the length
            std::size_t arrLength, currPos {lenTokEnd + 2};
            std::from_chars_result parseResult {std::from_chars(serialized.c_str() + 1, serialized.c_str() + lenTokEnd, arrLength)};
            if (parseResult.ec != std::errc())
                return std::make_unique<PlainRedisNode>("Invalid input", false);

            // The length of aggregate node
            std::unique_ptr<AggregateRedisNode> aggNode {std::make_unique<AggregateRedisNode>()};
            for (std::size_t i{0}; i < arrLength; i++)
                aggNode->push_back(deserializeBulkStr(serialized, currPos));
            return currPos == serialized.size()? std::move(aggNode): std::move(errNode);
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

    VariantRedisNode::VariantRedisNode(const VARIANT_NODE_TYPE &value, const std::string &serialized):
        RedisNode(NODE_TYPE::VARIANT), value(value), serialized(serialized) {}

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
        if (serialized.empty()) {
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
            serialized = oss.str();
        }

        return serialized;
    }

    /* --------------- AGGREGATE REDIS NODE --------------- */

    AggregateRedisNode::AggregateRedisNode(std::deque<std::unique_ptr<VariantRedisNode>> &&values): 
        RedisNode(NODE_TYPE::AGGREGATE), values(std::move(values)) {}

    void AggregateRedisNode::push_back(std::unique_ptr<VariantRedisNode> &&node) { 
        values.push_back(std::move(node)); 
    }

    void AggregateRedisNode::push_front(std::unique_ptr<VariantRedisNode> &&node) { 
        values.push_front(std::move(node)); 
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

    VariantRedisNode &AggregateRedisNode::front() { 
        if (values.empty())
            throw PlainRedisNode("No element exists.", false);
        return *values.front();
    }
    
    VariantRedisNode &AggregateRedisNode::back() { 
        if (values.empty())
            throw PlainRedisNode("No element exists.", false);
        return *values.back(); 
    }

    const std::deque<std::unique_ptr<VariantRedisNode>> &AggregateRedisNode::getValues() const {
        return values;
    }

    const VariantRedisNode &AggregateRedisNode::operator[](long idx_) const {
        long N {static_cast<long>(values.size())};
        long idx {idx_ >= 0? idx_: N + idx_};
        if (idx < 0 || idx >= N)
            throw PlainRedisNode("Index out of bounds", false);
        else {
            return *values[static_cast<std::size_t>(idx)];
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
        for (const std::unique_ptr<VariantRedisNode> &value: values)
            serialized << value->serialize();
        return serialized.str();
    }
}
