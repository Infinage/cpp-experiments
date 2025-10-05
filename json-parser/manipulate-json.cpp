#include "json.hpp"
#include <fstream>
#include <iostream>
#include <string>

int main() {
    std::string fpath{"test/pass03.json"}, buffer{""}, raw{""};
    std::ifstream ifs{fpath};
    while (ifs) {
        std::getline(ifs, buffer);
        raw += buffer + '\n';
    }

    // Before manipulation
    std::cout << "Original ->\n" << raw << "\n";

    JSON::JSONHandle root {JSON::Parser::loads(raw)};
    
    // Cast root into Object node
    auto &childObj {root["JSON Test Pattern pass3"].cast<JSON::JSONObjectNode>()};
    childObj.setKey("Modified JSON");
    childObj.push(JSON::helper::createNode("But it could have", "been an array as well"));

    // Serialize the output
    std::cout << "Modified ->\n" << root.str() << "\n";
}
