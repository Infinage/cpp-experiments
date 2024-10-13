#include <iostream>
#include <fstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

int main(int argc, char* argv[]) {
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

            std::size_t N = json.size();

            // Symbols
            std::vector<std::string> tokens;
            std::unordered_map<char, char> bracePairs({{'{', '}'}, {'[', ']'}});
            std::stack<char> symStk;
            std::size_t idx = 0;
            while (idx < json.size()) {

                // Process Strings - keys, values, list items
                if (json[idx] == '"') {

                    buffer.clear();
                    idx++;
                    while (idx < N && json[idx] != '"') {
                        buffer += json[idx++];
                        if (idx < N && buffer.back() == '\\')
                            buffer += json[idx++];
                    }
                    tokens.push_back(buffer);
                    idx++;
                    buffer.clear();

                }

                else if (json[idx] == '{' || json[idx] == '[') {
                    symStk.push(json[idx++]);
                }

                else if (json[idx] == '}' || json[idx] == ']') {
                    if (symStk.empty() || bracePairs[symStk.top()] != json[idx]) {
                        std::cout << "Malformed json\n";
                        return 1;
                    } else
                        symStk.pop();
                    idx++;
                }

                else {
                    idx++;
                }
            }

            for (std::string& tok: tokens)
                std::cout << "Token: " << tok << "\n";

        }
    }
    return 0;
}
