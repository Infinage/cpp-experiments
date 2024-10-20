/*
 * https://stackoverflow.com/questions/19543326/datatypes-for-representing-json-in-c
 */

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <numeric>
#include <stack>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

/*
 * Data structure to store the nodes in a JSON Document
 */

// Types of JSON objects
enum NODE_TYPE: short {value, array, object};

// Forward declaration
class JSONNode;

// Types of JSON Simple values
using JSON_VALUE_TYPE = std::variant<std::string, std::nullptr_t, int, double, bool>;
using JSONNode_Ptr = std::shared_ptr<JSONNode>;

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

class JSONValueNode: public JSONNode {
    private:
        JSON_VALUE_TYPE value;

    public:
        JSONValueNode(const JSON_VALUE_TYPE &v): JSONNode("", NODE_TYPE::value), value(v) {};

        JSONValueNode(const std::string &k, const JSON_VALUE_TYPE &v):
            JSONNode(k, NODE_TYPE::value), value(v) {};

        JSON_VALUE_TYPE &getValue() {
            return value;
        }
};

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

        // Iterators
        std::vector<JSONNode_Ptr>::iterator begin() { return values.begin(); }
        std::vector<JSONNode_Ptr>::iterator end() { return values.end(); }
        std::vector<JSONNode_Ptr>::const_iterator cbegin() const { return values.cbegin(); }
        std::vector<JSONNode_Ptr>::const_iterator cend() const { return values.cend(); }
};

class JSONObjectNode: public JSONNode {
    private:
        std::vector<JSONNode_Ptr> values;
        bool checkDuplicates(std::vector<JSONNode_Ptr> &v) {
            std::unordered_set<JSON_VALUE_TYPE> st;
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

        JSONNode_Ptr operator[] (std::string &k) {
            auto it = find(k);
            if (it != values.end())
                return *it;
            else
                throw std::invalid_argument("Key not found: ");
        }

        // Iterators
        std::vector<JSONNode_Ptr>::iterator begin() { return values.begin(); }
        std::vector<JSONNode_Ptr>::iterator end() { return values.end(); }
        std::vector<JSONNode_Ptr>::const_iterator cbegin() const { return values.cbegin(); }
        std::vector<JSONNode_Ptr>::const_iterator cend() const { return values.cend(); }
};


class JSONParser {
public:
    static std::pair<int, std::string> extractString(std::size_t idx, std::size_t N, const std::string& json) {
        std::string acc{""};
        idx++;
        while (idx < N && json[idx] != '"') {
            acc += json[idx++];
            if (json[idx] == '\\') {
                acc += json[idx] + json[idx + 1];
                idx += 2;
            }
        }
        return {idx + 1, acc};
    }

    static JSON_VALUE_TYPE simple_parse(std::string &token) {
        // string, nullptr_t, int, double, bool
        std::vector<bool> isDigit;
        std::transform(token.begin(), token.end(), std::back_inserter(isDigit), [](char ch) { return std::isdigit(ch); });
        std::size_t digitCount = std::accumulate(isDigit.begin(), isDigit.end(), (std::size_t) 0, [](std::size_t sum, bool b) {
                return sum + (b? 1: 0);
        });
        if (token == "null")
            return nullptr;
        else if (token == "true" || token == "false")
            return token == "true";
        else if (token[0] == '"' && token.back() == '"')
            return token.substr(1, token.size() - 2);
        else if (digitCount == token.size() || (digitCount == token.size() - 1 && token[0] == '-'))
            return std::stoi(token);
        else if (
                (digitCount == token.size() - 1 && token.find('.') != std::string::npos) ||
                (digitCount == token.size() - 2 && token.find('.') != std::string::npos && token[0] == '-')
            )
            return std::stod(token);
        else if (token.find_first_of("eE") != std::string::npos) {
            try {
                return std::stod(token);
            } catch (const std::invalid_argument& e) {
                throw std::invalid_argument("Invalid scientific notation: " + token);
            }
        }
        else
            throw std::invalid_argument("Invalid value: " + token + "; Digit Count: " + std::to_string(digitCount));
    }

