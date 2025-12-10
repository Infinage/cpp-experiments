#include <chrono>
#include <print>
#include <string>

// ---------------------------------------------------------
// Toggle between implementations:
//   g++ -O3 -DVERSION=0 bench.cpp -o bench
//   g++ -O3 -DVERSION=1 bench.cpp -o bench
// ---------------------------------------------------------

#if defined(VERSION) && VERSION == 0
    #pragma message("Compiling with OLD SHA implementation")
    #include "hashlib-old.hpp"
#else
    #pragma message("Compiling with NEW SHA implementation")
    #include "hashlib.hpp"
#endif

std::uint64_t bench(auto &&fn, const std::string &data, std::size_t iters) {
    auto start {std::chrono::high_resolution_clock::now()};

    std::string out; out.reserve(40);
    for (std::size_t i = 0; i < iters; i++)
        out = fn(data, false);

    auto end {std::chrono::high_resolution_clock::now()};
    auto delta {std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()};
    return static_cast<uint64_t>(delta);
}

int main() {
    const std::size_t iters {5000};
    const std::string test1 {"The quick brown fox jumps over the lazy dog"};
    const std::string test2 (8 * 1024,  'A');    //   8 KB
    const std::string test3 (64 * 1024, 'B');    //  64 KB
    const std::string test4 (128 * 1024, 'C');   // 128 KB

    auto run {[&](const char *label, const std::string &input) {
        std::uint64_t us {bench(hashutil::sha1, input, iters)};
        std::println("{:15}: total {:10} us, per hash = {:10.3f} us",
                     label, us, double(us) / double(iters));
    }};

    run("short fixed", test1);
    run("8 KB",        test2);
    run("64 KB",       test3);
    run("128 KB",      test4);
}
