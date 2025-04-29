#include <iostream>
#include <memory>
#include "ordered_map.hpp"

/*
 * - Does not provide XSD validation
 * - Does not preserve whitespaces or comments
 * - Implement move constructor
 */

namespace XMLUtil {

    /*
     * Both CDATA and ordinary nodes can be represented via `XMLNode`
     */
    class XMLNode {
        private:
            XMLNode* parent {nullptr};
            std::list<std::unique_ptr<XMLNode>> children;
            const std::string name;
            stdx::ordered_map<std::string, std::string> attrs;
            std::optional<std::string> text;
            bool raw {false};

        public:
            static XMLNode TextNode(const std::string &text, bool raw) {
                XMLNode res; res.setText(text);
                if (raw) res.setRaw();
                return res;
            }

            XMLNode(const std::string &name = ""): name(name) {}

            std::string getName() const { return name; }

            std::string getText() const { return text.value_or(""); }

            bool isRaw() const { return raw; }

            XMLNode &setRaw() { raw = true; return *this; }

            XMLNode &setText(const std::string &text) { 
                this->text = text; return *this; 
            }

            std::optional<std::string> getAttr(const std::string &key) const { 
                stdx::ordered_map<std::string, std::string>::const_iterator it {attrs.find(key)};
                if (it == attrs.end()) return std::nullopt;
                else return it->second;
            }

            XMLNode &setAttr(const std::string &key, const std::string &val) { 
                attrs[key] = val; return *this; 
            }

            XMLNode &addChild(std::unique_ptr<XMLNode> &&child) {
                children.emplace_back(std::move(child));
                children.back()->parent = this;
                return *this;
            }

            XMLNode &addChild(XMLNode &&child) {
                addChild(std::make_unique<XMLNode>(std::move(child)));
                return *this;
            }

            template<typename ...Node> 
            requires ((std::is_same_v<std::decay_t<Node>, XMLNode> || 
                       std::is_same_v<std::decay_t<Node>, std::unique_ptr<XMLNode>>) && ...)
            XMLNode &addChildren(Node &&...children) {
                (addChild(std::move(children)), ...);
                return *this;
            }

            void unlink() {
                if (parent != nullptr) {
                    std::list<std::unique_ptr<XMLNode>> &siblings {parent->children};
                    for (std::list<std::unique_ptr<XMLNode>>::iterator it {siblings.begin()}; it != siblings.end();) {
                        if (it->get() != this) ++it;
                        else it = siblings.erase(it);
                    }
                }
            }

            friend std::ostream &operator<<(std::ostream &os, const XMLNode& node) {
                if (node.raw) os << "<![CDATA[" << node.getText() << "]]>";
                else if (node.name.empty()) os << node.getText();
                else {
                    // <TagName
                    os << '<' << node.name;

                    // Attributes
                    for (const std::pair<std::string, std::string> &kv: node.attrs)
                        os << ' ' << kv.first << "=\"" << kv.second << '"';

                    // Self closing Tag
                    if (node.children.empty() && !node.text) os << "/>";

                    // Tag with children or text
                    else {
                        os << '>';
                        if (node.text) os << *node.text;
                        for (const std::unique_ptr<XMLNode> &child: node.children)
                            os << *child;
                        os << "</" << node.name << '>';
                    }
                }
                return os;
            }
    };
};

int main() {
    using namespace XMLUtil;
    XMLNode root {"paragraph"};
    root.setAttr("attr", "hello");
    XMLNode text1 {XMLNode::TextNode("This is ", false)};
    XMLNode b1 {"b"}; b1.setText("bold");
    XMLNode text2 {XMLNode::TextNode(" and ", false)};
    XMLNode i1 {"i"}; i1.setText("italic");
    XMLNode text3 {XMLNode::TextNode(" text with ", false)};
    XMLNode u1 {"u"}; u1.setText("underline");
    root.addChildren(text1, b1, text2, i1, text3, u1);
    std::cout << root << '\n';
    return 0;
}
