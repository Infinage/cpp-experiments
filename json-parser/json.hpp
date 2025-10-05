#pragma once

/*
 * Inspiration: https://codingchallenges.fyi/challenges/challenge-json-parser
 * Data Structure credits: https://stackoverflow.com/questions/19543326/datatypes-for-representing-json-in-c
 * Design decisions:
       1. Mimic (imperfectly) python's JSON module: loads, dumps
       2. Throw errors instead of a cout + nullptr
       3. Vector used for both Arrays & Objects following the stackoverflow thread, to maintain the insertion order
 * Library can be used to create JSON Documents (refer example-document.cpp)
 * Module can be used to parse JSON files provided the file is already loaded into memory (refer validate-json.cpp)
 */

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <memory>
#include <numeric>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace JSON {

    // Helper to constrain given type exists in variant type
    template<typename T, typename Variant> struct is_in_variant;
    template<typename T, typename... Types>
    struct is_in_variant<T, std::variant<Types...>>: std::disjunction<std::is_same<T, Types>...> {};

    // Types of JSON objects
    enum class NodeType: short {value, array, object};

    // Forward declaration
    class JSONNode;

    // Types of JSON Simple values
    using JSONSimpleType = std::variant<std::string, std::nullptr_t, long, double, bool>;

    // Concept to constrain input type to lie within above types
    template<typename T> concept JSONSimple = is_in_variant<T, JSONSimpleType>::value;

    // Abstracting shared_ptr + saving some typing effort
    using JSONNodePtr = std::shared_ptr<JSONNode>;

    // Base class - all 3 JSON objects would have a key.
    // Values are handled differently as per object type
    class JSONNode {
        private:
            std::string key;
            NodeType type;

        public:
            JSONNode(const std::string &k, const NodeType &t):
                key(k), type(t) {};

            NodeType getType() {
                return type;
            }

            std::string &getKey() {
                return key;
            }

            void setKey(const std::string &k) {
                key = k;
            }
    };

    // Simple pair of Key - Values
    // Key is optional - if blank set it as empty
    class JSONValueNode: public JSONNode {
        private:
            JSONSimpleType value;

        public:
            friend struct JSONHandle;
            static constexpr NodeType staticType {NodeType::value};

            JSONValueNode(const JSONSimpleType &v): JSONNode("", NodeType::value), value(v) {};

            JSONValueNode(const std::string &k, const JSONSimpleType &v):
                JSONNode(k, NodeType::value), value(v) {};

            JSONSimpleType &getValue() { return value; }
            void setValue(const JSONSimpleType &val) { value = val; }
    };

    // Array of JSONNodes (could be any of the 3 object types)
    // Can be nested at any level, hence we repr it as a vector of JSONNode (pointers)
    // Similar to JSONValueNode, key is optional and set to empty string if not set
    class JSONArrayNode: public JSONNode {
        private:
            std::vector<JSONNodePtr> values;

        public:
            friend struct JSONHandle;
            static constexpr NodeType staticType {NodeType::array};

            JSONArrayNode(const std::string &k): JSONNode(k, NodeType::array) {};
            JSONArrayNode(const std::string &k, const std::vector<JSONNodePtr> &v): JSONNode(k, NodeType::array), values(v) {};
            JSONArrayNode(const std::vector<JSONNodePtr> &v): JSONNode("", NodeType::array), values(v) {};

            std::size_t size() {
                return values.size();
            }

            void push(JSONNodePtr node) {
                values.push_back(node);
            }

            JSONNodePtr pop() {
                JSONNodePtr result = values.back();
                values.pop_back();
                return result;
            }

            JSONNode &operator[] (std::size_t idx) {
                if (idx < values.size())
                    return *values[idx];
                else
                    throw std::invalid_argument("Out of bounds");
            }

            // Iterators to do a `for (JSONNodePtr &ptr: arr_) {}`
            std::vector<JSONNodePtr>::iterator begin() { return values.begin(); }
            std::vector<JSONNodePtr>::iterator end() { return values.end(); }
            std::vector<JSONNodePtr>::const_iterator cbegin() const { return values.cbegin(); }
            std::vector<JSONNodePtr>::const_iterator cend() const { return values.cend(); }
    };

    // Similar to JSONArrayNode, diff being in how its children would be rendered
    // All the children's keys would be rendered during dump (whereas an array node's children's key would not)
    // We disallow duplicate key entries as is typical for any dictionary like data structure
    class JSONObjectNode: public JSONNode {
        private:
            std::vector<JSONNodePtr> values;

            bool checkDuplicates(const std::vector<JSONNodePtr> &v) {
                std::unordered_set<JSONSimpleType> st;
                for (const JSONNodePtr &ele: v) {
                    if (st.find(ele->getKey()) != st.end())
                        return false;
                    else
                        st.insert(ele->getKey());
                }
                return true;
            }

        public:
            friend struct JSONHandle;
            static constexpr NodeType staticType {NodeType::object};

            JSONObjectNode(const std::string &k): JSONNode(k, NodeType::object) {};

            JSONObjectNode(const std::vector<JSONNodePtr> &v): JSONNode("", NodeType::object) {
                if (!checkDuplicates(v))
                    throw std::invalid_argument("Duplicate key found");
                values = v;
            }

            JSONObjectNode(const std::string &k, const std::vector<JSONNodePtr> &v): JSONNode(k, NodeType::object) {
                // Check for duplicates in one shot before assigning the values O(N)
                if (!checkDuplicates(v))
                    throw std::invalid_argument("Duplicate key found");
                values = v;
            }

            std::size_t size() const {
                return values.size();
            }

            void push(JSONNodePtr node) {
                auto it = find(node->getKey());
                if (it == values.end())
                    values.push_back(node);
                else
                    values[(std::size_t)(it - values.begin())] = node;
            }

            std::vector<JSONNodePtr>::iterator find(const std::string &k) {
                for (std::vector<JSONNodePtr>::iterator it = values.begin(); it < values.end(); it++) {
                    if ((*it)->getKey() == k)
                        return it;
                }
                return values.end();
            }

            // Access by keys (strings)
            // Much slower than traditional dict objects since we iterate sequentially - O(N)
            JSONNode &operator[] (const std::string &k) {
                auto it = find(k);
                if (it != values.end())
                    return **it;
                else
                    throw std::invalid_argument("Key not found: " + k);
            }

            // Iterators - similar to what we have for Array Objects
            std::vector<JSONNodePtr>::iterator begin() { return values.begin(); }
            std::vector<JSONNodePtr>::iterator end() { return values.end(); }
            std::vector<JSONNodePtr>::const_iterator cbegin() const { return values.cbegin(); }
            std::vector<JSONNodePtr>::const_iterator cend() const { return values.cend(); }
    };

    /*
     * A convenience wrapper to support dynamically overloaded operators
     */
    struct JSONHandle {
        JSONNodePtr ptr;

        NodeType getType() const { return ptr->getType(); }
        JSONHandle(JSONNodePtr ptr): ptr{ptr} {}

        JSONHandle operator[](this auto &&self, const std::size_t idx) {
            // Check if type is indeed an array
            if (!self.ptr || self.ptr->getType() != NodeType::array) 
                return {nullptr};

            // Check if idx within bounds
            auto *node {static_cast<JSONArrayNode*>(self.ptr)};
            if (idx >= node->values.size()) return {nullptr};

            // Return shared_ptr from underlying vector
            return node->values[idx];
        }

        JSONHandle operator[](this auto &&self, const std::string &key) {
            // Check if type is indeed an object
            if (self.ptr->getType() != NodeType::object) return {nullptr};

            // Check if object has the required key
            auto &obj {static_cast<JSONObjectNode&>(*self.ptr)};
            auto it {obj.find(key)};
            if (it == obj.values.end()) return {nullptr};

            // Return underlying shared_ptr
            return *it;
        }

        JSONHandle at(this auto &&self, const std::size_t idx) {
            auto result {self.get(idx)};
            if (!result) {
                if (self.getType() != NodeType::object) throw std::runtime_error("Node is not an array");
                else throw std::runtime_error("Array idx out of bounds, tried to access: " + std::to_string(idx));
            }            
            return *result;
        }

        JSONHandle at(this auto &&self, const std::string &key) {
            auto result {self.get(key)};
            if (!result) {
                if (self.getType() != NodeType::object) throw std::runtime_error("Node is not an object");
                else throw std::runtime_error("Object key doesn't exist, tried to access with: " + key);
            }
            return *result;
        }

        /*
         * Converts into one of the JSONSimpleType, if type is a simple type
         * If ptr is null, defaults constructs it
         */
        template<JSONSimple VariantType>
        auto to() const {
            // Return 0 equivalent or whatever that is default constructible
            if (!ptr) return VariantType{};

            if (ptr->getType() != NodeType::value)
                throw std::runtime_error{"Node is not an simple type"};

            auto &value {static_cast<JSONValueNode&>(*ptr).value};
            if (!std::holds_alternative<VariantType>(value))
                throw std::invalid_argument("Variant type expected doesn't match with what is available");

            return std::get<VariantType>(value);
        }

        /*
         * Convenience wrapper to dump output as a string
         */
        inline std::string str(bool pretty = true) const;

        template<typename T>
        decltype(auto) cast() requires(std::is_base_of_v<JSONNode, T>) {
            if (getType() != T::staticType) throw std::runtime_error{"Invalid Cast"};
            return static_cast<T&>(*ptr);
        }
    };

    // Helper to make node creation easier
    // Using RValue reference to avoid need to create objects and then pass by reference
    namespace helper {
        // Simple Node
        inline JSONNodePtr createNode(const JSONSimpleType &value) { return std::make_shared<JSONValueNode>(value); }
        inline JSONNodePtr createNode(const std::string &key, const JSONSimpleType &value) { return std::make_shared<JSONValueNode>(key, value); }

        // Creating Arrays
        inline JSONNodePtr createArray(const std::vector<JSONNodePtr> &values) { return std::make_shared<JSONArrayNode>(values); }
        inline JSONNodePtr createArray(const std::string &key, const std::vector<JSONNodePtr> &values) { return std::make_shared<JSONArrayNode>(key, values); }

        // Creating Objects
        inline JSONNodePtr createObject(const std::vector<JSONNodePtr> &values) { return std::make_shared<JSONObjectNode>(values); }
        inline JSONNodePtr createObject(const std::string &key, const std::vector<JSONNodePtr> &values) { return std::make_shared<JSONObjectNode>(key, values); }

        // Helper function to format JSON_SIMPLE_TYPEs to String
        inline std::string simple_format (const JSONSimpleType &v) {
            // String
            if (std::holds_alternative<std::string>(v)) {
                auto val {std::get<std::string>(v)};
                std::string escaped {}; auto quoteCount {std::count(val.begin(), val.end(), '"')};
                escaped.reserve(val.size() + static_cast<std::size_t>(2 * quoteCount));
                std::for_each(val.begin(), val.end(), 
                [&escaped](char ch){ 
                    if (ch == '"') escaped.push_back('\\');
                    escaped.push_back(ch);
                });
                return "\"" + escaped + "\"";
            }

            // NULLPTR
            else if (std::holds_alternative<std::nullptr_t>(v))
                return "null";
            
            // LONG
            else if (std::holds_alternative<long>(v))
                return std::to_string(std::get<long>(v));

            // DOUBLE
            else if (std::holds_alternative<double>(v))
                return std::to_string(std::get<double>(v));

            // BOOL
            else
                return std::get<bool>(v)? "true": "false";
        }

        // Helper function to parse string into JSON Simple Type objects
        inline JSONSimpleType simple_parse(const std::string &token) {
            // Compute the number of digits in the string to determine
            // if it could be an long or a double
            std::vector<bool> isDigit;
            std::transform(token.begin(), token.end(), std::back_inserter(isDigit), [](char ch) { return std::isdigit(ch); });
            std::size_t digitCount = std::accumulate(isDigit.begin(), isDigit.end(), (std::size_t) 0, [](std::size_t sum, bool b) {
                    return sum + (b? 1: 0);
            });

            // Error to throw if the input token is not correctly formatted
            std::invalid_argument error = std::invalid_argument("Invalid value: " + token);

            // Check if leading zeros exist in a long or a double
            auto leadingZeros = [] (const std::string &tok) -> bool {
                std::size_t firstDigit = tok.find_first_of("0123456789");
                return (tok[firstDigit] == '0' && firstDigit + 1 < tok.size() && std::isdigit(tok[firstDigit + 1]));
            };

            // Position of '.' for decimals
            std::size_t dotPos = token.find('.');

            // Begin Parsing Logic -----------------------

            // NULLPTR
            if (token == "null")
                return nullptr;

            // BOOL
            else if (token == "true" || token == "false")
                return token == "true";

            // STRING - enclosed within quotes - ensure there aren't newlines or tabs inside
            else if (token[0] == '"' && token.back() == '"') {
                if (token.find_first_of("\t\n") == std::string::npos) return token.substr(1, token.size() - 2);
                else throw error;
            }

            // LONG - Digit count should be same as token size or one less (for negative sign)
            // Ensure there aren't leading zeros - '001' or '-01'
            else if (digitCount == token.size() || (digitCount == token.size() - 1 && token[0] == '-')) {
                if (!leadingZeros(token)) return std::stol(token);
                else throw error;
            }

            // DOUBLE - Same as LONG but one less digit allowed for the decimal point
            // Ensure there are no leading zeros, also check if '.' succeeds and preceeds a digit
            else if (
                    ((digitCount == token.size() - 1 && dotPos != std::string::npos && dotPos != 0) ||
                    (digitCount == token.size() - 2 && dotPos != std::string::npos && token[0] == '-' && dotPos != 1)) &&
                    dotPos != token.size() - 1
                ) {
                if (!leadingZeros(token)) return std::stod(token);
                else throw error;
            }

            // DOUBLE - Scientific notation (make use of strtod)
            else if (token.find_first_of("eE") != std::string::npos) {
                char *ptr;
                double result = std::strtod(token.c_str(), &ptr);
                if (*ptr == '\0' && !leadingZeros(token)) return result;
                else throw error;
            }

            // What the heck is this token?
            else
                throw error;
        }
    }

    // Handles logic To parse JSON from strings (and) To Dump JSON into string
    class Parser {
        private:
            static std::string repeatStr(std::string_view str, std::size_t times) {
                std::string result; 
                result.reserve(str.size() * times);
                while (times-- > 0) result += str;
                return result;
            };

            // Recursively dump output, skipKey and level are function parameters
            static std::string _dumps(JSONNodePtr root, bool pretty, bool _skipKey, std::size_t _level) {
                if (root == nullptr) return "";

                std::string keyStr;
                if (!_skipKey) {
                    keyStr = "\"" + root->getKey() + "\":";
                    if (pretty) keyStr.push_back(' ');
                }

                if (root->getType() == NodeType::value) {
                    JSONValueNode &v = static_cast<JSONValueNode&>(*root);
                    return keyStr + helper::simple_format(v.getValue());
                }

                else if (root->getType() == NodeType::array) {
                    std::string result {keyStr + "["}, spacing {pretty? '\n' + repeatStr("  ", _level): ""};
                    JSONArrayNode &v = static_cast<JSONArrayNode&>(*root);
                    for (JSONNodePtr nxt: v) 
                        result += spacing + _dumps(nxt, pretty, true, _level + 1) + ",";
                    if (v.size() > 0) result.pop_back();
                    if (pretty) result += '\n' + repeatStr(" ", _level - 1);
                    result += ']';
                    return result;
                }

                else {
                    std::string result {keyStr + "{"}, spacing {pretty? '\n' + repeatStr("  ", _level): ""};
                    JSONObjectNode &v = static_cast<JSONObjectNode&>(*root);
                    for (JSONNodePtr nxt: v) result += spacing + _dumps(nxt, pretty, false, _level + 1) + ",";
                    if (v.size() > 0) result.pop_back();
                    if (pretty) result += '\n' + repeatStr(" ", _level - 1);
                    result += '}';
                    return result;
                } 
            }

        public:
            // Load from a string in memory into JSON
            static JSONHandle loads(const std::string &raw) {

                std::unordered_set<char> 
                    // List of special characters that seperate the individual tokens
                    splChars{'{', '}', '[', ']', ',', ':'},

                    // List of valid characters that can follow a '\'
                    validEscapes{'"', '\\', '/', 'b', 'f', 'n', 'r', 't', 'u'};

                // Capture what opening brace we encountered while keeping track of position
                std::stack<std::pair<char, std::size_t>> validateStk;
                std::stack<std::variant<std::string, JSONNodePtr>> tokens;
                std::string acc{""};
                bool processingString = false, currCharEscaped = false;

                // We count commas, colons and compute the expected values to check if the JSON is valid
                long commas = 0, commasExpected = 0, colons = 0, colonsExpected = 0;

                for (char ch: raw) {
                    if (ch == '"' || processingString) {
                        acc += ch;

                        // Put everything demarcated by a quote inside
                        if (ch == '"' && !currCharEscaped) {
                            processingString = !processingString;
                            if (!processingString) {
                                tokens.push(acc);
                                acc.clear();
                            }
                        }

                        // Logic to handle escape sequences
                        if (ch == '\\')
                            currCharEscaped = !currCharEscaped;

                        // Next char following an escape char
                        else if (currCharEscaped) {
                            if (validEscapes.find(ch) == validEscapes.end())
                                throw std::invalid_argument("Invalid Escape \\" + std::string(1, ch));
                            else
                                currCharEscaped = false;
                        }


                    } else if (!std::isspace(ch)) {
                        // Accumulate until we hit a special character
                        // Note that strings are already capture in the above if block
                        // Here capture long, bools, nulls, etc
                        if (!std::isspace(ch) && splChars.find(ch) == splChars.end())
                            acc += ch;

                        else if (acc.size()) {
                            tokens.push(acc);
                            acc.clear();
                        }

                        // We hit a closing paren but dont have a matching one -> invalid json
                        if (
                                (ch == '}' && (validateStk.empty() || validateStk.top().first != '{')) ||
                                (ch == ']' && (validateStk.empty() || validateStk.top().first != '['))
                            )
                            throw std::invalid_argument("Invalid JSON");

                        else if (ch == '{' || ch == '[')
                            validateStk.push({ch, tokens.size()});

                        // End paren but not invalid, pop until closing pair is
                        // hit, creating nodes for tokens in between
                        else if (ch == '}' || ch == ']') { 
                            auto [prevCh, startPos] = validateStk.top();
                            validateStk.pop();
                            std::vector<JSONNodePtr> values;
                            while (tokens.size() > startPos) {
                                std::variant<std::string, JSONNodePtr>& valMixed = tokens.top();
                                // If we encounter a token that had already been
                                // processed into a node, push it as is
                                if (std::holds_alternative<JSONNodePtr>(valMixed))
                                    values.push_back(std::get<JSONNodePtr>(valMixed));

                                // String token - needs processing
                                else
                                    values.push_back(std::make_shared<JSONValueNode>(helper::simple_parse(std::get<std::string>(valMixed))));

                                // Pop at last for keeping the '&' reference valid
                                tokens.pop();

                                // If object we need to check for keys, if arrays
                                // auto set key as empty strings
                                std::string key {""};
                                if (prevCh == '{') {
                                    // If tokens.size() is insufficient we set key to a value that will fail
                                    key = tokens.size() > 0? std::get<std::string>(tokens.top()): "*";

                                    // Similar to how we checked for newlines & tabs in values,
                                    // ensure key strings dont have these as well
                                    if (key[0] != '"' || key.back() != '"' || key.find_first_of("\n\t") != std::string::npos)
                                        throw std::invalid_argument("Invalid JSON");
                                    else
                                        key = key.substr(1, key.size() - 2);
                                    tokens.pop();
                                }
                                values.back()->setKey(key);
                            }

                            // Stack pop yields in reverse order - unreverse it
                            std::ranges::reverse(values);

                            if (prevCh == '{')
                                tokens.push(std::make_shared<JSONObjectNode>(values));
                            else
                                tokens.push(std::make_shared<JSONArrayNode>(values));

                            // Based on the objects and arrays we are having, we can compute how many we expect to have
                            // [1, 2, 3] (2 commas for 2 objs) | {"a": 1, "b": 2, "c": 3} (3 colons for 3 objs)
                            commasExpected += std::max((long)values.size() - 1, 0L);
                            colonsExpected += prevCh == '{'? values.size(): 0L;

                        } else if (ch == ',' || ch == ':') {
                            commas += (ch == ','? 1: 0);
                            colons += (ch == ':'? 1: 0);
                        }
                    }
                }

                if (
                        commasExpected == commas && colonsExpected == colons && acc.size() == 0 &&
                        tokens.size() == 1 && std::holds_alternative<JSONNodePtr>(tokens.top())
                    )
                    return std::get<JSONNodePtr>(tokens.top());
                else
                    throw std::logic_error("Invalid JSON");
            }

            // Exposed function hides some parameters from end user
            static std::string dumps(JSONHandle root_, bool pretty = false) {
                return _dumps(root_.ptr, pretty, true, 1);
            }
    };

    // Define the JSONHandle::str convenience function
    inline std::string JSONHandle::str(bool pretty) const { 
        std::string jsonDump {Parser::dumps(*this, pretty)}; 
        return jsonDump;
    }
}
