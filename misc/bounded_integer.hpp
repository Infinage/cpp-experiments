#include <concepts>
#include <limits>

namespace stdx {
    template<std::unsigned_integral T = unsigned>
    struct Bounded {
        static constexpr T tmax {std::numeric_limits<T>::max()};
        T max, val; 

        constexpr Bounded(T max_, T val_ = {}): max {max_}, val {val_} { 
            if (val > max_) val %= max_ + 1;
        }

        constexpr operator T() const { return val; }

        constexpr Bounded &operator+=(T other) {
            if (max == tmax) val += other;
            else {
                T delta {max - val}; other %= max + 1;
                val = other <= delta? other + val: other - delta - 1;
            }
            return *this;
        }

        constexpr Bounded &operator-=(T other) {
            if (max == tmax) val -= other;
            else {
                other %= max + 1;
                val = val >= other? val - other: max - (other - val) + 1;
            }
            return *this;
        }

        constexpr Bounded operator-(T other) const {
            Bounded temp{max, val}; temp -= other;
            return temp;
        }

        constexpr Bounded operator+(T other) const {
            Bounded temp{max, val}; temp += other;
            return temp;
        }
    };
}

// Edge 1: val = 0
static_assert(stdx::Bounded{10u, 0u} + 0u == 0);
static_assert(stdx::Bounded{10u, 0u} - 0u == 0);
static_assert(stdx::Bounded{10u, 0u} - 1u == 10);

// Edge 2: val = max
static_assert(stdx::Bounded{10u, 10u} + 0u == 10);
static_assert(stdx::Bounded{10u, 10u} + 1u == 0);
static_assert(stdx::Bounded{10u, 10u} - 0u == 10);
static_assert(stdx::Bounded{10u, 10u} - 10u == 0);
static_assert(stdx::Bounded{10u, 10u} - 11u == 10);

// Edge 3: max = tmax
static_assert(stdx::Bounded{std::numeric_limits<unsigned>::max(), 0u} + 1u == 1);
static_assert(stdx::Bounded{std::numeric_limits<unsigned>::max(), 1u} - 1u == 0);
static_assert(stdx::Bounded{std::numeric_limits<unsigned>::max(), std::numeric_limits<unsigned>::max()} + 1u == 0);

// Edge 4: other > max
static_assert(stdx::Bounded{10u, 3u} + 25u == 6);
static_assert(stdx::Bounded{10u, 8u} - 15u == 4);

// Edge 5: other == max + 1
static_assert(stdx::Bounded{10u, 5u} + 11u == 5);
static_assert(stdx::Bounded{10u, 5u} - 11u == 5);

// Edge 6: other == 0
static_assert(stdx::Bounded{10u, 5u} + 0u == 5);
static_assert(stdx::Bounded{10u, 5u} - 0u == 5);
