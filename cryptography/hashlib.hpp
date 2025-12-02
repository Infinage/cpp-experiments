#pragma once

#include <bitset>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace hashutil {
    template <std::size_t T>
    inline std::bitset<T> operator+(const std::bitset<T> &b1, const std::bitset<T> &b2) {
        return std::bitset<T>{b1.to_ullong() + b2.to_ullong()};
    }

    template <std::size_t T>
    inline std::bitset<T> rotate_left(const std::bitset<T> &b, std::size_t shift) {
        shift %= T;
        return (b << shift) | (b >> (T - shift));
    }

    [[nodiscard]] inline std::string sha1(const std::string &raw, bool asBytes = false) {
        // Define constants for SHA1
        std::bitset<32> h0{"01100111010001010010001100000001"};
        std::bitset<32> h1{"11101111110011011010101110001001"};
        std::bitset<32> h2{"10011000101110101101110011111110"};
        std::bitset<32> h3{"00010000001100100101010001110110"};
        std::bitset<32> h4{"11000011110100101110000111110000"};

        // Store the bits into a dynamic vector
        std::vector<bool> bitString;

        // Convert each char to 8 bit binary and append to a bool vector
        for (char ch: raw) {
            std::uint8_t byte {static_cast<std::uint8_t>(ch)};
            for (int bit = 7; bit >= 0; bit--) {
                bitString.push_back((byte >> bit) & 1);
            }
        }

        // Store the length to add to bit rep later on
        std::size_t bitStrLen {bitString.size()}; 

        // Append '1' to the bool vec
        bitString.push_back(1);

        // Pad until size % 512 == 448
        std::size_t padUntil448 {bitString.size() % 512};
        padUntil448 = {padUntil448 <= 448? 448 - padUntil448: (448 + 512) - padUntil448};
        for (std::size_t i {0}; i < padUntil448; i++)
            bitString.push_back(0);

        // Add the bitstring length
        std::bitset<64> bitStrLenInBits {bitStrLen};
        for (std::size_t i {64}; i-- > 0;)
            bitString.push_back(bitStrLenInBits[i]);

        // Split the vector<bool> into 512 chunks, each chunk further into 
        // 16 words each containing 32 bits (nested vector<vector<bitset>>)
        std::size_t nChunks {bitString.size() / 512};
        std::vector<std::vector<std::bitset<32>>> words;
        for (std::size_t i{0}; i < nChunks; i++) {
            words.push_back(std::vector<std::bitset<32>>(16, std::bitset<32>{})) ;
            for (std::size_t j {0}; j < 16; j++) {
                for (std::size_t k {0}; k < 32; k++) {
                    if (bitString[(i * 512) + (j * 32) + k])
                        words[i][j][31 - k] = 1;
                }
            }
        }

        // Convert 16 word nested vector into 80 word nested vector
        // using some bit wise logic formula applied on each inner vector
        for (std::vector<std::bitset<32>> &word: words) {
            for (std::size_t i {16}; i <= 79; i++) {
                // Take 4 words using i as reference
                std::bitset<32> w1 {word[i -  3]}, w2 {word[i -  8]}; 
                std::bitset<32> w3 {word[i - 14]}, w4 {word[i - 16]};

                // Xor all words
                std::bitset<32> xor_ {w1 ^ w2 ^ w3 ^ w4};

                // Left rotate by 1 and append result to array
                word.emplace_back(rotate_left(xor_, 1));
            }
        }

        // Meat of the actual algorithm
        for (std::size_t i {0}; i < nChunks; i++) {
            // Initialize values to constant values defined at top
            std::bitset<32> a {h0}, b {h1}, c {h2}, d {h3}, e {h4};
            for (std::size_t j {0}; j < 80; j++) {
                // init k & f based on where we are at the loop
                std::bitset<32> f, k;

                if (j < 20) {
                    k = std::bitset<32>{"01011010100000100111100110011001"}; 
                    f = (b & c) | (~b & d);
                } else if (j < 40) {
                    k = std::bitset<32>{"01101110110110011110101110100001"}; 
                    f = b ^ c ^ d;
                } else if (j < 60) {
                    k = std::bitset<32>{"10001111000110111011110011011100"}; 
                    f = (b & c) | (b & d) | (c & d);
                } else {
                    k = std::bitset<32>{"11001010011000101100000111010110"}; 
                    f = b ^ c ^ d;
                }

                // Execute regardless of j's position
                std::bitset<32> temp {rotate_left(a, 5) + f + e + k + words[i][j]};

                // Shift assign
                e = d; d = c; 
                c = rotate_left(b, 30);
                b = a; a = temp;
            }

            // Update the constants post iterating 80 times inside the loop
            h0 = h0 + a; h1 = h1 + b; h2 = h2 + c; h3 = h3 + d; h4 = h4 + e;
        }

        // Convert h0..h4 to hex string (40 char long ascii)
        if (!asBytes) {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0')
                << std::setw(8) << h0.to_ulong() 
                << std::setw(8) << h1.to_ulong() 
                << std::setw(8) << h2.to_ulong() 
                << std::setw(8) << h3.to_ulong() 
                << std::setw(8) << h4.to_ulong();
            return oss.str();
        }
        
        // Convert h0..h4 to hex string (40 char long ascii)
        else {
            std::string digest; digest.reserve(20);
            std::vector<unsigned long> parts {h0.to_ulong(), h1.to_ulong(), h2.to_ulong(), 
                h3.to_ulong(), h4.to_ulong()};
            for (unsigned long part: parts) {
                for (int i = 3; i >= 0; --i) {
                    digest.push_back(static_cast<char>((part >> (i * 8)) & 0xFF));
                }
            }
            return digest;
        }
    }
};
