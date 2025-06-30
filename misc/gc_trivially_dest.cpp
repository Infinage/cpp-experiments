#include <cstdlib>
#include <mutex>
#include <print>
#include <type_traits>
#include <vector>

class GC {
    private:
        std::vector<void*> memory;
        GC() = default;
        std::mutex mutex;

        template<typename T, typename ...Args>
        requires(std::is_trivially_destructible_v<T>)
        T *addRoot(Args &&...args) {
            std::lock_guard lock(mutex);
            void *ptr {memory.emplace_back(new T{std::forward<Args>(args)...})};
            return static_cast<T*>(ptr);
        }

    public:
        GC(const GC&) = delete;
        GC &operator=(const GC&) = delete;

        // Meyers singleton pattern
        static GC &get() { 
            static GC singleton{}; 
            return singleton; 
        }

        template<typename T, typename ...Args>
        friend T *GCNew(Args &&...args);

        ~GC() { 
            std::println("GC running. Deallocating {} objects.", std::size(memory));
            for (void *ptr: memory) std::free(ptr); 
        }
};

template<typename T, typename ...Args>
T *GCNew(Args &&...args) {
    return GC::get().addRoot<T>(std::forward<Args>(args)...); 
}

// ----------- TEST DRIVER CODE ----------- //

struct X {
    int x;
    X(int x): x(x) { std::println("CTOR: {}", x); }
    ~X() { std::println("DTOR: {}", x); }
};

int main() {
    [[maybe_unused]] int *intPtr {GCNew<int>(10)};
    [[maybe_unused]] double *dblPtr {GCNew<double>(10.)};
    //[[maybe_unused]] X *xPtr {GCNew<X>(15)};
}
