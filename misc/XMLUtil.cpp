#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <ranges>
#include <sstream>
#include <stack>
#include <stdexcept>
#include "ordered_map.hpp"

/*
 * - Does not provide XSD validation
 * - Does not preserve whitespaces or comments
 *
 *   TODO:
 *   - Support for &apos, &quot, etc
 *   - Support namespace validation
 *   - API for navigating tree
 */

namespace XMLUtil {
    /*
     * Both CDATA, PI & Ordinary nodes can be represented via `XMLNode`
     */
    template<typename T>
    bool isIn(T val, const std::initializer_list<T> &options) {
        return std::ranges::find(options, val) != options.end();
    }

    std::string trim(const std::string &str) {
        return 
            str 
            | std::views::drop_while(::isspace) 
            | std::views::reverse 
            | std::views::drop_while(::isspace) 
            | std::views::reverse
            | std::ranges::to<std::string>();
    }

    class XMLNode {
        public:
            using NodePtr = std::shared_ptr<XMLNode>;
            enum class NodeType {CDATA, TEXT, NODE, PI};

            std::string getName() const { return name; }
            std::string getText() const { return text.value_or(""); }
            NodeType getType() const { return type; }
            void setText(const std::string &text) { this->text = text; }

            std::optional<std::string> getAttr(const std::string &key) const { 
                stdx::ordered_map<std::string, std::string>::const_iterator it {attrs.find(key)};
                if (it == attrs.end()) return std::nullopt;
                else return it->second;
            }

            void setAttr(const std::string &key, const std::string &val) { attrs[key] = val; }
            void setAttrs(stdx::ordered_map<std::string, std::string> &&attrs) { 
                this->attrs = std::move(attrs); 
            }

            void addChild(const NodePtr &child) {
                children.emplace_back(child);
                children.back()->parent = this;
            }

            template<typename ...Node> 
            requires ((std::is_same_v<std::decay_t<Node>, NodePtr>) && ...)
            void addChildren(Node &&...children) { (addChild(std::move(children)), ...); }

            void unlink() {
                if (parent != nullptr) {
                    std::list<NodePtr> &siblings {parent->children};
                    for (std::list<NodePtr>::iterator it {siblings.begin()}; it != siblings.end();) {
                        if (it->get() != this) ++it;
                        else it = siblings.erase(it);
                    }
                }
            }

            std::string toString(unsigned int level = 0) const {
                std::ostringstream oss;
                switch (type) {
                    case NodeType::CDATA: {
                        oss << "<![CDATA[" << getText() << "]]>";
                        break;
                    }

                    case NodeType::TEXT: {
                        oss << getText();
                        break;
                    }

                    case NodeType::PI: {
                        // <TagName
                        oss << "<?" << name;

                        // Attributes
                        for (const std::pair<std::string, std::string> &kv: attrs)
                            oss << ' ' << kv.first << "=\"" << kv.second << '"';

                        // Close tag
                        oss << "?>";
                        break;
                    }

                    case NodeType::NODE: {
                        // <TagName
                        oss << '<' << name;

                        // Attributes
                        for (const std::pair<std::string, std::string> &kv: attrs)
                            oss << ' ' << kv.first << "=\"" << kv.second << '"';

                        // Self closing Tag
                        if (children.empty() && !text) oss << "/>";

                        // Tag with children or text
                        else {
                            std::string nlIndent{'\n' + std::string(level, '\t')};
                            oss << '>';
                            if (text) oss << nlIndent << '\t' << *text;
                            for (const NodePtr &child: children)
                                oss << nlIndent << '\t' << child->toString(level + 1);
                            oss << nlIndent << "</" << name << '>';
                        }
                        break;
                    }
                }
                return oss.str();
            }

            friend std::ostream &operator<<(std::ostream &os, const XMLNode& node) {
                os << node.toString(); return os;
            }

            friend std::ostream &operator<<(std::ostream &os, const NodePtr& node) {
                os << node->toString(); return os;
            }

            // Helper to return shared_ptr<XMLNode> - Node
            static NodePtr Node(const std::string &name, const NodeType &type = NodeType::NODE) {
                // Helper to allow shared_ptr to indirectly invoke private constructor
                struct make_shared_enabler: public XMLNode {
                    make_shared_enabler(const std::string &name, const NodeType &type): 
                        XMLNode(name, type) {}
                };

                return std::make_shared<make_shared_enabler>(name, type);
            }

            // Helper to return shared_ptr<XMLNode> - Text only
            static NodePtr TextNode(const std::string &text) {
                XMLNode::NodePtr node {Node("", NodeType::TEXT)};
                node->setText(text); return node;
            }

            // Helper to return shared_ptr<XMLNode> - CData Node
            static NodePtr CDataNode(const std::string &text) {
                XMLNode::NodePtr node {Node("", NodeType::CDATA)};
                node->setText(text); return node;
            }

        private:
            XMLNode* parent {nullptr};
            std::list<NodePtr> children;
            const std::string name; const NodeType type;
            stdx::ordered_map<std::string, std::string> attrs;
            std::optional<std::string> text;

