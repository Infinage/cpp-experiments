/*
 * https://stackoverflow.com/questions/19543326/datatypes-for-representing-json-in-c
 */

#include <cctype>
#include <cstddef>
#include <iostream>
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

// Types of JSON Simple values
using JSON_VALUE_TYPE = std::variant<std::string, std::nullptr_t, int, double, bool>;

class JSONNode {
    private:
        JSON_VALUE_TYPE key;
        NODE_TYPE type;

    public:
        JSONNode(const JSON_VALUE_TYPE &k, NODE_TYPE t):
            key(k), type(t) {};

        NODE_TYPE getType() {
            return type;
        }

        JSON_VALUE_TYPE &getKey() {
            return key;
        }
};

class JSONValueNode: public JSONNode {
    private:
        JSON_VALUE_TYPE value;

    public:
        JSONValueNode(const JSON_VALUE_TYPE &v): JSONNode("", NODE_TYPE::value), value(v) {};

        JSONValueNode(const JSON_VALUE_TYPE &k, const JSON_VALUE_TYPE &v):
            JSONNode(k, NODE_TYPE::value), value(v) {};

        JSON_VALUE_TYPE &getValue() {
            return value;
        }
};

class JSONArrayNode: public JSONNode {
    private:
        std::vector<JSONNode*> values;

    public:
        JSONArrayNode(const JSON_VALUE_TYPE &k): JSONNode(k, NODE_TYPE::array) {};
        JSONArrayNode(const JSON_VALUE_TYPE &k, std::vector<JSONNode*> &v): JSONNode(k, NODE_TYPE::array), values(v) {};
        JSONArrayNode(std::vector<JSONNode*> &v): JSONNode("", NODE_TYPE::array), values(v) {};

        std::size_t size() {
            return values.size();
        }

        void push(JSONNode *node) {
            values.push_back(node);
        }

        JSONNode *pop() {
            JSONNode *result = values.back();
            values.pop_back();
            return result;
        }

        const JSONNode *operator[] (std::size_t idx) {
            if (idx < values.size())
                return values[idx];
            else
                throw std::invalid_argument("Out of bounds");
        }

        // Iterators
        std::vector<JSONNode*>::iterator begin() { return values.begin(); }
        std::vector<JSONNode*>::iterator end() { return values.end(); }
        std::vector<JSONNode*>::const_iterator cbegin() const { return values.cbegin(); }
        std::vector<JSONNode*>::const_iterator cend() const { return values.cend(); }
};

class JSONObjectNode: public JSONNode {
    private:
        std::vector<JSONNode*> values;
        bool checkDuplicates(std::vector<JSONNode*> &v) {
            std::unordered_set<JSON_VALUE_TYPE> st;
            for (JSONNode *ele: v) {
                if (st.find(ele->getKey()) != st.end())
                    return false;
                else
                    st.insert(ele->getKey());
            }
            return true;
        }

    public:
        JSONObjectNode(const JSON_VALUE_TYPE &k): JSONNode(k, NODE_TYPE::object) {};

        JSONObjectNode(std::vector<JSONNode*> &v): JSONNode("", NODE_TYPE::object) {
            if (!checkDuplicates(v))
                throw std::invalid_argument("Duplicate key found");
            values = v;
        }

        JSONObjectNode(const JSON_VALUE_TYPE &k, std::vector<JSONNode*> &v): JSONNode(k, NODE_TYPE::object) {
            if (!checkDuplicates(v))
                throw std::invalid_argument("Duplicate key found");
            values = v;
        }

        std::size_t size() {
            return values.size();
        }

        void push(JSONNode *node) {
            auto it = find(node->getKey());
            if (it == values.end())
                values.push_back(node);
            else
                values[(std::size_t)(it - values.begin())] = node;
        }

        std::vector<JSONNode*>::iterator find(JSON_VALUE_TYPE &k) {
            for (std::vector<JSONNode*>::iterator it = values.begin(); it < values.end(); it++) {
                if ((*it)->getKey() == k)
                    return it;
            }
            return values.end();
        }

        const JSONNode *operator[] (JSON_VALUE_TYPE &k) {
            auto it = find(k);
            if (it != values.end())
                return *it;
            else
                throw std::invalid_argument("Key not found: ");
        }

        // Iterators
        std::vector<JSONNode*>::iterator begin() { return values.begin(); }
        std::vector<JSONNode*>::iterator end() { return values.end(); }
        std::vector<JSONNode*>::const_iterator cbegin() const { return values.cbegin(); }
        std::vector<JSONNode*>::const_iterator cend() const { return values.cend(); }
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

    static std::string dumps(JSONNode &root, bool ignoreKeys = true) {

        auto simple_format = [] (JSON_VALUE_TYPE &v) -> std::string {
            // string, nullptr_t, int, double, bool
            if (std::get_if<std::string>(&v) != nullptr)
                return "\"" + std::get<std::string>(v) + "\"";
            else if (std::get_if<std::nullptr_t>(&v) != nullptr)
                return "null";
            else if (std::get_if<int>(&v) != nullptr)
                return std::to_string(std::get<int>(v));
            else if (std::get_if<double>(&v) != nullptr)
                return std::to_string(std::get<double>(v));
            else
                return std::get<bool>(v)? "true": "false";
        };

        std::string keyStr = {ignoreKeys? "": simple_format(root.getKey()) + ": "};

        if (root.getType() == NODE_TYPE::value) {
            JSONValueNode &v = static_cast<JSONValueNode&>(root);
            return keyStr + simple_format(v.getValue());
        }

        else if (root.getType() == NODE_TYPE::array) {
            std::string result {keyStr + "["};
            JSONArrayNode &v = static_cast<JSONArrayNode&>(root);
            for (JSONNode *nxt: v)
                result += dumps(*nxt, true) + ", ";
            result += v.size() > 0?"\b\b]": "]";
            return result;
        }

        else {
            std::string result {keyStr + "{"};
            JSONObjectNode &v = static_cast<JSONObjectNode&>(root);
            for (JSONNode *nxt: v)
                result += dumps(*nxt, false) + ", ";
            result += v.size() > 0?"\b\b}": "}";
            return result;
        } 
    }
};

int main() {

    /* Sample JSON Document creation */

    // Array
    JSONValueNode one{1};
    JSONValueNode two{2};
    JSONValueNode three{3};
    std::vector<JSONNode*> arrValues{&one, &two, &three};
    JSONArrayNode arr_{"array", arrValues};

    // Simple Data types
    JSONValueNode bool_{"boolean", true};
    JSONValueNode null_{"null", nullptr};
    JSONValueNode int_{"number", 123};
    JSONValueNode float_{"float", 1.0};
    JSONValueNode string_{"string", "Hello world"};

    // Object
    JSONValueNode a{"a", "b"};
    JSONValueNode c{"c", "d"};
    std::vector<JSONNode*> objValues{&a, &c};
    JSONObjectNode obj_{"object", objValues};

    // Create the root object
    std::vector<JSONNode*> rootValues{&arr_, &bool_, &null_, &int_, &float_, &string_, &obj_};
    JSONObjectNode root{rootValues};

    // Serialize & print the result
    std::cout << JSONParser::dumps(root) << "\n";

    /* End of sample Document creation */

    return 0;
}
