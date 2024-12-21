#pragma once

#include <deque>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Redis {

    using VARIANT_NODE_TYPE = std::variant<bool, double, long, std::string, std::nullptr_t>;
    enum NODE_TYPE: short {PLAIN, VARIANT, AGGREGATE};
    extern const std::string SEP;

    class RedisNode: public std::enable_shared_from_this<RedisNode> {
        protected:
            const NODE_TYPE type;

        public:
            virtual std::string serialize() const = 0;
            virtual ~RedisNode();
            RedisNode(const NODE_TYPE &t);
            NODE_TYPE getType() const;
            static std::shared_ptr<RedisNode> deserialize(const std::string &request);
            template<typename T> std::shared_ptr<T> cast();
    };

    class PlainRedisNode: public RedisNode {
        private:
            const std::string message;
            const bool isNotError;

        public:
            PlainRedisNode(const std::string &message, bool notError = true);
            std::string getMessage() const;
            std::string serialize() const override;
    };

    class VariantRedisNode: public RedisNode {
        private:
            VARIANT_NODE_TYPE value;

        public:
            VariantRedisNode(const VARIANT_NODE_TYPE &value);
            VARIANT_NODE_TYPE getValue() const;
            void setValue(VARIANT_NODE_TYPE value);
            std::string str() const;
            std::string serialize() const override;
    };

    class AggregateRedisNode: public RedisNode {
        private:
            std::deque<std::shared_ptr<RedisNode>> values;

        public:
            AggregateRedisNode(const std::deque<std::shared_ptr<RedisNode>> &values = {});

            void push_back(std::shared_ptr<RedisNode> node);
            void push_front(std::shared_ptr<RedisNode> node);

            std::size_t size() const;

            void pop_back();
            void pop_front();

            std::shared_ptr<RedisNode> front();
            std::shared_ptr<RedisNode> back();

            std::shared_ptr<RedisNode> operator[](long idx_) const;

            std::vector<std::string> vector() const;
            std::string serialize() const override;
    };

    template<typename T> 
    std::shared_ptr<T> RedisNode::cast() {
        return std::static_pointer_cast<T>(shared_from_this());
    }
}