            // Private constructor
            XMLNode(const std::string &name, const NodeType &type): 
                name(name), type(type) {}
    };

    class XMLDeclaration {
        public:
            std::string version {"1.0"};
            std::string encoding {"UTF-8"};
            std::string standalone {"yes"};

            friend std::ostream &operator<<(std::ostream &os, const XMLDeclaration &dec) {
                os << "<?xml version=\"" << dec.version 
                   << "\" encoding=\"" << dec.encoding 
                   << "\" standalone=\"" << dec.standalone 
                   << "\"?>";
                return os;
            }
    };

    class XMLTree {
        private:
            // Helper to extract name, attrs from content - `whitespaceTest attr = "spaced "  another="value"`
            static std::pair<std::string, stdx::ordered_map<std::string, std::string>>
            extractNodeInfo (std::string_view sv) {
                auto extractKey {[&sv] -> std::pair<std::string, std::string> {
                    if (!std::isalpha(sv.front())) 
                        throw std::runtime_error("Malformed XML");

                    // Extract key as Namespace:Key
                    std::string NS, Key;
                    std::size_t i {0}, N {sv.size()};
                    while (i < N && (std::isalnum(sv.at(i)) || isIn(sv.at(i), {'-', '_'}))) ++i;
                    if (i == N) 
                        NS = "", Key = std::string{sv};
                    else if (std::isspace(sv.at(i)) || sv.at(i) == '=') 
                        NS = "", Key = std::string{sv.substr(0, i)};
                    else if (sv.at(i) == ':') {
                        NS = sv.substr(0, i); std::size_t j {i + 1};
                        while (j < N && (std::isalnum(sv.at(j)) || isIn(sv.at(j), {'-', '_'}))) ++j;
                        Key = sv.substr(i + 1, j - i - 1); i = j;
                        if (Key.empty()) throw std::runtime_error("Malformed XML");
                    } else throw std::runtime_error("Malformed XML");
                    
                    // Update SV
                    while (i < N && std::isspace(sv.at(i))) ++i;
                    sv = sv.substr(i);
                    return {NS, Key};
                }};

                auto extractValue {[&sv] -> std::string {
                    if (!isIn(sv.at(0), {'\'', '"'}))
                        throw std::runtime_error("Malformed XML");
                    std::size_t i {1}, N {sv.size()};
                    while (i < N && sv.at(i) != sv.at(0)) ++i;
                    if (i == N) throw std::runtime_error("Malformed XML");
                    std::string result {sv.substr(1, i - 1)}; ++i;

                    // Update SV
                    while (i < N && std::isspace(sv.at(i))) ++i;
                    sv = sv.substr(i);
                    return result;
                }};

                // Extract the name
                auto [NS, Key] {extractKey()};
                std::string name {NS.empty()? Key: NS + ':' + Key};

                // Extract the key, value pairs
                stdx::ordered_map<std::string, std::string> attrs;
                while (!sv.empty()) {
                    auto [NS, attrKey] {extractKey()};
                    attrKey = NS.empty()? attrKey: NS + ':' + attrKey;
                    if (sv.size() <= 2 || attrs.exists(attrKey) || sv.at(0) != '=')
                        throw std::runtime_error("Malformed XML");

                    // Update SV
                    std::size_t i {1};
                    while (i < sv.size() && std::isspace(sv.at(i))) ++i;
                    sv = sv.substr(i);

                    std::string attrVal {extractValue()};
                    attrs.emplace(attrKey, attrVal);
                }

                return std::make_pair(name, std::move(attrs));
            }

        public:
            std::optional<XMLDeclaration> xmlDeclaration;
            XMLNode::NodePtr root;

            explicit XMLTree(
                const XMLNode::NodePtr &root, 
                const std::optional<XMLDeclaration> &xmlDec = std::nullopt
            ): 
                xmlDeclaration{xmlDec}, 
                root{root} 
            {}

            friend std::ostream &operator<<(std::ostream &os, const XMLTree &tree) {
                if (tree.xmlDeclaration) 
                    os << *tree.xmlDeclaration << '\n';
                os << tree.root << '\n'; 
                return os;
            }

            static XMLTree parseFile(const std::string &fname) {
                std::ifstream ifs {fname};
                std::ostringstream oss;
                oss << ifs.rdbuf();
                return parse(oss.str());
            }

