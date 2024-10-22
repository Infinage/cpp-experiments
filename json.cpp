/*

 * Inspiration: https://codingchallenges.fyi/challenges/challenge-json-parser
 * Data Structure credits: https://stackoverflow.com/questions/19543326/datatypes-for-representing-json-in-c
  
 * Design decisions:
       1. Mimic (imperfectly) python's JSON module: loads, dumps
       2. Throw errors instead of a cout + nullptr
       3. Vector used for both Arrays & Objects following the stackoverflow thread, to maintain the insertion order
  
 * Module can be used to create JSON Documents as follows:
   ```
   // Array
   JSON::JSONNode_Ptr one     = JSON::helper::createNode(1);
   JSON::JSONNode_Ptr two     = JSON::helper::createNode(2);
   JSON::JSONNode_Ptr three   = JSON::helper::createNode(3);
   JSON::JSONNode_Ptr arr_    = JSON::helper::createArray("array", {one, two, three});

   // Simple Data types
   JSON::JSONNode_Ptr bool_   = JSON::helper::createNode("boolean", true);
   JSON::JSONNode_Ptr null_   = JSON::helper::createNode("null", nullptr);
   JSON::JSONNode_Ptr int_    = JSON::helper::createNode("number", 123);
   JSON::JSONNode_Ptr float_  = JSON::helper::createNode("float", 1.0);
   JSON::JSONNode_Ptr string_ = JSON::helper::createNode("string", "Hello world");

   // Object
   JSON::JSONNode_Ptr a       = JSON::helper::createNode("a", "b");
   JSON::JSONNode_Ptr c       = JSON::helper::createNode("c", "d");
   JSON::JSONNode_Ptr empty   = JSON::helper::createNode("", "empty");
   JSON::JSONNode_Ptr obj_    = JSON::helper::createObject("object", {a, c, empty});

   // Create the root object
   JSON::JSONNode_Ptr root    = JSON::helper::createObject({arr_, bool_, null_, int_, float_, string_, obj_});

   // Serialize & print the result
   std::cout << JSON::Parser::dumps(root) << "\n";
   ```

 * Module can be used to parse JSON files provided the file is already loaded into memory
   ```
   // Parse string into JSON Root obj
   JSON::JSONNode_Ptr root = JSON::Parser::loads(jsonStr);

   // Serialize & print the result
   std::cout << JSON::Parser::dumps(root) << "\n";
  ```
 */

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace JSON {

    // Types of JSON objects
    enum NODE_TYPE: short {value, array, object};

    // Forward declaration
    class JSONNode;

    // Types of JSON Simple values
    using JSON_SIMPLE_TYPE = std::variant<std::string, std::nullptr_t, int, double, bool>;

    // Abstracting shared_ptr + saving some typing effort
    using JSONNode_Ptr = std::shared_ptr<JSONNode>;

    // Base class - all 3 JSON objects would have a key.
    // Values are handled differently as per object type
    class JSONNode {
        private:
            std::string key;
            NODE_TYPE type;

        public:
            JSONNode(const std::string &k, NODE_TYPE t):
                key(k), type(t) {};

            NODE_TYPE getType() {
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
            JSON_SIMPLE_TYPE value;

        public:
            JSONValueNode(const JSON_SIMPLE_TYPE &v): JSONNode("", NODE_TYPE::value), value(v) {};

            JSONValueNode(const std::string &k, const JSON_SIMPLE_TYPE &v):
                JSONNode(k, NODE_TYPE::value), value(v) {};

            JSON_SIMPLE_TYPE &getValue() {
                return value;
            }
    };

    // Array of JSONNodes (could be any of the 3 object types)
    // Can be nested at any level, hence we repr it as a vector of JSONNode (pointers)
    // Similar to JSONValueNode, key is optional and set to empty string if not set
    class JSONArrayNode: public JSONNode {
        private:
            std::vector<JSONNode_Ptr> values;

        public:
            JSONArrayNode(const std::string &k): JSONNode(k, NODE_TYPE::array) {};
            JSONArrayNode(const std::string &k, std::vector<JSONNode_Ptr> &v): JSONNode(k, NODE_TYPE::array), values(v) {};
            JSONArrayNode(std::vector<JSONNode_Ptr> &v): JSONNode("", NODE_TYPE::array), values(v) {};

            std::size_t size() {
                return values.size();
            }

            void push(JSONNode_Ptr node) {
                values.push_back(node);
            }

            JSONNode_Ptr pop() {
                JSONNode_Ptr result = values.back();
                values.pop_back();
                return result;
            }

            JSONNode_Ptr &operator[] (std::size_t idx) {
                if (idx < values.size())
                    return values[idx];
                else
                    throw std::invalid_argument("Out of bounds");
            }

            // Iterators to do a `for (JSONNode_Ptr &ptr: arr_) {}`
            std::vector<JSONNode_Ptr>::iterator begin() { return values.begin(); }
            std::vector<JSONNode_Ptr>::iterator end() { return values.end(); }
            std::vector<JSONNode_Ptr>::const_iterator cbegin() const { return values.cbegin(); }
            std::vector<JSONNode_Ptr>::const_iterator cend() const { return values.cend(); }
    };

    // Similar to JSONArrayNode, diff being in how its children would be rendered
    // All the children's keys would be rendered during dump (whereas an array node's children's key would not)
    // We disallow duplicate key entries as is typical for any dictionary like data structure
    class JSONObjectNode: public JSONNode {
        private:
            std::vector<JSONNode_Ptr> values;

            bool checkDuplicates(std::vector<JSONNode_Ptr> &v) {
                std::unordered_set<JSON_SIMPLE_TYPE> st;
                for (JSONNode_Ptr &ele: v) {
                    if (st.find(ele->getKey()) != st.end())
                        return false;
                    else
                        st.insert(ele->getKey());
                }
                return true;
            }

        public:
            JSONObjectNode(const std::string &k): JSONNode(k, NODE_TYPE::object) {};

            JSONObjectNode(std::vector<JSONNode_Ptr> &v): JSONNode("", NODE_TYPE::object) {
                if (!checkDuplicates(v))
                    throw std::invalid_argument("Duplicate key found");
                values = v;
            }

            JSONObjectNode(const std::string &k, std::vector<JSONNode_Ptr> &v): JSONNode(k, NODE_TYPE::object) {
                // Check for duplicates in one shot before assigning the values O(N)
                if (!checkDuplicates(v))
                    throw std::invalid_argument("Duplicate key found");
                values = v;
            }

            std::size_t size() {
                return values.size();
            }

            void push(JSONNode_Ptr node) {
                auto it = find(node->getKey());
                if (it == values.end())
                    values.push_back(node);
                else
                    values[(std::size_t)(it - values.begin())] = node;
            }

            std::vector<JSONNode_Ptr>::iterator find(std::string &k) {
                for (std::vector<JSONNode_Ptr>::iterator it = values.begin(); it < values.end(); it++) {
                    if ((*it)->getKey() == k)
                        return it;
                }
                return values.end();
            }

            // Access by keys (strings)
            // Much slower than traditional dict objects since we iterate sequentially - O(N)
            JSONNode_Ptr &operator[] (std::string &k) {
                auto it = find(k);
                if (it != values.end())
                    return *it;
                else
                    throw std::invalid_argument("Key not found: ");
            }

            // Iterators - similar to what we have for Array Objects
            std::vector<JSONNode_Ptr>::iterator begin() { return values.begin(); }
            std::vector<JSONNode_Ptr>::iterator end() { return values.end(); }
            std::vector<JSONNode_Ptr>::const_iterator cbegin() const { return values.cbegin(); }
            std::vector<JSONNode_Ptr>::const_iterator cend() const { return values.cend(); }
    };

    // Helper to make node creation easier
    // Using RValue reference to avoid need to create objects and then pass by reference
    namespace helper {
        // Simple Node
        JSONNode_Ptr createNode(JSON_SIMPLE_TYPE &&value) { return std::make_shared<JSONValueNode>(value); }
        JSONNode_Ptr createNode(std::string &&key, JSON_SIMPLE_TYPE &&value) { return std::make_shared<JSONValueNode>(key, value); }

        // Creating Arrays
        JSONNode_Ptr createArray(std::vector<JSONNode_Ptr> &&values) { return std::make_shared<JSONArrayNode>(values); }
        JSONNode_Ptr createArray(std::string &&key, std::vector<JSONNode_Ptr> &&values) { return std::make_shared<JSONArrayNode>(key, values); }

        // Creating Objects
        JSONNode_Ptr createObject(std::vector<JSONNode_Ptr> &&values) { return std::make_shared<JSONObjectNode>(values); }
        JSONNode_Ptr createObject(std::string &&key, std::vector<JSONNode_Ptr> &&values) { return std::make_shared<JSONObjectNode>(key, values); }

        // Helper function to format JSON_SIMPLE_TYPEs to String
        inline std::string simple_format (JSON_SIMPLE_TYPE &v) {
            // String
            if (std::holds_alternative<std::string>(v))
                return "\"" + std::get<std::string>(v) + "\"";

            // NULLPTR
            else if (std::holds_alternative<std::nullptr_t>(v))
                return "null";
            
            // INT
            else if (std::holds_alternative<int>(v))
                return std::to_string(std::get<int>(v));

            // DOUBLE
            else if (std::holds_alternative<double>(v))
                return std::to_string(std::get<double>(v));

            // BOOL
            else
                return std::get<bool>(v)? "true": "false";
        }

        // Helper function to parse string into JSON Simple Type objects
        inline JSON_SIMPLE_TYPE simple_parse(std::string &token) {
            // Compute the number of digits in the string to determine
            // if it could be an integer or a double
            std::vector<bool> isDigit;
            std::transform(token.begin(), token.end(), std::back_inserter(isDigit), [](char ch) { return std::isdigit(ch); });
            std::size_t digitCount = std::accumulate(isDigit.begin(), isDigit.end(), (std::size_t) 0, [](std::size_t sum, bool b) {
                    return sum + (b? 1: 0);
            });

            // Error to throw if the input token is not correctly formatted
            std::invalid_argument error = std::invalid_argument("Invalid value: " + token);

            // Check if leading zeros exist in a int or a double
            auto leadingZeros = [] (std::string &tok) -> bool {
                std::size_t firstDigit = tok.find_first_of("0123456789");
                return (tok[firstDigit] == '0' && firstDigit + 1 < tok.size() && std::isdigit(tok[firstDigit + 1]));
            };

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

            // INT - Digit count should be same as token size or one less (for negative sign)
            // Ensure there aren't leading zeros - '001' or '-01'
            else if (digitCount == token.size() || (digitCount == token.size() - 1 && token[0] == '-')) {
                if (!leadingZeros(token)) return std::stoi(token);
                else throw error;
            }

            // DOUBLE - Same as int but one less digit allowed for the decimal point
            // Ensure there are no leading zeros
            else if (
                    (digitCount == token.size() - 1 && token.find('.') != std::string::npos) ||
                    (digitCount == token.size() - 2 && token.find('.') != std::string::npos && token[0] == '-')
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

    // Handles logic to Parse JSON from strings and to Dump JSON into strings
    class Parser {
        public:
            static JSONNode_Ptr loads(std::string &raw) {
                std::unordered_set<char> 
                    splChars{'{', '}', '[', ']', ',', ':'},
                    validEscapes{'"', '\\', '/', 'b', 'f', 'n', 'r', 't', 'u'};
                std::string acc{""};
                std::stack<std::pair<char, std::size_t>> validateStk;
                std::stack<std::variant<std::string, JSONNode_Ptr>> tokens;
                bool processingString = false, currCharEscaped = false;
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
                        else if (currCharEscaped) {
                            if (validEscapes.find(ch) == validEscapes.end())
                                throw std::invalid_argument("Invalid Escape \\" + std::string(1, ch));
                            else
                                currCharEscaped = false;
                        }

                    } else if (!std::isspace(ch)) {
                        if (!std::isspace(ch) && splChars.find(ch) == splChars.end())
                            acc += ch;
                        else if (acc.size()) {
                            tokens.push(acc);
                            acc.clear();
                        }

                        if ((ch == '}' && (validateStk.empty() || validateStk.top().first != '{')) || (ch == ']' && (validateStk.empty() || validateStk.top().first != '[')))
                            throw std::invalid_argument("Invalid JSON");
                        else if (ch == '{' || ch == '[')
                            validateStk.push({ch, tokens.size()});
                        else if (ch == '}' || ch == ']') {
                            auto [prevCh, startPos] = validateStk.top();
                            validateStk.pop();
                            std::vector<JSONNode_Ptr> values;
                            while (tokens.size() > startPos) {
                                std::variant<std::string, JSONNode_Ptr>& valMixed = tokens.top();
                                if (std::holds_alternative<JSONNode_Ptr>(valMixed))
                                    values.push_back(std::get<JSONNode_Ptr>(valMixed));
                                else
                                    values.push_back(std::make_shared<JSONValueNode>(helper::simple_parse(std::get<std::string>(valMixed))));
                                tokens.pop();

                                std::string key {""};
                                if (prevCh == '{') {
                                    key = std::get<std::string>(tokens.top());
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
                        commasExpected == commas && colonsExpected == colons &&
                        tokens.size() == 1 && std::holds_alternative<JSONNode_Ptr>(tokens.top())
                    )
                    return std::get<JSONNode_Ptr>(tokens.top());
                else
                    throw std::logic_error("Invalid JSON");
            }

            static std::string dumps(JSONNode_Ptr root, bool ignoreKeys = true) {
                if (root == nullptr)
                    return "";

                else {
                    std::string keyStr = {ignoreKeys? "": "\"" + root->getKey() + "\": "};

                    if (root->getType() == NODE_TYPE::value) {
                        JSONValueNode &v = static_cast<JSONValueNode&>(*root);
                        return keyStr + helper::simple_format(v.getValue());
                    }

                    else if (root->getType() == NODE_TYPE::array) {
                        std::string result {keyStr + "["};
                        JSONArrayNode &v = static_cast<JSONArrayNode&>(*root);
                        for (JSONNode_Ptr nxt: v)
                            result += dumps(nxt, true) + ", ";
                        result += v.size() > 0?"\b\b]": "]";
                        return result;
                    }

                    else {
                        std::string result {keyStr + "{"};
                        JSONObjectNode &v = static_cast<JSONObjectNode&>(*root);
                        for (JSONNode_Ptr nxt: v)
                            result += dumps(nxt, false) + ", ";
                        result += v.size() > 0?"\b\b}": "}";
                        return result;
                    } 
                }
            }
    };

}


int main(int argc, char* argv[]) {

    if (argc != 2)
        std::cout << "Usage: ./json.out <filepath>\n";

    else {

        std::string fname{argv[1]};
        std::ifstream ifs({fname});

        if (!ifs) {
            std::cout << "IO Error\n";
            return 1;

        } else {

            // Read from file
            std::ostringstream raw;
            std::string buffer;
            while (std::getline(ifs, buffer))
                raw << buffer << "\n";

            // Convert to string
            std::string jsonStr = raw.str();

            // Parse string into JSON Root obj
            JSON::JSONNode_Ptr root = JSON::Parser::loads(jsonStr);

            // Serialize & print the result
            std::cout << JSON::Parser::dumps(root) << "\n";

        }
    }

    return 0;
}
