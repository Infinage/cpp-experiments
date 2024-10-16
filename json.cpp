/*
 * https://stackoverflow.com/questions/19543326/datatypes-for-representing-json-in-c
 */

#include <cctype>
#include <cstddef>
#include <exception>
#include <iostream>
#include <fstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>
#include <unordered_map>>

/*
 * Data structure to store the nodes in a JSON Document
 */

// Types of JSON objects
enum NODE_TYPE: short {value, array, object};

// Types of JSON Simple values
using JSON_VALUE_TYPE = std::variant<std::string, std::nullptr_t, int, double, bool>;

class JSONNode {
    private:
        std::string key;
        const NODE_TYPE type;

    public:
        JSONNode(const std::string &k, NODE_TYPE t):
            key(k), type(t) {};

        NODE_TYPE getType() {
            return type;
        }

        std::string getKey() {
            return key;
        }
};

class JSONValueNode: JSONNode {
    private:
        JSON_VALUE_TYPE value;

    public:
        JSONValueNode(const std::string &k, const JSON_VALUE_TYPE &v):
            JSONNode(k, NODE_TYPE::value), value(v) {};
};

class JSONArrayNode: JSONNode {
    private:
        std::vector<JSONNode> values;

    public:
        JSONArrayNode(const std::string &k): JSONNode(k, NODE_TYPE::array) {};

        void push(JSONNode node) {
            values.push_back(node);
        }

        JSONNode pop() {
            JSONNode result = values.back();
            values.pop_back();
            return result;
        }

        const JSONNode &operator[] (std::size_t idx) {
            if (idx < values.size())
                return values[idx];
        }
};

class JSONObjectNode: JSONNode {
    private:
        std::vector<JSONNode> values;

    public:
        JSONObjectNode(const std::string &k): JSONNode(k, NODE_TYPE::object) {};

        bool exists(std::string &k) {
            for (JSONNode &node: values) {
                if (node.getKey() == k)
                    return true;
            }
            return false;
        }

        const JSONNode &operator[] (std::string &k) {
            for (JSONNode &node: values) {
                if (node.getKey() == k)
                    return node;
            }
        }
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
};

int main(int argc, char* argv[]) {
    using JSONCombined = std::variant<JSONSimple, JSONCompound>;

    if (argc <= 1)
        std::cout << "Usage: ./json.out <filename>\n";
    else {
        std::string fname {argv[1]};
        std::ifstream ifs({fname});

        if (!ifs) {
            std::cout << "IO Error!";
            return 1;
        } else {

            // TODO: Read piece by piece
            std::string json {""}, buffer;
            while (ifs) {
                std::getline(ifs, buffer);
                json += buffer + "\n";
            }

            std::size_t N = json.size(), i {0};
            std::stack<JSONCombined> stk;
            buffer.clear();
            while (i < N){
                if (json[i] == '{' || json[i] == '[')
                    stk.push(std::string{json[i]});
                else if (json[i] == '"') {
                    std::tie(i, buffer) = JSONParser::extractString(i, N, json);
                    stk.push(buffer);
                } else if (json[i] == '}') {
                    JSONCombined topElem = stk.top();
                    stk.pop();
                    std::map<JSONSimple, JSONCompound> mpp;
                    while (topElem != nullptr && topElem != "{") {
                        JSONCombined key, value; 
                        value = stk.top();
                        stk.pop();
                        key = stk.top();
                        stk.pop();
                        mpp[key] = value;
                    }
                    stk.push(mpp);
                } else if (json[i] == ']') {
                    auto topElem = std::get_if<JSONSimple>(&stk.top());
                    std::vector<JSONSimple> vec;
                    while (topElem != nullptr && *topElem != "{") {
                        JSONCombined curr = stk.top();
                        vec.push_back(curr);
                    }
                }
                i++;
            }

        }
    }
    return 0;
}
