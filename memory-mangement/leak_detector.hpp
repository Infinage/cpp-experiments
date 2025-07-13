#pragma once
#include <atomic>
#include <cstddef>
#include <cstdlib>

/********* Tracking memory usage using a Singleton *********/
class MemoryAccountant {
    private:
        std::atomic_uint64_t memory {0};
        MemoryAccountant() = default;

    public:
        // Delete copy & move operations
        MemoryAccountant(const MemoryAccountant&) = delete;
        MemoryAccountant &operator=(const MemoryAccountant&) = delete;
        MemoryAccountant(MemoryAccountant&&) = delete;
        MemoryAccountant &operator=(MemoryAccountant&&) = delete;

        static inline MemoryAccountant &get() {
            static MemoryAccountant accountant;
            return accountant;
        }

        inline std::uint64_t usage() const { return memory.load(); }
        inline void allocate(const std::size_t N) { memory += N; }
        inline void deallocate(const std::size_t N) { memory -= N; }
};

/********* Helpers to manage memory *********/
template<bool mayThrow>
inline void *allocateAndTrack(std::size_t N) noexcept(!mayThrow) {
    void *ptr {std::malloc(N + sizeof(std::max_align_t))};
    if (!ptr) {
        if constexpr (mayThrow) throw std::bad_alloc{};
        return nullptr;
    }
    new (ptr) std::size_t{N};
    MemoryAccountant::get().allocate(N);
    return static_cast<std::max_align_t*>(ptr) + 1;
}

inline void deallocateAndTrack(void *ptr) noexcept {
    if (!ptr) return;
    ptr = static_cast<std::max_align_t*>(ptr) - 1;
    std::size_t N {*static_cast<std::size_t*>(ptr)};
    MemoryAccountant::get().deallocate(N);
    std::free(ptr);
}

/*
// Overload all allocation operators - throwable versions
void *operator     new(std::size_t N) { return allocateAndTrack<true>(N); }
void *operator   new[](std::size_t N) { return allocateAndTrack<true>(N); }
void operator   delete(void *P) noexcept { deallocateAndTrack(P); }
void operator delete[](void *P) noexcept { deallocateAndTrack(P); }
void operator   delete(void *P, std::size_t) noexcept { deallocateAndTrack(P); }
void operator delete[](void *P, std::size_t) noexcept { deallocateAndTrack(P); }

// Overload all allocation operators - nothrow versions
void *operator     new(std::size_t N, const std::nothrow_t&) noexcept { return allocateAndTrack<false>(N); }
void *operator   new[](std::size_t N, const std::nothrow_t&) noexcept { return allocateAndTrack<false>(N); }
void operator   delete(void *P, const std::nothrow_t&) noexcept { deallocateAndTrack(P); }
void operator delete[](void *P, const std::nothrow_t&) noexcept { deallocateAndTrack(P); }
void operator   delete(void *P, std::size_t, const std::nothrow_t&) noexcept { deallocateAndTrack(P); }
void operator delete[](void *P, std::size_t, const std::nothrow_t&) noexcept { deallocateAndTrack(P); }
*/
