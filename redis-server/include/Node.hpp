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

    class RedisNode {
        protected:
            const NODE_TYPE type;

        public:
            virtual std::string serialize() const = 0;
            virtual ~RedisNode();
            RedisNode(const NODE_TYPE &t);
            const NODE_TYPE &getType() const;
            static std::unique_ptr<RedisNode> deserialize(const std::string &request);
            static std::unique_ptr<VariantRedisNode> deserializeBulkStr(const std::string& serialized, std::size_t &currPos);
    };

    class PlainRedisNode: public RedisNode {
        private:
            const std::string message;
            const bool isNotError;

        public:
            PlainRedisNode(const std::string &message, bool notError = true);
            const std::string &getMessage() const;
            std::string serialize() const override;
    };

    class VariantRedisNode: public RedisNode {
        private:
            VARIANT_NODE_TYPE value;
            mutable std::string serialized;

        public:
            VariantRedisNode(const VARIANT_NODE_TYPE &value);
            VariantRedisNode(const VARIANT_NODE_TYPE &value, const std::string &serialized);
            const VARIANT_NODE_TYPE &getValue() const;
            void setValue(const VARIANT_NODE_TYPE &value);
            std::string str() const;
            std::string serialize() const override;
    };

    class AggregateRedisNode: public RedisNode {
        private:
            std::deque<std::unique_ptr<VariantRedisNode>> values;

        public:
            AggregateRedisNode(std::deque<std::unique_ptr<VariantRedisNode>> &&values = {});

            void push_back(std::unique_ptr<VariantRedisNode> &&node);
            void push_front(std::unique_ptr<VariantRedisNode> &&node);

            std::size_t size() const;

            void pop_back();
            void pop_front();

            VariantRedisNode& front();
            VariantRedisNode& back();

            const VariantRedisNode &operator[](long idx_) const;
            const std::deque<std::unique_ptr<VariantRedisNode>> &getValues() const;

            std::vector<std::string> vector() const;
            std::string serialize() const override;
    };
}
