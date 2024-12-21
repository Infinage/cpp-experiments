#include <deque>
#include <stack>
#include <vector>
#include "../include/Node.hpp"

namespace Redis {

    /* --------------- CONSTANTS --------------- */

    const std::string SEP {"\r\n"};

    /* --------------- BASE REDIS NODE --------------- */

    RedisNode::~RedisNode() = default;
    RedisNode::RedisNode(const NODE_TYPE &t): type(t) {} 

    NODE_TYPE RedisNode::getType() const { 
        return type; 
    }

    std::shared_ptr<RedisNode> RedisNode::deserialize(const std::string &serialized) {
        // Variables to process request 
        std::size_t currPos {0};
        std::stack<std::tuple<char, std::size_t, std::shared_ptr<Redis::RedisNode>>> stk;

        // Dummy Node to return on error, deleted when func goes out of scope
        std::shared_ptr<RedisNode> errorNode {std::make_shared<PlainRedisNode>("Invalid input", false)};

        while (currPos < serialized.size()) {
            // We check for curr token end usually by counting '\r\n' except
            // when we are parsing a bulk string, in which case we count chars
            if (stk.empty() || std::get<0>(stk.top()) != '$') {
                std::size_t tokEnd {serialized.find("\r\n", currPos)};
                if (tokEnd == std::string::npos) { return errorNode; } 
                else {
                    std::string token {serialized.substr(currPos, tokEnd - currPos)};

                    if (token.at(0) == '$' || token.at(0) == '*') {
                        std::size_t aggLength {token.at(1) == '-'? 0: std::stoull(token.substr(1))};
                        std::shared_ptr<Redis::RedisNode> aggNode;
                        if (token.at(1) == '-')
                            aggNode = std::make_shared<VariantRedisNode>(nullptr);
                        else if (token.at(0) == '$')
                            aggNode = std::make_shared<Redis::VariantRedisNode>("");
                        else 
                            aggNode = std::make_shared<Redis::AggregateRedisNode>();
                        stk.push({token.at(0), aggLength, aggNode});
                    } 

                    else if (token.at(0) == '+' || token.at(0) == '-') {
                        stk.push({token.at(0), 0, std::make_shared<Redis::PlainRedisNode>(token.substr(1), token.at(0) == '+')}); 
                    }

                    else { 
                        stk.push({':', 0, std::make_shared<Redis::VariantRedisNode>(std::stol(token.substr(1)))}); 
                    }

                    currPos = tokEnd + 2;
                }
            } 

            // We are currently parsing a bulk string
            else {
                 std::tuple<char, std::size_t, std::shared_ptr<Redis::RedisNode>> &top{stk.top()};
                 std::shared_ptr<Redis::VariantRedisNode> vnode {std::get<2>(top)->cast<VariantRedisNode>()};
                 std::size_t bulkStrAddLength {std::min(std::get<1>(top), serialized.size() - currPos)};
                 vnode->setValue(std::get<std::string>(vnode->getValue()) + serialized.substr(currPos, bulkStrAddLength));
                 stk.top() = {'$', std::get<1>(top) - bulkStrAddLength, vnode};
                 currPos += bulkStrAddLength;

                 // If we have parsed the bulk string
                 if (std::get<1>(stk.top()) == 0) currPos += 2;

                 // Else we need to read some more data
                 else return errorNode;
            }

            // Pop the top most if we can and add to prev guy
            while (!stk.empty() && std::get<1>(stk.top()) == 0) {
                 std::tuple<char, std::size_t, std::shared_ptr<Redis::RedisNode>> curr{stk.top()};
                 stk.pop();

                 if (stk.empty())
                     return std::get<2>(curr);
                 else {
                     std::shared_ptr<Redis::AggregateRedisNode> parent{std::get<2>(stk.top())->cast<Redis::AggregateRedisNode>()};
                     parent->push_back(std::get<2>(curr));
                     stk.top() = {'*', std::get<1>(stk.top()) - 1, parent};
                 }
            }
        }
        
        return errorNode;
    }

    /* --------------- PLAIN REDIS NODE --------------- */

