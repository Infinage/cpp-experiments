#pragma once
#include <atomic>
#include <functional>
#include <utility>

namespace scratchpad {
    template<typename T>
    struct DefaultDeleter {
        void operator()(T* data) const { delete data; }
    };

    template<typename T>
    struct DefaultDeleter<T[]> {
        void operator()(T* data) const { delete []data; }
    };

    template<typename T>
    class shared_ptr_base {
        public:
            using DeleterType = std::function<void(T*)>;

            shared_ptr_base() = default;
            ~shared_ptr_base() { if (cnt && (*cnt)-- == 1) { deleter(data); delete cnt; } }

            shared_ptr_base(T *data, DeleterType deleter = DefaultDeleter<T>{}): 
                data{data}, deleter{std::move(deleter)} 
            { 
                try {
                    cnt = new std::atomic_int64_t(1);
                } catch (...) {
                    deleter(data);
                    throw;
                }
            }

            // Copy CTOR
            shared_ptr_base(const shared_ptr_base &other): data(other.data), 
                cnt(other.cnt), deleter(other.deleter) { if(cnt) ++(*cnt); }

            // Move CTOR
            shared_ptr_base(shared_ptr_base &&other) noexcept: 
                data(std::exchange(other.data, nullptr)), 
                cnt(std::exchange(other.cnt, nullptr)),
                deleter(std::move(other.deleter))
            {}

            // Swap function
            void swap(shared_ptr_base &other) noexcept {
                using std::swap;
                swap(data, other.data);
                swap(cnt, other.cnt);
                swap(deleter, other.deleter);
            }

            // Copy Assignment
            shared_ptr_base &operator=(const shared_ptr_base &other) {
                shared_ptr_base{other}.swap(*this); 
                return *this;
            }

            // Move Assignment
            shared_ptr_base &operator=(shared_ptr_base &&other) noexcept {
                shared_ptr_base{std::move(other)}.swap(*this); 
                return *this;
            }

            // Member functions
            bool empty() const noexcept { return !data; }
            operator bool() const noexcept { return !!data; }
            bool operator==(const shared_ptr_base &other) const noexcept { 
                return data == other.data; }
            T *get(this auto &&self) { return self.data; } 

        protected:
            T *data {nullptr};
            std::atomic_int64_t *cnt {nullptr};
            DeleterType deleter;
    };

    template<typename T>
    class shared_ptr: shared_ptr_base<T> {
        public:
            using shared_ptr_base<T>::shared_ptr_base;

            // Template specific overloads
            T &operator *(this auto &&self) noexcept { return *self.data; }
            T *operator->(this auto &&self) noexcept { return self.data; }
    };

    template<typename T>
    class shared_ptr<T[]>: shared_ptr_base<T> {
        public:
            using shared_ptr_base<T>::shared_ptr_base;

            // Template specific overloads
            T &operator[](this auto &&self, const std::size_t idx) { 
                return self.data[idx]; } 
    };
}
