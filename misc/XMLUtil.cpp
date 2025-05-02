#include <iostream>
#include <memory>
#include <sstream>
#include "ordered_map.hpp"

/*
 * - Does not provide XSD validation
 * - Does not preserve whitespaces or comments
 *
 * TODO:
 * 1. Create XML Tree class - contains XML Dec and entity defs
 */

namespace XMLUtil {
    /*
     * Both CDATA, PI & Ordinary nodes can be represented via `XMLNode`
     */
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
                   << "\"standalone=\"" << dec.standalone 
                   << "\"?>";
                return os;
            }
    };

    class EntityDeclaration {
        private:
            const std::string name;
            stdx::ordered_map<std::string, std::string> entityMap;

        public:
            EntityDeclaration(
                const std::string &name,
                const stdx::ordered_map<std::string, std::string> &entityMap = {}
            ): name(name), entityMap(entityMap) {}

            inline bool exists(const std::string &key) const { return entityMap.exists(key); } 
            inline bool empty() const { return entityMap.empty(); }
            inline std::string operator[](const std::string &key) const { return entityMap.at(key); }
            inline void insert(const std::string &key, const std::string &value) { entityMap.emplace(key, value); }

            friend std::ostream &operator<<(std::ostream &os, const EntityDeclaration &dec) {
                if (!dec.empty()) {
                    os << "<!DOCTYPE " << dec.name << "[\n";
                    for (const std::pair<std::string, std::string> &kv: dec.entityMap)
                        os << "\t<!ENTITY " << kv.first << " \"" << kv.second << "\">\n";
                    os << "]>";
                }
                return os;
            }
    };

    class XMLTree {
        public:
            XMLDeclaration xmlDeclaration;
            EntityDeclaration entityDeclaration;
            XMLNode::NodePtr root;

            XMLTree(const XMLNode::NodePtr &root, const EntityDeclaration &entities = {""}): 
                xmlDeclaration{}, 
                entityDeclaration{entities.empty()? root->getName(): entities}, 
                root{root} 
            {}

            friend std::ostream &operator<<(std::ostream &os, const XMLTree &tree) {
                os << tree.xmlDeclaration << '\n';
                os << tree.entityDeclaration << '\n'; 
                os << tree.root << '\n'; 
                return os;
            }
    };
};

int main() {
    using namespace XMLUtil;

    // Child nodes
    XMLNode::NodePtr text1 {XMLNode::TextNode("This is ")};
    XMLNode::NodePtr b1 {XMLNode::Node("b")}; b1->setText("bold");
    XMLNode::NodePtr text2 {XMLNode::TextNode(" and ")};
    XMLNode::NodePtr i1 {XMLNode::Node("i")}; i1->setText("italic");
    XMLNode::NodePtr text3 {XMLNode::TextNode(" text with ")};
    XMLNode::NodePtr u1 {XMLNode::Node("u")}; u1->setText("underline");
    XMLNode::NodePtr cdata1 {XMLNode::CDataNode("if (a < b && b > c) { return; }")};

    // Container node
    XMLNode::NodePtr pi1 {XMLNode::Node("process", XMLNode::NodeType::PI)};
    pi1->setAttr("do-something", "true"); pi1->setAttr("dummy", "what?");

    // Root node
    XMLNode::NodePtr root {XMLNode::Node("root")};
    root->setAttr("attr", "hello");
    root->addChildren(text1, b1, text2, i1, text3, u1, cdata1, pi1);

    // Create an Entity declaration
    EntityDeclaration entities(root->getName());
    entities.insert("company1", "OpenAI Inc.");
    entities.insert("company2", "Tesla Inc.");

    // Create a tree with these components
    XMLTree tree {root, entities};

    // Try printing before and after deleting a node
    std::cout << tree << '\n';
    pi1->unlink();
    std::cout << tree << '\n';

    return 0;
}
