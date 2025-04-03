#pragma once

#include <concepts>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace BI {
    class BigInt {
        private:
            static constexpr unsigned short BASE {10000};

            // Store data and sign bit
            std::vector<unsigned short> data;
            bool negative{false};

            // Helper to clean 0s at the front - 000123
            static void clean(std::vector<unsigned short> &data) {
                while (!data.empty() && data.back() == 0)
                    data.pop_back();
            }

            // Private constructor
            BigInt(const std::vector<unsigned short> &data, bool negative = false): 
                data(data), negative(negative) {}

            static int absCompare(const BigInt &num1, const BigInt &num2) {
                std::vector<unsigned short> d1 {num1.data}, d2 {num2.data};
                std::size_t len1 {d1.size()}, len2 {d2.size()};
                if (len1 < len2) return -1;
                if (len1 > len2) return  1;
                for (std::size_t i {len1}; i-- > 0;) {
                    if (d1[i] < d2[i]) return -1;
                    if (d1[i] > d2[i]) return  1;
                }
                return 0;
            }

            static inline int compare(const BigInt &num1, const BigInt &num2) {
                bool neg1 {num1.negative}, neg2 {num2.negative};
                if (neg1 != neg2) return neg1? -1: 1;
                else {
                    int result {absCompare(num1, num2)};
                    if (result == 0 || !neg1) return result;
                    else return result == -1? 1: -1;
                }
            }

            static std::vector<unsigned short> add(const std::vector<unsigned short> &d1, 
                    const std::vector<unsigned short> &d2) 
            {
                unsigned short carry {0};
                std::size_t length {std::max(d1.size(), d2.size())};
                std::vector<unsigned short> tempData;
                for (std::size_t i {0}; i < length || carry; i++) {
                    int left {i < d1.size()? d1[i]: 0}, right {i < d2.size()? d2[i]: 0};
                    int sum {left + right + carry};
                    tempData.push_back(static_cast<unsigned short>(sum % BASE));
                    carry = static_cast<unsigned short>(sum / BASE);
                }

                return tempData;
            }

            // Assumes that d1 > d2
            static std::vector<unsigned short> sub(const std::vector<unsigned short> &d1, 
                    const std::vector<unsigned short> &d2) 
            {
                unsigned short carry {0};
                std::vector<unsigned short> tempData;
                for (std::size_t i {0}; i < d1.size(); i++) {
                    int left {d1[i]}, right {i < d2.size()? d2[i]: 0};
                    int sub {left - right - carry};
                    if (sub >= 0) carry = 0;
                    else { sub += BASE; carry = 1; }
                    tempData.push_back(static_cast<unsigned short>(sub));
                }

                clean(tempData);
                return tempData;
            }

            static std::vector<unsigned short> multiply(const std::vector<unsigned short> &d1, const std::vector<unsigned short> &d2) {
                std::vector<unsigned short> tempData(d1.size() + d2.size(), 0);
                for (std::size_t i {0}; i < d1.size(); i++) {
                    unsigned short carry {0};
                    for (std::size_t j {0}; j < d2.size() || carry; j++) {
                        unsigned int prod {static_cast<unsigned int>(tempData[i + j] 
                                + d1[i] * (j < d2.size()? d2[j]: 0) + carry)};
                        tempData[i + j] = prod % BASE; 
                        carry = static_cast<unsigned short>(prod / BASE);
                    }
                }

                // Remove extra 0s from the end
                while (!tempData.empty() && tempData.back() == 0)
                    tempData.pop_back();

                clean(tempData);
                return tempData;
            }

            static std::pair<std::vector<unsigned short>, std::vector<unsigned short>> div(
                const std::vector<unsigned short> &d1, const std::vector<unsigned short> &d2)
            {
                // Double check non zero
                if (d2.empty()) throw std::runtime_error("Division by zero.");

                // Result placeholders
                std::vector<unsigned short> quotient, remainder;
                remainder.reserve(d1.size());
                for (std::size_t i {d1.size()}; i-- > 0;) {
                    remainder.insert(remainder.begin(), d1[i]);

                    // If remainder < d2, add 0 to quotient and 
                    // continue to add next chunk
                    if (absCompare(remainder, d2) == -1) {
                        quotient.insert(quotient.begin(), 0);
                        continue;
                    }

                    // Binary search to approximate the quotient
                    std::vector<unsigned short> prod;
                    unsigned short low {0}, high {9999}, quot;
                    while (low <= high) {
                        unsigned short mid {static_cast<unsigned short>((low + high) / 2)};
                        std::vector<unsigned short> temp {multiply(d2, {mid})};
                        int cmp {absCompare(temp, remainder)};
                        if (cmp <= 0) {
                            quot = mid; 
                            low = mid + 1; 
                            prod = temp;
                        } 

                        else {
                            high = mid - 1;
                        }
                    }

                    // Add result to quotient
                    quotient.insert(quotient.begin(), quot);

                    // Sub from remainder
                    remainder = sub(remainder, prod);
                }

                clean(quotient); clean(remainder);
                return {quotient, remainder};
            }

        public:
            inline bool empty() const { 
                for (const unsigned short &num: data)
                    if (num > 0) return false;
                return true;
            }

            BigInt() = default;
            BigInt(const char* str): BigInt(std::string_view(str)) {}

            // Move Constructor
            BigInt(BigInt &&other) noexcept: 
                data(std::move(other.data)), 
                negative(other.negative) 
            {}

            // Move assignment
            BigInt& operator=(BigInt &&other) noexcept {
                if (this != &other) {
                    data = std::move(other.data);
                    negative = other.negative;
                }
                return *this;
            }

            // Copy constructor
            BigInt(const BigInt &other) noexcept:
                data(other.data), negative(other.negative) {}

            // Templated constructor
            template<typename T> requires (std::integral<T> && !std::is_same_v<T, bool>)
            explicit BigInt(T num): negative(num < 0) {
                num = std::abs(num);
                while (num) {
                    data.push_back(num % BASE);
                    num /= BASE;
                }
            }

            // Recommended constructor
            BigInt(std::string_view str) {
                // Return early
                if (str.empty()) return;

                // Check sign explicitly set
                if (!str.empty() && (str[0] == '-' || str[0] == '+')) {
                    if (str[0] == '-') negative = true;
                    str = str.substr(1);
                }

                // Process the rest as chunks
                unsigned short acc {0}, pow {1};
                for (std::size_t i {str.size()}; i-- > 0;) {
                    if (!std::isdigit(str[i])) 
                        throw std::runtime_error("Not a valid Big Int: " + std::string{str});

                    acc += pow * (str[i] - '0');
                    pow *= 10;
                    if (pow == BASE) {
                        data.push_back(acc);
                        acc = 0; pow = 1;
                    }
                }

                // Append last piece if any left
                if (acc) data.push_back(acc);
            }

            inline operator std::string() const {
                // Return early
                if (empty()) return "0";

                std::ostringstream oss;
                for (unsigned short num: data) {
                    std::string temp(4, '0'); std::size_t curr {0};
                    while (num) {
                        temp[curr++] = '0' + num % 10;
                        num /= 10;
                    }
                    oss << temp;
                }

                // Pop out any extra 0s at the front (string is 
                // constructed here in the rev order tho)
                std::string result {oss.str()};
                while (!result.empty() && result.back() == '0')
                    result.pop_back();

                // Append the sign and reverse the entire result
                if (negative) result.push_back('-');
                std::reverse(result.begin(), result.end());

                return result;
            }

            friend inline std::ostream &operator<<(std::ostream &os, const BigInt &num) {
                os << static_cast<std::string>(num);
                return os;
            }

            // Unary operators
            inline BigInt operator-() const { return BigInt{data, !negative}; }
            inline BigInt operator+() const { return BigInt{data,  negative}; }

            // Comparators
            inline bool operator> (const BigInt &other) { return compare(*this, other) ==  1; }
            inline bool operator>=(const BigInt &other) { return compare(*this, other) >=  0; }
            inline bool operator==(const BigInt &other) { return compare(*this, other) ==  0; }
            inline bool operator<=(const BigInt &other) { return compare(*this, other) <=  0; }
            inline bool operator< (const BigInt &other) { return compare(*this, other) == -1; }

            // Arithmetic op - Add
            inline BigInt operator+(const BigInt &other) const {
                if (negative == other.negative) {
                    return BigInt{add(data, other.data), negative};
                } else {
                    int cmp {absCompare(data, other.data)};
                    if (cmp == 0) return BigInt{};
                    else
                        return BigInt{
                            sub(
                                cmp == 1? data: other.data, 
                                cmp == 1? other.data: data
                            ),  cmp == 1? negative: other.negative
                        };
                }
            }

            // Arithmetic op - Sub
            inline BigInt operator-(const BigInt &other) const {
                if (negative != other.negative) {
                    return BigInt{add(data, other.data), negative};
                } else {
                    int cmp {absCompare(data, other.data)};
                    if (cmp == 0) return BigInt{};
                    else
                        return BigInt{
                            sub(
                                cmp == 1? data: other.data, 
                                cmp == 1? other.data: data
                            ),  cmp == 1? negative: !other.negative
                        };
                }
            }

            // Arithmetic op - Mul
            inline BigInt operator*(const BigInt &other) const {
                return BigInt{
                    multiply(data, other.data), 
                    static_cast<bool>(negative ^ other.negative)
                };
            }

            // Arithmetic op - Div
            inline BigInt operator/(const BigInt &other) const {
                int cmp {absCompare(*this, other)};
                bool resNeg {static_cast<bool>(negative ^ other.negative)};
                if (other.empty()) throw std::runtime_error("Division by zero.");
                else if (cmp ==  0) return BigInt{{1}, resNeg};
                else if (cmp == -1) return BigInt{};
                else {
                    std::vector<unsigned short> quotient, remainder;
                    std::tie(quotient, remainder) = div(data, other.data);
                    return BigInt{quotient, resNeg};
                }
            }

            // Arithmetic op - Mod
            inline BigInt operator%(const BigInt &other) const {
                int cmp {absCompare(*this, other)};
                if (other.empty()) throw std::runtime_error("Division by zero.");
                else if (cmp ==  0) return {};
                else if (cmp == -1) return {data, negative};
                else {
                    std::vector<unsigned short> quotient, remainder;
                    std::tie(quotient, remainder) = div(data, other.data);
                    return {remainder, negative};
                }
            }

    };

    // User defined literals
    inline BigInt operator"" _bi(const char* num) {
        return BigInt{std::string_view{num}};
    }
}
