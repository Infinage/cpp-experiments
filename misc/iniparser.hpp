#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace INI {
    template<typename T>
    class Iterator {
        private:
            std::unordered_map<std::string, T> data;
            std::unordered_map<std::string, std::size_t> ordering;
            std::size_t counter {0};

        public:
            template <typename U>
            class OrderedIterator {
                public:
                    using KV_PAIR = std::pair<const std::string, U&>;

                private:
                    std::vector<KV_PAIR>::const_iterator it{};
                    std::vector<KV_PAIR> sorted;

                public:
                    OrderedIterator() { it = sorted.end(); }

                    OrderedIterator(
                        std::unordered_map<std::string, T> &data,
                        std::unordered_map<std::string, std::size_t> &ordering
                    ) {
                        // Accumulate to a vector
                        std::vector<std::pair<std::size_t, std::string>> temp;
                        for (const auto &[key, pos]: ordering)
                            temp.emplace_back(pos, key);

                        // Sort the temp vector
                        std::sort(temp.begin(), temp.end(), [](const auto &p1, const auto &p2) { 
                            return p1.first < p2.first;
                        });

                        // insert into sorted
                        for (const auto &[pos, key]: temp)
                            sorted.emplace_back(key, data.at(key));

                        // Assign the iterator
                        it = sorted.begin();
                    }

                    const KV_PAIR &operator*() { return *it; }
                    OrderedIterator &operator++() { ++it; return *this; }
                    bool operator!=(const OrderedIterator &other) const {
                        return it != other.it;
                    }
            };

            OrderedIterator<T> begin() {
                return OrderedIterator<T>{data, ordering};
            }

            OrderedIterator<T> end() {
                return OrderedIterator<T>{};
            }

            OrderedIterator<const T> begin() const {
                return OrderedIterator<const T>{data, ordering};
            }

            OrderedIterator<const T> end() const {
                return OrderedIterator<const T>{};
            }
            
            // Non const access
            inline T &operator[] (const std::string &key) {
                if (!exists(key)) {
                    ordering[key] = counter++;
                    data.emplace(key, T{});
                }
                return data[key];
            }

            // Const access
            inline const T &operator[] (const std::string &key) const {
                if (!exists(key)) 
                    throw std::runtime_error("Key: `" + key + "` not found.");
                return data.at(key);
            }

            bool exists(const std::string &key) const noexcept {
                return data.find(key) != data.end();
            }

            bool remove(const std::string &key) {
                if (!exists(key)) return false;
                else {
                    data.erase(key);
                    ordering.erase(key);
                    return true;
                }
            }
    };

    class Section: public Iterator<std::string> {};

    class Parser: public Iterator<Section> {
        public:
            using Iterator::exists, Iterator::remove;

            bool exists(const std::string &sectionName, const std::string &key) const noexcept {
                return this->exists(sectionName) && this->operator[](sectionName).exists(key);
            }

            bool remove(const std::string &sectionName, const std::string &key) {
                if (!exists(sectionName, key)) return false;
                else {
                    this->operator[](sectionName).remove(key);
                    return true;
                }
            }

            void reads(const std::string &raw) {

            }

            std::string dumps() const {
                std::ostringstream oss; 
                return oss.str();
            }
    };
};
