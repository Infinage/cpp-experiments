#pragma once

#include <deque>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace Redis {

    using VARIANT_NODE_TYPE = std::variant<long, std::string, std::nullptr_t>;
    enum NODE_TYPE: short {PLAIN, VARIANT, AGGREGATE};
    extern const std::string SEP;

    // Forward decl
    class VariantRedisNode;

    class RedisNode: public std::enable_shared_from_this<RedisNode> {
        protected:
            const NODE_TYPE type;

        public:
            virtual std::string serialize() const = 0;
            virtual ~RedisNode();
            RedisNode(const NODE_TYPE &t);
            NODE_TYPE getType() const;
            static std::shared_ptr<RedisNode> deserialize(const std::string &request);
            static std::shared_ptr<VariantRedisNode> deserializeBulkStr(const std::string& serialized, std::size_t &currPos);
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
            void setValue(const VARIANT_NODE_TYPE &value);
            std::string str() const;
            std::string serialize() const override;
    };

    class AggregateRedisNode: public RedisNode {
        private:
            std::deque<std::shared_ptr<VariantRedisNode>> values;

        public:
            AggregateRedisNode(const std::deque<std::shared_ptr<VariantRedisNode>> &values = {});

            void push_back(const std::shared_ptr<VariantRedisNode> &node);
            void push_front(const std::shared_ptr<VariantRedisNode> &node);

            std::size_t size() const;

            void pop_back();
            void pop_front();

            std::shared_ptr<VariantRedisNode> front();
            std::shared_ptr<VariantRedisNode> back();

            std::shared_ptr<VariantRedisNode> operator[](long idx_) const;

            std::vector<std::string> vector() const;
            std::string serialize() const override;
    };

    template<typename T> 
    std::shared_ptr<T> RedisNode::cast() {
        return std::static_pointer_cast<T>(shared_from_this());
    }
}
