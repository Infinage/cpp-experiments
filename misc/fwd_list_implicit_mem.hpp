#pragma once
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <utility>

template<typename T>
class forward_list {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

    private:
        struct Node {
            T value;
            Node *next {};
            Node(auto &&value): value{std::forward<decltype(value)>(value)} {}
        };

        template<typename U>
        class Iterator {
            private:
                Node *curr {};
                friend class forward_list<T>;

            public:
                using value_type = U;
                using reference = U&;
                using pointer = U*;
                using difference_type = std::ptrdiff_t;
                using iterator_category = std::forward_iterator_tag;

                Iterator() = default;
                Iterator(Node *ptr): curr{ptr} {}

                Iterator &operator++() {
                    curr = curr->next;
                    return *this;
                }

                Iterator &operator++(int) {
                    auto temp {*this};
                    operator++();
                    return temp;
                }

                bool operator==(const Iterator &other) const {
                    return curr == other.curr;
                }

                auto &operator*(this auto &&self) { return self.curr->value; }
                auto *operator->(this auto &&self) { return &self.curr->value; }
        };

        struct Deleter {
            void operator() (Node *ptr) const {
                while (ptr) delete std::exchange(ptr, ptr->next);
            }
        };

        // Data members
        std::unique_ptr<Node, Deleter> head {};
        size_type nElems {};

    public:
        using iterator = Iterator<T>;
        using const_iterator = Iterator<const T>;

        // Basic functions
        auto front(this auto &&self) { 
            if (self.empty()) [[unlikely]] 
                throw std::out_of_range{"Empty List"};
            return self.head->value; 
        }
        size_type size() const noexcept { return nElems; }
        bool empty() const noexcept { return !head; }
        void clear() noexcept {
            head.reset();
            nElems = 0;
        }

        // Default CTOR and Destructor
        forward_list() = default;
        ~forward_list() = default;

        // Construct from two iterables
        template<std::input_iterator It>
        forward_list(It b, It e) {
            if (b == e) return;
            head.reset(new Node{*b});
            Node *tail = head.get();
            ++b; ++nElems; // Add first ele
            for (It curr {b}; curr != e; ++curr) {
                tail->next = new Node{*curr};
                tail = tail->next;
                ++nElems;
            }
        }

        // Initializer list ctor
        forward_list(std::initializer_list<T> list): 
            forward_list(list.begin(), list.end()) {}

        // Copy Constructor
        forward_list(const forward_list &other):
            forward_list(other.begin(), other.end()) {}

        // Move Constructor
        forward_list(forward_list &&other) noexcept:
            head {std::move(other.head)},
            nElems{std::exchange(other.nElems, 0)} {}

        // Free Swap function for assign ops
        friend void swap(forward_list &l1, forward_list &l2) noexcept {
            using std::swap; 
            l1.head.swap(l2.head);
            swap(l1.nElems, l2.nElems);
        }

        // Copy Assignment
        forward_list &operator=(const forward_list &other) {
            auto temp {forward_list{other}};
            swap(*this, temp);
            return *this;
        }

        // Move Assignment
        forward_list &operator=(forward_list &&other) noexcept {
            auto temp {forward_list{std::move(other)}};
            swap(*this, temp);
            return *this;
        }

        // Equality Check
        bool operator==(const forward_list &other) const noexcept {
            return size() == other.size() &&
                std::equal(begin(), end(), other.begin());
        }

        void push_front(auto &&value) 
        requires(std::is_constructible_v<T, std::remove_cvref_t<decltype(value)>>)
        {
            auto next {head.release()};
            head.reset(new Node{std::forward<decltype(value)>(value)});
            head->next = next;
            ++nElems;
        }

        void pop_front() {
            if (empty()) return;
            auto oldHead {head.release()};
            head.reset(oldHead->next);
            delete oldHead;
            --nElems;
        }

        iterator insert_after(iterator pos, const_reference value) {
            Node *node {new Node{value}};   
            node->next = std::exchange(pos.curr->next, node);
            ++nElems;
            return iterator{node};
        }

        iterator erase_after(iterator pos) {
            if (pos == end() || std::next(pos) == end()) return end();
            Node *node {pos.curr};
            delete std::exchange(node->next, node->next->next);
            --nElems;
            return iterator{node->next};
        }

        // Iterator support
        auto begin(this auto &&self) { return iterator{self.head.get()}; }
        auto end(this auto &&) { return iterator{}; }
        auto cbegin() const { return begin(); }
        auto cend() const { return end(); }
};
