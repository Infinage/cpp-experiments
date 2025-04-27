#pragma once

#include <list>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace stdx {
    template <typename K, typename V, typename Hash = std::hash<K>>
    class ordered_map {
        public:
            using iterator = std::list<std::pair<K, V>>::iterator;
            using const_iterator = std::list<std::pair<K, V>>::const_iterator;

        private:
            std::unordered_map<K, iterator, Hash> lookup;
            std::list<std::pair<K, V>> data;

        public:
            void insert(const K &key, const V &value) {
                iterator it {find(key)};
                if (it != end())
                    it->second = value;
                else {
                    data.emplace_back(key, value);
                    lookup.emplace(key, std::prev(end()));
                }
            }

            // Erase + emplace to support types without assignment operators (e.g. const members)
            template<typename Val_ = V> requires std::is_same_v<std::decay_t<Val_>, V>
            V& emplace(const K &key, Val_ &&value) {
                iterator it {find(key)};
                if (it != end()) erase(key);
                data.emplace_back(key, std::forward<Val_>(value));
                lookup.emplace(key, std::prev(end()));
                return data.back().second;
            }

            bool erase(const K &key) {
                iterator it {find(key)};
                if (it == end()) 
                    return false;
                else {
                    lookup.erase(key);
                    data.erase(it);
                    return true;
                }
            }

            // Moves the key to the end of list and returns the value
            V& touch(const K &key) {
                iterator it {find(key)};
                if (it == end())
                    throw std::runtime_error("ordered_map touch");
                data.emplace_back(key, std::move(it->second));
                data.erase(it);
                lookup[key] = std::prev(end());
                return data.back().second;
            }

            inline V &operator[] (const K &key) {
                iterator it {find(key)};
                if (it == end()) {
                    insert(key, V{});
                    it = find(key);
                }
                return it->second;
            }

            inline const V &at(const K &key) const {
                const_iterator it {find(key)};
                if (it == end())
                    throw std::runtime_error("ordered_map at");
                return it->second;
            }
            
            inline V &at(const K &key) {
                iterator it {find(key)};
                if (it == end())
                    throw std::runtime_error("ordered_map at");
                return it->second;
            }

            const_iterator find(const K &key) const {
                auto it {lookup.find(key)};
                return it == lookup.cend()? data.cend(): it->second;
            }

            iterator find(const K &key) {
                auto it {lookup.find(key)};
                return it == lookup.end()? data.end(): it->second;
            }
            
            bool empty() const { return data.empty(); }
            bool exists(const K &key) const { return find(key) != end(); }

            std::size_t size() const { return data.size(); }

            iterator begin() { return data.begin(); }
            iterator end() { return data.end(); }

            const_iterator begin() const { return data.cbegin(); }
            const_iterator end() const { return data.cend(); }

            const_iterator cbegin() const { return data.cbegin(); }
            const_iterator cend() const { return data.cend(); }
    };
}
