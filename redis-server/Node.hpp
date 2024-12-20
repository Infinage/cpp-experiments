#include <deque>
#include <memory>
#include <stack>
#include <string>
#include <variant>

namespace Redis {

    enum NODE_TYPE: short {PLAIN, VARIANT, AGGREGATE};
    using VARIANT_NODE_TYPE = std::variant<bool, double, long, std::string, std::nullptr_t>;
    const std::string SEP {"\r\n"};

    class RedisNode {
        protected:
            const NODE_TYPE type;

        public:
            virtual std::string serialize() const = 0;
            virtual ~RedisNode() = default;
            RedisNode(const NODE_TYPE &t): type(t) {} 
            NODE_TYPE getType() { return type; }
            static std::shared_ptr<RedisNode> deserialize(const std::string &request);
    };

    class PlainRedisNode: public RedisNode {
        private:
            const std::string message;
            const bool isNotError;

        public:
            PlainRedisNode(const std::string &message, bool notError = true): 
                RedisNode(NODE_TYPE::PLAIN), message(message), isNotError(notError) {}

            std::string getMessage() const { return message; }
            std::string serialize() const override {
                return (isNotError? "+": "-") + message + SEP;
            }
    };

    class VariantRedisNode: public RedisNode {
        private:
            VARIANT_NODE_TYPE value;

        public:
            VariantRedisNode(const VARIANT_NODE_TYPE &value): RedisNode(NODE_TYPE::VARIANT), value(value) {}
            VARIANT_NODE_TYPE getValue() const { return value; }
            void setValue(VARIANT_NODE_TYPE value) { 
                if (value.index() == this->value.index())
                    this->value = value;
                else throw PlainRedisNode("Setting a different value type.", false);
            }
            std::string serialize() const override {
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
    };

    class AggregateRedisNode: public RedisNode {
        private:
            std::deque<std::shared_ptr<RedisNode>> values;

        public:
            AggregateRedisNode(const std::deque<std::shared_ptr<RedisNode>> &values = {}): RedisNode(NODE_TYPE::AGGREGATE), values(values) {}
            void push_back(std::shared_ptr<RedisNode> node) { values.push_back(node); }
            std::shared_ptr<RedisNode> operator[](long idx_) const {
                std::size_t idx {idx_ >= 0? static_cast<std::size_t>(idx_): static_cast<std::size_t>(-idx_) - 1};
                if (idx >= values.size()) 
                    throw PlainRedisNode("Index out of bounds", false);
                else 
                    return values[idx];
            }

            std::string serialize() const override {
                std::string serialized {"*" + std::to_string(values.size()) + "\r\n"};
                for (const std::shared_ptr<RedisNode> &value: values)
                    serialized += value->serialize();
                return serialized;
            }
    };

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
                 std::shared_ptr<Redis::VariantRedisNode> vnode {std::static_pointer_cast<Redis::VariantRedisNode>(std::get<2>(top))};
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
                     std::shared_ptr<Redis::AggregateRedisNode> parent{std::static_pointer_cast<Redis::AggregateRedisNode>(std::get<2>(stk.top()))};
                     parent->push_back(std::get<2>(curr));
                     stk.top() = {'*', std::get<1>(stk.top()) - 1, parent};
                 }
            }
        }
        
        return errorNode;
    }
}
