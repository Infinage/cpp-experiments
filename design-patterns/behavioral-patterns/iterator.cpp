#include <iostream>
#include <queue>
#include <stack>
#include <vector>

template <typename T>
class BinaryTreeNode {
    public:
        T data;
        BinaryTreeNode *left;
        BinaryTreeNode *right;
        BinaryTreeNode(int data): data(data), left(nullptr), right(nullptr) {}
        ~BinaryTreeNode() {
            if (left  != nullptr) delete left;
            if (right != nullptr) delete right;
        }

        static BinaryTreeNode<T> *fromVector(std::vector<T> &&vec, T null) {
            std::size_t parent{0};
            std::vector<BinaryTreeNode<T>*> nodes;

            for (std::size_t i{0}; i < vec.size(); i++) {
                BinaryTreeNode<T> *curr {
                    vec[i] != null? new BinaryTreeNode{vec[i]}: nullptr
                };

                if (vec[i] != null)
                    nodes.push_back(curr);

                if (i == 0) continue;
                else if (i % 2) nodes[parent]->left = curr;
                else nodes[parent++]->right = curr;
            }

            return nodes.empty()? nullptr: nodes[0];
        }
};

template <typename Node>
class Iterator {
    public:
        virtual ~Iterator() = default;
        virtual void reset() = 0;
        virtual bool hasNext() = 0;
        virtual Node* getNext() = 0;
};

template <typename Node>
class DFSIterator: public Iterator<Node> {
    private:
        Node* root;
        std::stack<Node*> stk;

    public:
        DFSIterator(Node *root): root(root) { stk.push(root); }
        void reset() override { stk = std::stack<Node*>{{root}}; }
        bool hasNext() override { return !stk.empty(); }
        Node* getNext() override {
            Node* curr {stk.top()};
            stk.pop();
            if (curr->right != nullptr) stk.push(curr->right);
            if (curr->left  != nullptr) stk.push(curr->left);
            return curr;
        }
};

template <typename Node>
class BFSIterator: public Iterator<Node> {
    private:
        Node* root;
        std::queue<Node*> que;

    public:
        BFSIterator(Node* root): root(root) { que.push(root); }
        void reset() override { que = std::queue<Node*>{{root}}; }
        bool hasNext() override { return !que.empty(); }
        Node* getNext() override {
            Node* curr {que.front()};
            que.pop();
            if (curr->left != nullptr) que.push(curr->left);
            if (curr->right != nullptr) que.push(curr->right);
            return curr;
        }
};

template <typename T>
void printTraversal(Iterator<T> *it) {
    while (it->hasNext())
        std::cout << it->getNext()->data << " ";
    std::cout << "\n";
}

int main() {
    using BTI = BinaryTreeNode<int>;
    BTI *root = BTI::fromVector({5, 12, 7, 18, -1, -1, 69, 4, 13}, -1);

    Iterator<BTI> *it_dfs {new DFSIterator{root}};
    printTraversal<BTI>(it_dfs);

    Iterator<BTI> *it_bfs {new BFSIterator{root}};
    printTraversal<BTI>(it_bfs);

    delete it_dfs; delete it_bfs; delete root;
}
