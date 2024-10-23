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
    std::cout << raw << "\n";

    JSON::JSONNode_Ptr root {JSON::Parser::loads(raw)};
    
    // Cast root into Object node
    JSON::JSONObjectNode &rootObj {static_cast<JSON::JSONObjectNode&>(*root)};
    JSON::JSONObjectNode &childObj {static_cast<JSON::JSONObjectNode&>(*rootObj["JSON Test Pattern pass3"])};
    childObj.setKey("Modify JSON");
    childObj.push(JSON::helper::createNode("But it could have", "been an array as well"));

    // Serialize the output
    std::string processed {JSON::Parser::dumps(root)};
    std::cout << JSON::helper::pretty(processed) << "\n";
}
