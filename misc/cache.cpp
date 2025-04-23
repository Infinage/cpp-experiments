#include <functional>
#include <iostream>
#include <ostream>
#include <tuple>
#include <unordered_map>

// Concept for print helper
template<typename T>
concept OpStreamable = requires(T val, std::ostream &os) {
    { os << val } -> std::same_as<std::ostream&>;
};

// Concept to check if hashable
template<typename Key>
concept Hashable = requires(Key key) { 
    {std::hash<Key>{}(key)} -> std::same_as<std::size_t>; 
};

template<typename Value, typename ...Key>
class Cache {
    private:
        std::unordered_map<std::size_t, Value> data;
        std::function<Value(Key...)> func; 
        std::function<std::size_t(Key...)> hashFunc;
        std::size_t hits {0}, miss {0};

    public:
        Cache(
            const std::function<Value(Key...)>& func, 
            const std::function<std::size_t(Key...)> &hashFunc
        ): func(func), hashFunc(hashFunc) {}

        template<typename = void> requires (sizeof...(Key) == 1 && (Hashable<Key> && ...))
        Cache( const std::function<Value(Key...)>& func): 
        func(func), hashFunc([](const Key &...args){
            // Okay since we have just 1 arg anyway
            return std::hash<Key...>{}(args...);
        }) {}

        inline std::array<std::size_t, 3> stat() const { 
            return {hits, miss, data.size()}; 
        }

        inline Value operator()(const Key &...args) {
            std::size_t key {hashFunc(args...)};
            if (data.find(key) == data.end()) {
                miss++;
                return data[key] = func(args...);
            } else {
                hits++;
                return data[key];
            }
        }
};

// Print helper
template<typename T> requires OpStreamable<T>
inline std::ostream &operator<<(std::ostream &os, const std::vector<T> &vec) {
    os << "[ ";
    for (const T &val: vec)
        os << val << ' ';
    os << ']';
    return os;
}

// Hashing helper - use with care
template<typename ...Args> requires (Hashable<Args> && ...)
struct TrivialHasher {
    inline std::size_t operator()(const Args &...args) const {
        std::size_t seed = 0;
        (..., (seed ^= std::hash<Args>{}(args) + 0x9e3779b9 + (seed << 6) + (seed >> 2)));
        return seed;
    }
};

// Silly functions for demo
int factorial(int n) { return n <= 2? n: n * factorial(n - 1); }
int add(int n1, int n2) { return n1 + n2; }
std::vector<int> initVector(std::size_t size, int initValue = 0) { return std::vector<int>(size, initValue); }

int main() {
    // Cache stats
    std::size_t hits, miss, counts;

    {
        Cache<int, int> cachedFactorial{factorial};
        int res {cachedFactorial(5)}; res = cachedFactorial(5);
        std::tie(hits, miss, counts) = cachedFactorial.stat();
        std::cout << res << ": (Hits: " << hits << ", Miss: " << miss << ", Size: " << counts << ")\n";
    }

    {
        Cache<int, int, int> cachedAdd{add, TrivialHasher<int, int>{}};
        int res {cachedAdd(1, 2)}; res = cachedAdd(1, 2);
        std::tie(hits, miss, counts) = cachedAdd.stat();
        std::cout << res << ": (Hits: " << hits << ", Miss: " << miss << ", Size: " << counts << ")\n";
    }

    {
        Cache<std::vector<int>, std::size_t, int> cachedInitVec{initVector, TrivialHasher<std::size_t, int>{}};
        std::vector<int> res {cachedInitVec(10, -1)}; res = cachedInitVec(10, -1);
        std::tie(hits, miss, counts) = cachedInitVec.stat();
        std::cout << res << ": (Hits: " << hits << ", Miss: " << miss << ", Size: " << counts << ")\n";
    }
}
