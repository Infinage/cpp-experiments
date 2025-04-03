#pragma once

#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

class BigInt {
    private:
        static constexpr unsigned short BASE {10000};
        std::vector<unsigned short> data;
        bool negative{false};

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
                int result {compare(num1, num2)};
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
            for (std::size_t i {0}; i < length; i++) {
                int left {i < d1.size()? d1[i]: 0}, right {i < d2.size()? d2[i]: 0};
                int sum {left + right + carry};
                tempData.push_back(static_cast<unsigned short>(sum % BASE));
                carry = static_cast<unsigned short>(sum / BASE);
            }

            // Handle left overs
            if (carry) tempData.push_back(carry);
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

            return tempData;
        }

    public:
        BigInt() = default;
        BigInt(const char* str): BigInt(std::string_view(str)) {}

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
                    throw std::runtime_error("Not a valid input: " + std::string{str});

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
            if (data.empty()) return "0";

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
        inline bool operator< (const BigInt &other) { return compare(*this, other) == -1; }
        inline bool operator> (const BigInt &other) { return compare(*this, other) ==  1; }
        inline bool operator==(const BigInt &other) { return compare(*this, other) ==  0; }

        // Arithmetic ops
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
};
