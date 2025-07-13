#pragma once
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

// Dummy Allocator that delegates to `malloc` and `free`
template<typename T>
struct SimpleAllocator {
    using value_type = T;
    T* allocate(std::size_t N) {
        auto ptr {static_cast<T*>(std::malloc(sizeof(T) * N))};
        if (!ptr) throw std::bad_alloc{};
        return ptr;
    }

    void deallocate(T *ptr, std::size_t) {
        std::free(ptr);
    }
};

// `typename A = std::allocator<T>` is a better default
template<typename T, typename A = SimpleAllocator<T>>
class forward_list {
    public:
        using value_type = std::allocator_traits<A>::value_type;
        using size_type = std::allocator_traits<A>::size_type;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;

    private:
        // forward_list is a structure linking "nodes"
        struct Node {
            T value;
            Node *next {};
            Node(auto &&value): value{std::forward<decltype(value)>(value)} {}
        };

        template<typename U>
        class Iterator {
            private:
                Node *curr {};
                friend class forward_list<T, A>;

            public:
                using value_type = U;
                using reference = U&;
                using pointer = U*;
                using difference_type = std::ptrdiff_t;
                using iterator_category = std::forward_iterator_tag;

                Iterator() = default;
                Iterator(Node *ptr): curr{ptr} {}

                // Note: we return by ref
                Iterator &operator++() {
                    curr = curr->next;
                    return *this;
                }

                // Note: we return by value
                Iterator operator++(int) {
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

        // Data members
        using NodeAlloc = typename std::allocator_traits<A>::template 
            rebind_alloc<Node>;
        [[no_unique_address]] NodeAlloc alloc {};
        Node *head {};
        size_type nElems {};

    public:
        using iterator = Iterator<T>;
        using const_iterator = Iterator<const T>;

        auto &front(this auto &&self) { 
            if (self.empty()) [[unlikely]] 
                throw std::out_of_range{"Empty List"};
            return self.head->value; 
        }

        // Basic functions
        size_type size() const noexcept { return nElems; }
        bool empty() const noexcept { return !head; }
        void clear() noexcept {
            while(!!head) {
                auto next {head->next};
                std::allocator_traits<NodeAlloc>::destroy(alloc, head);
                std::allocator_traits<NodeAlloc>::deallocate(alloc, head, 1);
                head = next;
            }
            nElems = 0;
        }

        // Default CTOR and Destructor
        ~forward_list() { clear(); }

        // Construct from two iterables
        template<std::input_iterator It>
        forward_list(It b, It e) {
            if (b == e) return;
            try {
                head = std::allocator_traits<NodeAlloc>::allocate(alloc, 1);
                std::allocator_traits<NodeAlloc>::construct(alloc, head, *b);
                Node *tail = head; ++b; ++nElems; // Add first ele
                for (It curr {b}; curr != e; ++curr) {
                    tail->next = std::allocator_traits<NodeAlloc>::allocate(alloc, 1);
                    std::allocator_traits<NodeAlloc>::construct(alloc, tail->next, *curr);
                    tail = tail->next;
                    ++nElems;
                }
            } catch (...) { clear(); throw; }
        }

        // CTOR to support allocators
        forward_list(const NodeAlloc &alloc = NodeAlloc{}): alloc{alloc} {}

        // Initializer list ctor
        forward_list(std::initializer_list<T> list): 
            forward_list(list.begin(), list.end()) {}

        // Copy Constructor
        forward_list(const forward_list &other):
            forward_list(other.begin(), other.end()) {}

        // Move Constructor - we deliberately don't swap the allocator state
        forward_list(forward_list &&other) noexcept:
            head{std::exchange(other.head, nullptr)}, 
            nElems{std::exchange(other.nElems, 0)} {}

        // Free Swap function for copy-swap idiom
        friend void swap(forward_list &l1, forward_list &l2) noexcept {
            using std::swap; 
            swap(l1.head, l2.head);
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

        // Equality Check - support comparing lists of diff alloc types
        template<typename OtherAlloc>
        bool operator==(const forward_list<T, OtherAlloc> &other) const noexcept {
            return size() == other.size() &&
                std::equal(begin(), end(), other.begin());
        }

        // Inserts a new element at the front of the list
        void push_front(auto &&value)
        requires(std::is_constructible_v<T, std::remove_cvref_t<decltype(value)>>)
        {
            auto node {std::allocator_traits<NodeAlloc>::allocate(alloc, 1)};
            std::allocator_traits<NodeAlloc>::construct(alloc, node, std::forward<decltype(value)>(value));
            node->next = head;
            head = node;
            ++nElems;
        }

        // Removes the first element from the list if it exists
        void pop_front() {
            if (empty()) return;
            Node *node {std::exchange(head, head->next)};
            std::allocator_traits<NodeAlloc>::destroy(alloc, node);
            std::allocator_traits<NodeAlloc>::deallocate(alloc, node, 1);
            --nElems;
        }

        // Inserts a new element immediately after the given position
        iterator insert_after(iterator pos, auto &&value)
        requires(std::is_constructible_v<T, std::remove_cvref_t<decltype(value)>>)
        {
            auto node {std::allocator_traits<NodeAlloc>::allocate(alloc, 1)};
            std::allocator_traits<NodeAlloc>::construct(alloc, node, std::forward<decltype(value)>(value));
            node->next = std::exchange(pos.curr->next, node);
            ++nElems;
            return iterator{node};
        }

        // Erases the element immediately after the given position
        iterator erase_after(iterator pos) {
            if (pos == end() || std::next(pos) == end()) 
                return end();
            Node *node {pos.curr};
            auto toDel {std::exchange(node->next, node->next->next)};
            std::allocator_traits<NodeAlloc>::destroy(alloc, toDel);
            std::allocator_traits<NodeAlloc>::deallocate(alloc, toDel, 1);
            --nElems;
            return iterator{node->next};
        }

        // Iterator support
        auto begin(this auto &&self) { return iterator{self.head}; }
        auto end(this auto &&) { return iterator{}; }
        auto cbegin() const { return begin(); }
        auto cend() const { return end(); }
};
