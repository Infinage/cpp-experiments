/*
 * Implementation of python's @functools.lru_cache
 *   - Arbitrary input
 *   - Single valued output
 *   - Support recursion
 *   - Strong type support
 *   - Bounded LRU cache
 */

#pragma once

#include <functional>
#include <iostream>
#include <ostream>
#include "ordered_map.hpp"

template<typename T>
concept Hashable = requires(T val) {
    { std::hash<T>{}(val) } -> std::same_as<std::size_t>;
};

template<typename Value, typename ...Keys> requires (!std::is_same_v<Value, void>)
class Cache {
    private:
        std::function<Value(Cache&, Keys...)> func;
        std::function<std::size_t(Keys...)> hashFunc;
        stdx::ordered_map<std::size_t, Value> cache;
        std::size_t hits {0}, miss {0}, capacity {128};

        // Helper to hash trivial types
        static std::size_t DefaultHash(Keys &&...keys) {
            std::size_t seed = 0;
            (..., (seed ^= std::hash<Keys>{}(keys) + 0x9e3779b9 + (seed << 6) + (seed >> 2)));
            return seed;
        }

        // Pop front until size is within capacity
        void ensureCapacity() {
            while (cache.size() > capacity)
                cache.erase(cache.begin()->first);
        }

    public:
        struct Stat {
            std::size_t hits, miss, size, capacity;
            inline friend std::ostream& operator<<(std::ostream &os, const Stat &stat) {
                os << "Stat(" 
                   << "hits="       << stat.hits 
                   << ", misses="   << stat.miss 
                   << ", currsize=" << stat.size 
                   << ", maxsize="  << stat.capacity
                   << ")";

                return os;
            }
        };

        Cache(
            const std::function<Value(Cache&, Keys...)> &func, 
            const std::function<std::size_t(Keys...)> &hashFunc
        ): func(func), hashFunc(hashFunc) {}

        template<typename = void> requires(Hashable<Keys> && ...)
        Cache(
            const std::function<Value(Cache&, Keys...)> &func, 
            const std::function<std::size_t(Keys...)> &hashFunc = DefaultHash
        ): func(func), hashFunc(hashFunc) {}

        template<typename ...Args>
        [[nodiscard]] inline Value operator()(Args &&...keys) {
            std::size_t key {hashFunc(std::forward<Args>(keys)...)};
            if (cache.find(key) == cache.end()) {
                ++miss;
                Value &inserted {cache.emplace(key, func(*this, std::forward<Args>(keys)...))};
                ensureCapacity();
                return inserted;
            }

            else {
                ++hits;
                return cache.touch(key);
            }
        }

        [[nodiscard]] Stat stat() const {
            return {hits, miss, cache.size(), capacity};
        }

        void resize(std::size_t capacity) {
            if (capacity == 0) 
                throw std::runtime_error("Capacity must be greater than 0");
            this->capacity = capacity;
            ensureCapacity();
        }
};
