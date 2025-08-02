#include "../json-parser/json.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>

namespace Bencode {
    std::string encode(JSON::JSONNodePtr root) {
        if (!root) return "";
        else {
            std::ostringstream oss;
            const std::string &key {root->getKey()};
            if (!key.empty()) oss << key.size() << ':' << key;
            switch (root->getType()) {
                case JSON::NodeType::value: {
                    // Only allow string / long
                    auto &val {static_cast<JSON::JSONValueNode&>(*root).getValue()};
                    bool isValid {std::visit([](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;
                        return std::is_same_v<T, std::string> || std::is_same_v<T, long>;
                    }, val)};

                    if (!isValid) 
                        throw std::runtime_error("Encoder got a non string / int");
                    else if (auto *ptr {std::get_if<std::string>(&val)})
                        oss << ptr->size() << ':' << *ptr;
                    else
                        oss << 'i' << std::get<long>(val) << 'e';
                    break;
                }

                case JSON::NodeType::array: {
                    oss << 'l';
                    for (auto &node: static_cast<JSON::JSONArrayNode&>(*root))
                        oss << encode(node);
                    oss << 'e';
                    break;
                }

                case JSON::NodeType::object: {
                    oss << 'd';
                    for (auto &node: static_cast<JSON::JSONObjectNode&>(*root))
                        oss << encode(node);
                    oss << 'e';
                    break;
                }
            }
            return oss.str();
        }
    }

