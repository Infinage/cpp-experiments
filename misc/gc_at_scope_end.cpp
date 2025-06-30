#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <print>

// Forward declare
template<typename T> class counting_ptr;

class GC {
    private:
        struct GCRoot {
            void *rawPtr {nullptr};        
            std::size_t size {1};
            GCRoot(void *ptr, std::size_t size = 1): rawPtr{ptr}, size(size) {}
            virtual ~GCRoot() = default;
        };

        template<typename T>
        struct GCNode: public GCRoot {
            template<typename ...Args>
            GCNode(Args &&...args): GCRoot{new T{std::forward<Args>(args)...}} {}
            ~GCNode() { delete static_cast<T*>(rawPtr); }
        };

        template<typename T>
        struct GCNode<T[]>: public GCRoot {
            template<typename ...Args>
            GCNode(Args &&...args): GCRoot{new T[]{std::forward<Args>(args)...}, 
                sizeof...(args)} {}
            ~GCNode() { delete []static_cast<T*>(rawPtr); }
        };

        // `memory` holds the address wrapped inside unique_ptrs
        // `collectibles` stores the ptrs that are good to be reclaimed back
        std::unordered_map<std::uintptr_t, std::unique_ptr<GCRoot>> memory;
        std::unordered_set<std::uintptr_t> collectibles;

        // For ensuring ops are thread safe
        std::mutex mutex;

        // Private default CTOR - singleton pattern
        GC() = default;

        // Helper to return `T*` post inserting into memory
        template<typename T, typename ...Args>
        auto addRoot(Args &&...args) {
            using Pointee = std::conditional_t<std::is_array_v<T>, std::remove_extent_t<T>, T>;
            std::lock_guard lock{mutex};    
            auto uPtr {std::make_unique<GCNode<T>>(std::forward<Args>(args)...)};
            void *rPtr {uPtr->rawPtr};
            memory.emplace(reinterpret_cast<std::uintptr_t>(rPtr), std::move(uPtr));
            return static_cast<Pointee*>(rPtr);
        }

        template<typename T, typename ...Args>
        friend counting_ptr<T> GCNew(Args &&...args);

    public:
        void mark(void* ptr) {
            std::lock_guard lock{mutex};
            collectibles.insert(reinterpret_cast<std::uintptr_t>(ptr));
        }

        void collect() {
            std::lock_guard lock{mutex};
            for (auto ptrID: collectibles)
                memory.erase(ptrID);
            collectibles.clear();
        }

        static GC &get() {
            static GC singleton{};
            return singleton;
        }
};

template<typename T>
class counting_ptr_base {
    protected:
        T *ptr;
        std::atomic<int> *count {nullptr};

    public:
        counting_ptr_base(T *ptr): ptr{ptr}, count{new std::atomic<int>{1}} {}

        counting_ptr_base(counting_ptr_base &&other) noexcept:
            ptr{std::exchange(other.ptr, nullptr)},
            count{std::exchange(other.count, nullptr)} {}

        counting_ptr_base(const counting_ptr_base& other):
            ptr{other.ptr}, count{other.count} { if (count) ++(*count); }

        void swap(counting_ptr_base &other) noexcept {
            using std::swap;
            swap(ptr, other.ptr);
            swap(count, other.count);
        }

        counting_ptr_base &operator=(const counting_ptr_base &other) {
            counting_ptr_base{other}.swap(*this);
            return *this;
        }

        counting_ptr_base &operator=(counting_ptr_base &&other) {
            counting_ptr_base{std::move(other)}.swap(*this);
            return *this;
        }

        ~counting_ptr_base() { 
            if (count && (*count)-- == 1) {
                GC::get().mark(ptr);
                delete count;
            }
        }

        // Member functions
        bool empty() const noexcept { return !ptr; }
        operator bool() const noexcept { return !empty(); }

        bool operator==(const counting_ptr_base &other) const noexcept { 
            return ptr == other.ptr; }

        template<typename U>
        bool operator==(const counting_ptr_base<U> &other) const noexcept { 
            return ptr == other.ptr; }

        bool operator==(const auto *other) const noexcept { 
            return ptr == other; }

        T *get(this auto &&self) { return self.ptr; } 
};

template<typename T> 
class counting_ptr: public counting_ptr_base<T> {
    public:
        using counting_ptr_base<T>::counting_ptr_base;

        // Template specific overloads
        T &operator *(this auto &&self) noexcept { return *self.ptr; }
        T *operator->(this auto &&self) noexcept { return self.ptr; }
};

template<typename T> 
class counting_ptr<T[]>: public counting_ptr_base<T> {
    public:
        using counting_ptr_base<T>::counting_ptr_base;

        // Template specific overloads
        T &operator [](this auto &&self, const std::size_t idx) { 
            return self.ptr[idx]; }
};

template<typename T, typename ...Args>
counting_ptr<T> GCNew(Args &&...args) {
    return GC::get().addRoot<T>(std::forward<Args>(args)...);
}

// Dummy class to trigger GC::collect on lifetime end
struct scoped_collect {
    scoped_collect() = default;
    scoped_collect(const scoped_collect &) = delete;
    scoped_collect &operator=(const scoped_collect&) = delete;
    scoped_collect(scoped_collect&&) noexcept = delete;
    scoped_collect &operator=(scoped_collect&&) noexcept = delete;
    ~scoped_collect() { GC::get().collect(); }
};

// ---- Test Code ---- //

struct Logger {
    int id;
    Logger(int id): id(id) { std::println("[Logger {}] Constructed", id); }
    ~Logger() { std::println("[Logger {}] Destructed", id); }
};

int main() {
    { scoped_collect gc;
        auto log = GCNew<Logger>(1);
        std::println("Inside scope - Logger @ {}", static_cast<void*>(log.get()));
    }
    return 0;
}
