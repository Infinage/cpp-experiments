#include <chrono>
#include <cstdlib>
#include <mutex>
#include <print>
#include <vector>

// Flag to enable or disable custom allocation
#ifdef ENABLE_ARENA_MACRO
constexpr bool ENABLE_ARENA = true;
#else
constexpr bool ENABLE_ARENA = false;
#endif

// Constants to drive the test code
constexpr std::size_t N_ORCS {1'000'000};
constexpr std::size_t THRESHOLD {25 * 1024 * 1024};

// Helper to check if N is power of 2
constexpr bool powerOf2(std::size_t n) {
    return (n > 0) && ((n & (n - 1)) == 0);
}

// Template with specialization to test Custom & Std Lib
template<typename T, std::size_t MAX_N, bool Enable = true>
requires(std::is_final_v<T>) 
class SizeBasedArena;

// Spl for Custom implementation
template<typename T, std::size_t MAX_N>
class SizeBasedArena<T, MAX_N, true> {
    public:
        // Minimum worst case bytes required
        // If multiple align adjustments are being made
        // this might actually be undershooting
        constexpr static std::size_t BYTES_REQUIRED 
        {(sizeof(T) * MAX_N) + alignof(T) - 1};

        // Delete copy operators
        SizeBasedArena(const SizeBasedArena&) = delete;
        SizeBasedArena &operator=(const SizeBasedArena&) = delete;
        ~SizeBasedArena() { std::free(BLOCK_START); }

        // Singleton access
        static SizeBasedArena &get() { return singleton; }

        // Using `std::align` ensures that each allocation respects the 
        // required alignment of T. If sizeof(T) is a power of 2 (or rounded 
        // up to one), we could skip alignment, but that would waste memory 
        // and break if T has a stricter "custom" alignment.
        // `std::align` handles both typical and edge cases

        void *allocate(const std::size_t N = 1) {
            std::lock_guard lock{mutex};

            // Try aligning the current pointer (CURR) within the available space.
            // `allocStart` is updated in-place if alignment succeeds.
            // `spaceAvailable` is adjusted to reflect remaining usable space.
            // Returns nullptr if alignment fails.
            void *allocStart {CURR};
            const std::size_t bytesToAllocate {sizeof(T) * N};
            std::size_t spaceAvailable {static_cast<std::size_t>(BLOCK_END - CURR)};
            
            if (!std::align(alignof(T), bytesToAllocate, allocStart, spaceAvailable))
                throw std::bad_alloc{};

            // Move CURR past the allocated and aligned block
            CURR = static_cast<char*>(allocStart) + bytesToAllocate;
            return allocStart;
        }

    private:
        // Sanity checks
        static_assert(MAX_N > 0, "Arena size must be atleast 1");
        static_assert(powerOf2(alignof(T)), "Alignment must be a power of 2");
        static_assert(BYTES_REQUIRED <= THRESHOLD, "Buffer size exceeds threshold limit");

        // Singleton access
        static SizeBasedArena singleton;

        // Mutex to ensure alloc ops are thread safe
        std::mutex mutex;

        // Private Default CTOR - Allocates memory block per template params
        char *BLOCK_START {nullptr}, *BLOCK_END {nullptr}, *CURR {nullptr};
        SizeBasedArena(): 
        BLOCK_START(static_cast<char*>(std::malloc(BYTES_REQUIRED))), 
        CURR(BLOCK_START) {
            if (BLOCK_START == nullptr) throw std::bad_alloc{}; 
            BLOCK_END = BLOCK_START + BYTES_REQUIRED; }
};

// Spl for Std lib version - do nothing
template<typename T, std::size_t MAX_N>
class SizeBasedArena<T, MAX_N, false> {
    static SizeBasedArena singleton; // never used
};

// Template with specialization to test Custom & Std Lib
template<bool CustomAlloc> class Orc;

// Spl for Custom implementation
template<> class Orc<true> final {
    private:
        [[maybe_unused]] char name[5] {"Umph"};
        [[maybe_unused]] int strength {100};
        [[maybe_unused]] double smell {1000.};

    public:
        static constexpr std::size_t MAX_ORCS {N_ORCS};

        // Override new operators -----
        void *operator new(const std::size_t N) {
            return SizeBasedArena<Orc, MAX_ORCS>::get().allocate(N / sizeof(Orc));
        }

        void *operator new[](const std::size_t N) {
            return SizeBasedArena<Orc, MAX_ORCS>::get().allocate(N / sizeof(Orc));
        }

        // Override delete operators with noop 
        // Arena takes care of deletion on its finalization
        void operator delete(void*) noexcept {}
        void operator delete[](void*) noexcept {}
};

// Spl for Std lib version - do nothing
template<> class Orc<false> final {
    private:
        [[maybe_unused]] char name[5] {"Umph"};
        [[maybe_unused]] int strength {100};
        [[maybe_unused]] double smell {1000.};

    public:
        static constexpr std::size_t MAX_ORCS {N_ORCS};
};

// Init the static variable (dummy init when `ENABLE_ARENA` is false)
using ORC = Orc<ENABLE_ARENA>;
template<> SizeBasedArena<ORC, ORC::MAX_ORCS, ENABLE_ARENA> 
SizeBasedArena<ORC, ORC::MAX_ORCS, ENABLE_ARENA>::singleton{};

// Test driver code
int main() {
    // Ensure vector init doesn't affect benchmarking
    std::vector<ORC*> orcs(ORC::MAX_ORCS, nullptr);

    // Time the allocation
    auto new_timer_start {std::chrono::steady_clock::now()};
    for (std::size_t i {0}; i < ORC::MAX_ORCS; ++i) orcs[i] = new ORC;
    auto new_delta {std::chrono::steady_clock::now() - new_timer_start};

    // Time the deallocation
    auto del_timer_start {std::chrono::steady_clock::now()};
    for (auto orc: orcs) delete orc;
    auto del_delta {std::chrono::steady_clock::now() - del_timer_start};

    std::println(ENABLE_ARENA? "Homemade version": "STANDARD LIBRARY VERSION");
    std::println("Construction: {} orcs in {}", orcs.size(), 
        std::chrono::duration_cast<std::chrono::microseconds>(new_delta));
    std::println("Destruction: {} orcs in {}", orcs.size(), 
        std::chrono::duration_cast<std::chrono::microseconds>(del_delta));
}
