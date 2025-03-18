#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// TODO: Handle multi line outputs
// TODO: Impl the reads function

namespace INI {
    template<typename T>
    class Iterator {
        protected:
            // Data containers
            std::unordered_map<std::string, T> data;
            std::unordered_map<std::string, std::size_t> ordering;
            std::size_t counter {0};

            // For iterators
            mutable bool dirty = true;
            mutable std::vector<std::pair<std::string, T&>> sorted;
            mutable std::vector<std::pair<std::string, const T&>> constSorted;

            void buildIterator() const {
                if (dirty) {
                    // Accumulate to a vector
                    std::vector<std::pair<std::size_t, std::string>> temp;
                    for (const auto &[key, pos]: ordering)
                        temp.emplace_back(pos, key);

                    // Sort the temp vector
                    std::sort(temp.begin(), temp.end(), [](const auto &p1, const auto &p2) { 
                        return p1.first < p2.first;
                    });

                    // insert into sorted
                    sorted.clear();
                    for (const auto &[pos, key]: temp) {
                        sorted.emplace_back(key, const_cast<T&>(data.at(key)));
                        constSorted.emplace_back(key, data.at(key));
                    }

                    // Flag to check if we require sorting
                    dirty = false;
                }
            }

            static bool validateKey(const std::string &key) {
                for (const char &ch: key) {
                    if (ch == '\n' || ch == '\r' || ch == '\b')
                        return false;
                }
                return true;
            }

        public:
            using OrderedIterator = std::vector<std::pair<std::string, T&>>::iterator;
            using ConstOrderedIterator = std::vector<std::pair<std::string, const T&>>::const_iterator;

            OrderedIterator begin() {
                buildIterator();
                return sorted.begin();
            }

            OrderedIterator end() {
                buildIterator();
                return sorted.end();
            }

            ConstOrderedIterator begin() const {
                buildIterator();
                return constSorted.cbegin();
            }

            ConstOrderedIterator end() const {
                buildIterator();
                return constSorted.cend();
            }

            // Non const access
            inline T &operator[] (const std::string &key) {
                if (!validateKey(key))
                    throw std::runtime_error("Key contains unsupported characters.");
                else if (!exists(key)) {
                    dirty = true;
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
                    dirty = true; return true;
                }
            }
    };

    class Section: public Iterator<std::string> {
        public:
            bool empty() const {
                return data.empty();
            }
    };

    class Parser: public Iterator<Section> {
        public:
            using Iterator::exists, Iterator::remove;

            bool exists(const std::string &sectionName, const std::string &key) const noexcept {
                return this->exists(sectionName) && this->operator[](sectionName).exists(key);
            }

            bool remove(const std::string &sectionName, const std::string &key) {
                if (!exists(sectionName, key)) return false;
                else {
                    Section section {this->operator[](sectionName)};
                    section.remove(key);
                    if (section.empty())
                        this->remove(sectionName);
                    return true;
                }
            }

            void reads(const std::string &raw) {

            }

            std::string dumps() const {
                std::ostringstream oss; 
                for (const auto& [sectionName, section]: *this) {
                    oss << "[" << sectionName << "]" << '\n';
                    for (const auto &[key, value]: section) {
                        oss << key << " = ";
                        for (const char &ch: value) {
                            oss << ch;
                            if (ch == '\n')
                                oss << '\t';
                        }
                        oss << '\n';
                    }
                    oss << '\n';
                }
                return oss.str();
            }
    };
};