    static JSONNode_Ptr loads(std::string &raw) {
        std::unordered_set<char> splChars{'{', '}', '[', ']', ',', ':'};
        std::string acc{""};
        std::stack<std::pair<char, std::size_t>> validateStk;
        std::stack<std::variant<std::string, JSONNode_Ptr>> tokens;
        bool processingString = false, currCharEscaped = false;
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
                else if (currCharEscaped)
                    currCharEscaped = false;

            } else if (!std::isspace(ch)) {
                if (!std::isspace(ch) && splChars.find(ch) == splChars.end())
                    acc += ch;
                else if (acc.size()) {
                    tokens.push(acc);
                    acc.clear();
                }

                if ((ch == '}' && validateStk.top().first != '{') || (ch == ']' && validateStk.top().first != '['))
                    throw std::logic_error("Invalid JSON");
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
                            values.push_back(std::make_shared<JSONValueNode>(simple_parse(std::get<std::string>(valMixed))));
                        tokens.pop();

                        std::string key {""};
                        if (prevCh == '{') {
                            key = std::get<std::string>(tokens.top());
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

                }
            }
        }

        return std::get<JSONNode_Ptr>(tokens.top());
    }

    /* Helper function to print JSON_VALUE_TYPE obj */
    static std::string simple_format (JSON_VALUE_TYPE &v) {
        // string, nullptr_t, int, double, bool
        if (std::holds_alternative<std::string>(v))
            return "\"" + std::get<std::string>(v) + "\"";
        else if (std::holds_alternative<std::nullptr_t>(v))
            return "null";
        else if (std::holds_alternative<int>(v))
            return std::to_string(std::get<int>(v));
        else if (std::holds_alternative<double>(v))
            return std::to_string(std::get<double>(v));
        else
            return std::get<bool>(v)? "true": "false";
    }

    static std::string dumps(JSONNode_Ptr root, bool ignoreKeys = true) {
        if (root == nullptr)
            return "";

        else {
            std::string keyStr = {ignoreKeys? "": "\"" + root->getKey() + "\": "};

            if (root->getType() == NODE_TYPE::value) {
                JSONValueNode &v = static_cast<JSONValueNode&>(*root);
                return keyStr + simple_format(v.getValue());
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

int main(int argc, char* argv[]) {

    /*
    // Sample JSON Document creation //
    // Array
    JSONNode_Ptr one     = std::make_shared<JSONValueNode>(1);
    JSONNode_Ptr two     = std::make_shared<JSONValueNode>(2);
    JSONNode_Ptr three   = std::make_shared<JSONValueNode>(3);
    std::vector<JSONNode_Ptr> arrValues{one, two, three};
    JSONNode_Ptr arr_    = std::make_shared<JSONArrayNode>("array", arrValues);

    // Simple Data types
    JSONNode_Ptr bool_   = std::make_shared<JSONValueNode>("boolean", true);
    JSONNode_Ptr null_   = std::make_shared<JSONValueNode>("null", nullptr);
    JSONNode_Ptr int_    = std::make_shared<JSONValueNode>("number", 123);
    JSONNode_Ptr float_  = std::make_shared<JSONValueNode>("float", 1.0);
    JSONNode_Ptr string_ = std::make_shared<JSONValueNode>("string", "Hello world");

    // Object
    JSONNode_Ptr a       = std::make_shared<JSONValueNode>("a", "b");
    JSONNode_Ptr c       = std::make_shared<JSONValueNode>("c", "d");
    JSONNode_Ptr empty   = std::make_shared<JSONValueNode>("", "empty");
    std::vector<JSONNode_Ptr> objValues{a, c, empty};
    JSONNode_Ptr obj_    = std::make_shared<JSONObjectNode>("object", objValues);

    // Create the root object
    std::vector<JSONNode_Ptr> rootValues{arr_, bool_, null_, int_, float_, string_, obj_};
    JSONNode_Ptr root    = std::make_shared<JSONObjectNode>(rootValues);

    // Serialize & print the result
    std::cout << JSONParser::dumps(root) << "\n";
    */

    if (argc != 2)
        std::cout << "Usage: ./json.out <filepath>\n";

    else {

        std::string fname{argv[1]};
        std::ifstream ifs({fname});

        if (!ifs) {
            std::cout << "IO Error\n";
            return 1;

        } else {

            std::string raw{""}, buffer;
            while (ifs) {
                std::getline(ifs, buffer);
                raw += buffer + "\n";
            }

            // Parse string into JSON Root obj
            JSONNode_Ptr root = JSONParser::loads(raw);

            // Serialize & print the result
            std::cout << JSONParser::dumps(root) << "\n";

        }
    }

    return 0;
}
