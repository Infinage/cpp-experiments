#include <cctype>
#include <cstddef>
#include <iostream>
#include <fstream>
#include <stack>
#include <string>
#include <variant>
#include <vector>
#include <map>

using JSONSimple = std::variant<std::nullptr_t, std::string, int, double, bool>;
using JSONCompound = std::variant<std::vector<JSONSimple>, std::map<JSONSimple, JSONSimple>>;

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
    using JSONCombined = std::variant<std::string, JSONSimple, JSONCompound>;

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
                    auto topElem = std::get_if<std::string>(&stk.top());
                    std::map<JSONSimple, JSONSimple> mpp;
                    while (topElem != nullptr && *topElem != "{") {
                        JSONCombined key, value; 
                        value = stk.top();
                        stk.pop();
                        key = stk.top();
                        stk.pop();
                        mpp[key] = value;
                    }
                } else if (json[i] == ']') {
                    auto topElem = std::get_if<std::string>(&stk.top());
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
