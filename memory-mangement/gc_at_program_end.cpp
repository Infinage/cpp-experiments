#include <memory>
#include <mutex>
#include <print>
#include <vector>

class GC {
    private:
        struct GCRoot {
            void *rawPtr {nullptr};
            GCRoot(void *ptr): rawPtr(ptr) {}
            virtual ~GCRoot() = default;
        };

        template<typename T>
        struct GCNode: public GCRoot {
            template<typename ...Args>
            GCNode(Args &&...args): GCRoot(new T{std::forward<Args>(args)...}) {}
            ~GCNode() { delete static_cast<T*>(rawPtr); }
        };

        // Variables
        std::mutex mutex;
        std::vector<std::unique_ptr<GCRoot>> memory;
        GC() = default;

        // Insert into vector and return pointer to same
        template<typename T, typename ...Args>
        T *addRoot(Args &&...args) {
            std::lock_guard lock(mutex);
            auto uPtr {std::make_unique<GCNode<T>>(std::forward<Args>(args)...)};
            memory.emplace_back(std::move(uPtr));
            return static_cast<T*>(memory.back()->rawPtr);
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
    [[maybe_unused]] X *xPtr {GCNew<X>(15)};
}