    PlainRedisNode::PlainRedisNode(const std::string &message, bool notError): 
        RedisNode(NODE_TYPE::PLAIN), message(message), isNotError(notError) {}

    std::string PlainRedisNode::serialize() const {
        return (isNotError? "+": "-") + message + SEP;
    }

    std::string PlainRedisNode::getMessage() const { 
        return message; 
    }

    /* --------------- VARIANT REDIS NODE --------------- */

    VariantRedisNode::VariantRedisNode(const VARIANT_NODE_TYPE &value): 
        RedisNode(NODE_TYPE::VARIANT), value(value) {}

    VARIANT_NODE_TYPE VariantRedisNode::getValue() const { return value; }

    void VariantRedisNode::setValue(VARIANT_NODE_TYPE value) { 
        if (value.index() == this->value.index())
            this->value = value;
        else throw PlainRedisNode("Setting a different value type.", false);
    }

    std::string VariantRedisNode::str() const {
        if (std::holds_alternative<bool>(value)) {
            return std::get<bool>(value)? "true": "false";
        } else if (std::holds_alternative<double>(value)) {
            return std::to_string(std::get<double>(value));
        } else if (std::holds_alternative<long>(value)) {
            return std::to_string(std::get<long>(value));
        } else if (std::holds_alternative<std::string>(value)) {
            return std::get<std::string>(value);
        } else {
            return "nil";
        }
    }

    std::string VariantRedisNode::serialize() const {
        if (std::holds_alternative<bool>(value)) {
            return std::string("+", 1) + (std::get<bool>(value)? "true": "false") + SEP;
        } else if (std::holds_alternative<double>(value)) {
            return std::string("+", 1) + std::to_string(std::get<double>(value)) + SEP;
        } else if (std::holds_alternative<long>(value)) {
            return std::string(":", 1) + std::to_string(std::get<long>(value)) + SEP;
        } else if (std::holds_alternative<std::string>(value)) {
            std::string val {std::get<std::string>(value)};
            return std::string("$", 1) + std::to_string(val.size()) + SEP + val + SEP;
        } else {
            return "$-1" + SEP;
        }
    }

    /* --------------- AGGREGATE REDIS NODE --------------- */

    AggregateRedisNode::AggregateRedisNode(const std::deque<std::shared_ptr<RedisNode>> &values): 
        RedisNode(NODE_TYPE::AGGREGATE), values(values) {}

    void AggregateRedisNode::push_back(std::shared_ptr<RedisNode> node) { 
        values.push_back(node); 
    }

    void AggregateRedisNode::push_front(std::shared_ptr<RedisNode> node) { 
        values.push_front(node); 
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

    std::shared_ptr<RedisNode> AggregateRedisNode::front() { 
        return values.front(); 
    }
    
    std::shared_ptr<RedisNode> AggregateRedisNode::back() { 
        return values.back(); 
    }

    std::shared_ptr<RedisNode> AggregateRedisNode::operator[](long idx_) const {
        long N {static_cast<long>(values.size())};
        long idx {idx_ >= 0? idx_: N + idx_};
        if (idx < 0 || idx >= N)
            throw PlainRedisNode("Index out of bounds", false);
        else {
            return values[static_cast<std::size_t>(idx)];
        }
    }

    std::vector<std::string> AggregateRedisNode::vector() const {
        std::vector<std::string> result;
        for (std::size_t i{0}; i < size(); i++) {
            if (values[i]->getType() == NODE_TYPE::PLAIN)
                result.push_back(values[i]->cast<PlainRedisNode>()->getMessage());
            else if (values[i]->getType() == NODE_TYPE::VARIANT)
                result.push_back(values[i]->cast<VariantRedisNode>()->str());
            else
                throw PlainRedisNode("Node cannot contain aggregate nodes inside", false);
        }
        return result;
    }

    std::string AggregateRedisNode::serialize() const {
        std::string serialized {"*" + std::to_string(values.size()) + "\r\n"};
        for (const std::shared_ptr<RedisNode> &value: values)
            serialized += value->serialize();
        return serialized;
    }
}
