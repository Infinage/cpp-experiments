#pragma once

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include "ordered_map.hpp"

namespace INI {
    template<typename T>
    class Iterator {
        protected:
            // Data containers
            stdx::ordered_map<std::string, T> data;

            static bool validateKey(const std::string &key) {
                for (const char &ch: key) {
                    if (ch == '\n' || ch == '\r' || ch == '\b')
                        return false;
                }
                return true;
            }

            static inline std::string tolower(std::string str) {
                std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) { 
                    return std::tolower(ch);
                });
                return str;
            }

        public:
            using OrderedIterator = stdx::ordered_map<std::string, T>::iterator;
            using ConstOrderedIterator = stdx::ordered_map<std::string, T>::const_iterator;

            OrderedIterator begin() { return data.begin(); }
            OrderedIterator end() { return data.end(); }

            ConstOrderedIterator begin() const { return data.cbegin(); }
            ConstOrderedIterator end() const { return data.cend(); }

            // Non const access
            inline T &operator[] (const std::string &key_) {
                // Convert to lower case for Section keys - no matter what key user enters lowercase it
                std::string key {std::is_same_v<T, std::string>? tolower(key_): key_};
                if (!validateKey(key))
                    throw std::runtime_error("Key contains unsupported characters.");
                else if (!exists(key))
                    data.insert(key, T{});
                return data[key];
            }

            // Const access
            inline const T &operator[] (const std::string &key_) const {
                // Convert to lower case for Section keys
                std::string key {std::is_same_v<T, std::string>? tolower(key_): key_};
                if (!exists(key)) 
                    throw std::runtime_error("Key: `" + key + "` not found.");
                return data.at(key);
            }

            inline bool exists(const std::string &key_) const noexcept {
                // Convert to lower case for Section keys regardless of user input
                std::string key {std::is_same_v<T, std::string>? tolower(key_): key_};
                return data.find(key) != data.end();
            }

            bool remove(const std::string &key_) {
                // Convert to lower case for Section keys regardless of user input
                std::string key {std::is_same_v<T, std::string>? tolower(key_): key_};
                return data.erase(key);
            }
    };

    class Section: public Iterator<std::string> {
        public:
            inline bool empty() const {
                return data.empty();
            }
    };

    class Parser: public Iterator<Section> {
        private:
            template<typename T>
            static std::size_t findFirstMatch(const std::string &str, T func) {
                std::size_t pos = std::string::npos;
                for (std::size_t i {0}; i < str.size(); i++) {
                    if (func(str[i]))
                        return i;
                }
                return pos;
            }

            static void trim(std::string &str) {
                // Remove spaces from the end
                while (!str.empty() && std::isspace(str.back()))
                    str.pop_back();

                // First non space char from string
                std::size_t firstNonSpacePos {findFirstMatch(str, [](const char &ch) { 
                    return !std::isspace(ch); 
                })};

                // Do nothing, no trim required
                if (firstNonSpacePos == 0) return;
                
                // Non space char exists and is > 0, trim required
                else if (firstNonSpacePos != std::string::npos)
                    str = str.substr(firstNonSpacePos);

                // No non space char exists (either empty or all spaces, all 
                // spaces cannot be since prev step would eliminat it)
                else str = "";
            }

            static std::pair<std::string, std::string> extractKV(const std::string &line, std::size_t splitPos) {
                std::string   key {line.substr(0,  splitPos)}; 
                std::string value {line.substr(splitPos + 1)};
                trim(key); trim(value);
                return {key, value};
            }

        public:
            using Iterator::exists, Iterator::remove;

            bool exists(const std::string &sectionName, const std::string &key) const noexcept {
                return exists(sectionName) && this->operator[](sectionName).exists(key);
            }

            bool remove(const std::string &sectionName, const std::string &key) {
                if (!exists(sectionName, key)) return false;
                else {
                    Section section {this->operator[](sectionName)};
                    section.remove(key);
                    if (section.empty())
                        remove(sectionName);
                    return true;
                }
            }

            // Read into curr object from an input string
            void reads(const std::string &raw, bool ignoreDuplicates = false) {
                std::string currSectionName, prevKey, line; 
                std::size_t prevFirstNonSpace {0}, emptyLines {0}, lineNo {1};
                for (const char &ch: raw) {
                    if (ch == '\n') {
                        // Find first non space char if not found returns string::npos
                        std::size_t firstNonSpace {findFirstMatch(line, [](const char &ch) { return !std::isspace(ch); })};

                        // Ignore if line contains no nonspace char or starts with a comment
                        if (firstNonSpace != std::string::npos && line[firstNonSpace] != ';' && line[firstNonSpace] != '#') {
                            // Strip spaces from both ends
                            trim(line);

                            // Find first key value seperator
                            std::size_t firstKVSeperator {findFirstMatch(line, [](const char &ch) { 
                                return ch == ':' || ch == '='; 
                            })};

                            // Check indentation level with prev level
                            if (firstNonSpace > prevFirstNonSpace && exists(currSectionName, prevKey)) {
                                data[currSectionName][prevKey] += std::string(emptyLines + 1, '\n') + line;
                            }

                            // Check start of a new section
                            else if (line.size() >= 3 && line[0] == '[' && line.back() == ']') {
                                prevKey = ""; prevFirstNonSpace = 0;
                                currSectionName = line.substr(1, line.size() - 2);
                                if (exists(currSectionName) && !ignoreDuplicates)
                                    throw std::runtime_error("Line #: " + std::to_string(lineNo) + ": Section '" 
                                            + currSectionName + "' already exists.");
                                (*this)[currSectionName] = {};
                            }

                            // Check if line contains '=' or ':', key contains atleast 1 char
                            else if (firstKVSeperator != std::string::npos && firstKVSeperator > 0) {
                                prevFirstNonSpace = firstNonSpace; std::string value;
                                std::tie(prevKey, value) = extractKV(line, firstKVSeperator);
                                if (exists(currSectionName, prevKey) && !ignoreDuplicates)
                                    throw std::runtime_error("Line #: " + std::to_string(lineNo) + ": Option '" + 
                                            prevKey + "' in section '" + currSectionName + "' already exists.");
                                (*this)[currSectionName][prevKey] = value;
                            }

                            else {
                                throw std::runtime_error("Line #: " + std::to_string(lineNo) +" Error parsing line: " + line);
                            }

                            // Every time it is not an empty line, reset the counter
                            emptyLines = 0;
                        } 

                        // Counting newlines to add into multi lined values
                        else if (firstNonSpace == std::string::npos) emptyLines++;

                        // Execute for every line once we have processed them
                        line.clear(); lineNo++;
                        
                    } else line += ch;
                }
            }

            // Write the file in INI format as a string
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
