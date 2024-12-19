#include <deque>
#include <memory>
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
}