    JSON::JSONNodePtr decode(const std::string &encoded, bool ignoreSpaces = true) {
        // Extract top and insert it into its ancestor
        auto extract_Push_to_ancestor { [](std::stack<JSON::JSONNodePtr> &stk) { 
            assert(stk.size() > 1);
            auto prevPtr {std::move(stk.top())}; stk.pop();
            if (stk.top()->getType() == JSON::NodeType::value)
                throw std::runtime_error("Invalid bencoded string");
            else if (stk.top()->getType() == JSON::NodeType::object)
                static_cast<JSON::JSONObjectNode&>(*stk.top()).push(std::move(prevPtr));
            else
                static_cast<JSON::JSONArrayNode&>(*stk.top()).push(std::move(prevPtr));
        }};

        // ** Before use ensure that stk top is a simple value node **
        // Used when we have received both key and value
        // We want to set value of parent and if the encoded string
        // is not a simple standalone, extract the parent and insert 
        // it back to its ancestor
        auto pop_Setval_Extract_Push { 
        [&extract_Push_to_ancestor] (std::stack<JSON::JSONNodePtr> &stk, const auto &val) {
            assert(stk.top()->getType() == JSON::NodeType::value);
            JSON::JSONValueNode &prev {static_cast<JSON::JSONValueNode&>(*stk.top())};
            if (!std::holds_alternative<std::nullptr_t>(prev.getValue())) 
                throw std::runtime_error("Invalid bencoded string");

            // Set the value for the existing simplevaluenode (key already set)
            prev.setValue(val);

            // Remove parent and insert into its ancestor (object / array)
            // If size == 1, we assume it is a standalone case
            if (stk.size() > 1) extract_Push_to_ancestor(stk);
        }};

        std::stack<JSON::JSONNodePtr> stk;
        std::size_t idx {}, N {encoded.size()};
        while (idx < N) {
            char ch {encoded[idx]};
            // An object (dict) must have a string key only. Cannot have array / dict as keys
            if ((ch == 'd' || ch == 'l') && (!stk.empty() && stk.top()->getType() == JSON::NodeType::object))
                throw std::runtime_error("Invalid bencoded string");

            else if (ch == 'd') { stk.push(JSON::helper::createObject("", {})); } 
            else if (ch == 'l') { stk.push(JSON::helper::createArray({})); } 

            // Inserting an integer
            // No parent -> Insert as standalone {"", val}
            // Parent is an object -> invalid; int as keys is disallowed
            // Parent is an array -> insert into parent as {"", val}
            // Parent is simple obj -> set int as parents value
            //    If there are more than 1 ancestors in chain
            //    Extract parent and insert back into its parent 
            else if (ch == 'i') {
                std::size_t endP {encoded.find('e', idx)};
                if (endP == std::string::npos || (!stk.empty() && stk.top()->getType() == JSON::NodeType::object))
                    throw std::runtime_error("Invalid bencoded string");

                long val {std::stol(encoded.substr(idx + 1, endP))};
                if (stk.empty())
                    stk.push(JSON::helper::createNode(val));
                else if (stk.top()->getType() == JSON::NodeType::array)
                    static_cast<JSON::JSONArrayNode&>(*stk.top()).push(JSON::helper::createNode(val));
                else // top == simplevaluenode
                    pop_Setval_Extract_Push(stk, val);
                idx = endP;
            } 

            // Inserting a string
            // No parent -> Insert as standalone {"", val}
            // Parent is an object -> insert as {key, ""} onto stack (wait for value)
            // Parent is an array -> insert into parent as {"", val}
            // Parent is simple obj -> set str as parents value
            //    If there are more than 1 ancestors in chain
            //    Extract parent and insert back into its parent 
            else if (std::isdigit(ch)) {
                std::size_t numEndP {encoded.find(':', idx)};
                if (numEndP == std::string::npos)
                    throw std::runtime_error("Invalid bencoded string");

                std::size_t strLen {std::stoull(encoded.substr(idx, numEndP))};
                if (numEndP + 1 + strLen > N) 
                    throw std::runtime_error("Invalid bencoded string");

                std::string str {encoded.substr(numEndP + 1, strLen)};
                if (stk.empty())
                    stk.push(JSON::helper::createNode(str));
                else if (stk.top()->getType() == JSON::NodeType::object)
                    stk.push(JSON::helper::createNode(str, nullptr));
                else if (stk.top()->getType() == JSON::NodeType::array)
                    static_cast<JSON::JSONArrayNode&>(*stk.top()).push(JSON::helper::createNode(str));
                else
                    pop_Setval_Extract_Push(stk, str); 
                idx = numEndP + strLen;
            }

            // End marker
            // Loop typically ends here unless this is a standalone case
            else if (ch == 'e') {
                if (stk.empty() || (idx + 1 == N && stk.size() != 1) || (stk.size() == 1 && idx + 1 < N))
                    throw std::runtime_error("Invalid bencoded string");

                auto prev {std::move(stk.top())}; stk.pop();
                if (idx + 1 == N && stk.empty())
                    return prev;
                else if (stk.top()->getType() == JSON::NodeType::array)
                    static_cast<JSON::JSONArrayNode&>(*stk.top()).push(std::move(prev));
                else if (stk.top()->getType() == JSON::NodeType::object)
                    static_cast<JSON::JSONObjectNode&>(*stk.top()).push(std::move(prev));
                else {
                    prev->setKey(stk.top()->getKey()); 
                    stk.pop(); stk.push(prev);
                    if (stk.size() > 1) extract_Push_to_ancestor(stk);
                }
            }

            else if (!std::isspace(ch) || !ignoreSpaces) {
                throw std::runtime_error("Invalid bencoded string");
            }

            ++idx;
        }

        // Loop typically ends when ch == 'e', this is to support standalone values
        if (stk.size() != 1) throw std::runtime_error("Invalid bencoded string");
        return std::move(stk.top());
    }
}

int main() {
    /*
    JSON::JSONNodePtr root {
            JSON::helper::createObject({
            JSON::helper::createNode("announce", "http://bttracker.debian.org:6969/announce"), 
            JSON::helper::createNode("comment", "Debian CD from cdimage.debian.org"), 
            JSON::helper::createNode("creation date", 1573903810),
            JSON::helper::createObject("info", {
                JSON::helper::createNode("length", 351272960),
                JSON::helper::createNode("name", "debian-10.2.0-amd64-netinst.iso"),
                JSON::helper::createNode("piece length", 262144),
                JSON::helper::createNode("pieces", "......."),
            })
        })
    };

    const std::string encoded {Bencode::encode(root)};
    std::cout << JSON::helper::pretty(JSON::Parser::dumps(root)) << "\n\n";
    std::cout << encoded << "\n\n";
    std::cout << JSON::helper::pretty(JSON::Parser::dumps(Bencode::decode(encoded))) << "\n\n";
    */

    ///*
    std::ifstream ifs {"alpine.iso.torrent", std::ios::binary};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string encoded {oss.str()};
    auto rootPtr {Bencode::decode(encoded)};
    std::string decoded {JSON::Parser::dumps(rootPtr)};
    std::cout << decoded << '\n';
    std::cout << JSON::helper::pretty(decoded) << "\n\n";
    //*/
}
