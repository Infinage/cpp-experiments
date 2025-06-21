#pragma once
#include <concepts>
#include <utility>

namespace scratchpad {
    template<typename T, typename DeleterType>
    concept DeleterFunc = requires(T *data) {
        {std::declval<DeleterType>()(data)} -> std::same_as<void>;
    };

    template<typename T>
    struct DefaultDeleter {
        void operator()(T *ptr) const { delete ptr; }
    };

    template<typename T>
    struct DefaultDeleter<T[]> {
        void operator()(T *ptr) const { delete []ptr; }
    };

    template<typename T, typename Deleter>
    requires (DeleterFunc<T, Deleter>)
    class unique_ptr_base: Deleter {
        protected:
            T *data {nullptr};

        public:
            using DeleterType = Deleter;

            unique_ptr_base() = default;
            ~unique_ptr_base() { if(data) static_cast<DeleterType&>(*this)(data); }
            explicit unique_ptr_base(T *data): data{data} {}
            unique_ptr_base(T *data, Deleter deleter): 
                DeleterType{deleter}, data{data} {}

            // Delete move ctor / assign
            unique_ptr_base(const unique_ptr_base &other) = delete;
            unique_ptr_base &operator=(const unique_ptr_base &other) = delete;

            // Move ctor
            unique_ptr_base(unique_ptr_base &&other) noexcept: 
                DeleterType(std::move(static_cast<DeleterType&>(other))),
                data{std::exchange(other.data, nullptr)}
            {}

            // Move assignment using swap idiom
            unique_ptr_base &operator=(unique_ptr_base &&other) noexcept { 
                unique_ptr_base{std::move(other)}.swap(*this);
                return *this;  
            }

            // Swap member function for swap idiom
            void swap(unique_ptr_base &other) noexcept {
                using std::swap; 
                swap(static_cast<DeleterType&>(*this), 
                     static_cast<DeleterType&>(other));
                swap(other.data, data);
            }

            // Member functions
            T *get(this auto &&self) { return self.data; }
            bool empty() const noexcept { return !data; }
            operator bool() const noexcept { return !!data; }
            T *release() noexcept { return std::exchange(data, nullptr); }
            bool operator==(const unique_ptr_base &other) const noexcept { 
                return data == other.data; }

            // Overload nullptr_t assignment as reset
            unique_ptr_base &operator=(std::nullptr_t) noexcept {
                if (data) static_cast<DeleterType&>(*this)(data); 
                data = nullptr; 
                return *this;
            }
    };

    template<typename T, typename Deleter = DefaultDeleter<T>>
    class unique_ptr: public unique_ptr_base<T, Deleter> {
        public:
            using unique_ptr_base<T, Deleter>::unique_ptr_base;
            using unique_ptr_base<T, Deleter>::operator=;

            // Template specific
            T *operator->(this auto &&self) { return self.data; }
            T &operator *(this auto &&self) { return *self.data; }
    };
    
    template<typename T, typename Deleter>
    class unique_ptr<T[], Deleter>: public unique_ptr_base<T, Deleter> {
        public:
            using unique_ptr_base<T, Deleter>::unique_ptr_base;
            using unique_ptr_base<T, Deleter>::operator=;

            // Template specific
            T &operator[](this auto &&self, const std::size_t idx) { 
                return self.data[idx]; }
    };
}