            static XMLTree parse(const std::string &raw, bool preserveSpace = false) {
                XMLNode::NodePtr root;
                std::optional<XMLDeclaration> xmlDec;
                std::stack<XMLNode::NodePtr> stk;
                std::size_t i {0}, N {raw.size()}; 
                while (i < N) {
                    if (!std::isspace(raw.at(i)) && raw.at(i) != '<') 
                        throw std::runtime_error("Malformed XML");

                    // Search until '>'
                    else if (raw.at(i) == '<') {
                        std::string acc;
                        enum class STATE {START, COMMENT, PI, CDATA, NODE} 
                        state {STATE::START};
                        char insideStr {0};
                        while (i < N) {
                            acc += raw.at(i);
                            if (state == STATE::START && acc.back() == '<') {
                                // Acc contains non space char and stk is empty
                                acc.pop_back();
                                std::string trimmed {trim(acc)};
                                if (!trimmed.empty() && stk.empty())
                                    throw std::runtime_error("Malformed XML");
                                else if ((preserveSpace && !acc.empty()) && !stk.empty())
                                    stk.top()->addChild(XMLNode::TextNode(acc));
                                else if ((!preserveSpace && !trimmed.empty()) && !stk.empty())
                                    stk.top()->addChild(XMLNode::TextNode(trimmed));
                                acc = '<';
                            } else if (state == STATE::START && acc == "<!--") {
                                state = STATE::COMMENT; 
                            } else if (state == STATE::START && acc == "<?") {
                                state = STATE::PI;
                            } else if (state == STATE::START && acc == "<!DOCTYPE") {
                                throw std::runtime_error("DOCTYPE declarations are not supported in this version.");
                            } else if (state == STATE::START && acc == "<![CDATA[") {
                                state = STATE::CDATA;
                            } else if (state == STATE::START && acc.size() >= 2 && acc.at(0) == '<' && (std::isalpha(acc.at(1)) || acc.at(1) == '/')) {
                                state = STATE::NODE;
                            } else if (isIn(state, {STATE::PI, STATE::NODE}) && isIn(acc.back(), {'"', '\''})) {
                                insideStr = !insideStr? acc.back(): 0;
                            } else if (state == STATE::COMMENT && acc.ends_with("-->")) {
                                state = STATE::START; acc.clear();
                            } else if (state == STATE::CDATA && acc.ends_with("]]>")) {
                                state = STATE::START;
                                if (stk.empty()) throw std::runtime_error("Malformed XML");
                                stk.top()->addChild(XMLNode::CDataNode(acc));
                                acc.clear();
                            } else if (state == STATE::PI && !insideStr && acc.ends_with("?>")) {
                                std::string_view content {acc.begin() + 2, acc.begin() + static_cast<long>(acc.size()) - 2};
                                auto [name, attrs] {extractNodeInfo(content)};
                                if (name == "xml") {
                                    xmlDec = XMLDeclaration {
                                        attrs.extract("version").value_or("1.0"), 
                                        attrs.extract("encoding").value_or("UTF-8"), 
                                        attrs.extract("standalone").value_or("yes")
                                    };
                                    if (!stk.empty() || !attrs.empty()) 
                                        throw std::runtime_error("Malformed XML");
                                } else if (stk.empty()) {
                                    throw std::runtime_error("Malformed XML");
                                } else {
                                    XMLNode::NodePtr node {XMLNode::Node(name, XMLNode::NodeType::PI)};
                                    stk.top()->addChild(node);
                                    node->setAttrs(std::move(attrs));
                                }
                                state = STATE::START; acc.clear();
                            } else if (state == STATE::NODE && !insideStr && acc.ends_with(">")) {
                                enum class MODE {SELF_CLOSING, OPEN, CLOSE} mode;
                                if (acc.size() >= 2 && acc.at(acc.size() - 2) == '/') mode = MODE::SELF_CLOSING;
                                else if (acc.size() >= 2 && acc.at(1) == '/') mode = MODE::CLOSE;
                                else mode = MODE::OPEN;
                                std::string_view content {
                                    acc.begin() + (mode != MODE::CLOSE? 1: 2), 
                                    acc.begin() + static_cast<long>(acc.size()) - (mode != MODE::SELF_CLOSING? 1: 2)
                                };
                                auto [name, attrs] {extractNodeInfo(content)};
                                if (mode == MODE::SELF_CLOSING && !stk.empty()) {
                                    XMLNode::NodePtr node {XMLNode::Node(name)};
                                    node->setAttrs(std::move(attrs));
                                    stk.top()->addChild(node);
                                } else if (mode == MODE::CLOSE && attrs.empty() && !stk.empty() && name == stk.top()->getName()) {
                                    root = std::move(stk.top()); stk.pop();
                                    if (!stk.empty()) stk.top()->addChild(root);
                                } else if (mode == MODE::OPEN) {
                                    XMLNode::NodePtr node {XMLNode::Node(name)};
                                    node->setAttrs(std::move(attrs));
                                    stk.push(node);
                                } else {
                                    throw std::runtime_error("Malformed XML");
                                }

                                state = STATE::START; acc.clear();
                            }
                            ++i;
                        }

                        // Validate tag
                        if (state != STATE::START || !acc.empty())
                            throw std::runtime_error("Malformed XML");
                    } 

                    ++i;
                }

                if (!stk.empty()) throw std::runtime_error("Malformed XML");
                return XMLTree{root, xmlDec};
            }
    };
};

int main() {
    XMLUtil::XMLTree tree {XMLUtil::XMLTree::parseFile("sample.xml")};
    std::cout << tree << '\n';
    return 0;
}
