#pragma once

#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace stdx {
    template <typename K, typename V, typename Hash = std::hash<K>>
    class ordered_map {
        public:
            using iterator = std::list<std::pair<K, V>>::iterator;
            using const_iterator = std::list<std::pair<K, V>>::const_iterator;

            // Defaults CTORS, Overloads
            ordered_map() = default;
            ordered_map(ordered_map&& other) = default;
            ordered_map &operator=(ordered_map&& other) = default;

            // Ctor with initializer support
            ordered_map(std::initializer_list<std::pair<K, V>> init) {
                for (const std::pair<K, V> &kv: init)
                    insert(kv.first, kv.second);
            }

            // Copy ctor must update refs
            ordered_map(const ordered_map &other): data(other.data) {
                for (iterator it {data.begin()}; it != data.end(); ++it)
                    lookup.emplace(it->first, it);
            }

            // Copy assign must update refs
            ordered_map &operator=(const ordered_map &other) {
                data = other.data;
                for (iterator it {data.begin()}; it != data.end(); ++it)
                    lookup.emplace(it->first, it);
                return *this; 
            }

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
            template<typename Val_> requires std::is_convertible_v<std::decay_t<Val_>, V>
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

            inline std::optional<V> extract(const K &key) {
                auto it {lookup.find(key)};
                if (it == lookup.end()) return std::nullopt;
                V res {it->second->second};
                lookup.erase(it); data.erase(it->second);
                return res;
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

        private:
            std::unordered_map<K, iterator, Hash> lookup;
            std::list<std::pair<K, V>> data;
    };
}
