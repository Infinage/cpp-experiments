#pragma once

#include <bit>
#include <bitset>
#include <string>
#include <vector>

namespace hashutil {
    template <std::size_t T>
    void extend(std::vector<bool> &vec, const std::bitset<T> &bits) {
        for (std::size_t i {bits.size()}; i-- > 0;)
            vec.push_back(bits[i]);
    }

    inline std::string sha1(const std::string &raw) {
        // Define constants for SHA1
        const constexpr std::bitset<32> H0{"01100111010001010010001100000001"};
        const constexpr std::bitset<32> H1{"11101111110011011010101110001001"};
        const constexpr std::bitset<32> H2{"10011000101110101101110011111110"};
        const constexpr std::bitset<32> H3{"00010000001100100101010001110110"};
        const constexpr std::bitset<32> H4{"11000011110100101110000111110000"};

        // Store the bits into a dynamic vector
        std::vector<bool> bitString;

        // Convert each char to 8 bit binary and append to a bool vector
        for (int _ch: raw) {
            std::bitset<8> bitRep {static_cast<std::size_t>(_ch)};
            extend(bitString, bitRep);
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
        extend(bitString, std::bitset<64>{bitStrLen});

        // Split the vector<bool> into 512 chunks, each chunk further into 
        // 16 words each containing 32 bits (nested vector<vector<bitset>>)
        std::size_t nChunks {bitString.size() / 512};
        std::vector<std::vector<std::bitset<32>>> words;
        for (std::size_t i{0}; i < nChunks; i++) {
            words.push_back(std::vector<std::bitset<32>>(16, std::bitset<32>{})) ;
            for (std::size_t j {0}; j < 16; j++) {
                for (std::size_t k {0}; k < 32; k++) {
                    if (bitString[(i * 512) + (j * 32) + k])
                        words[i][j][32 - j] = 1;
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
                unsigned long rotated = std::rotl(xor_.to_ulong(), 1);
                word.emplace_back(std::bitset<32>(rotated));
            }
        }

        // Meat of the actual algorithm
        for (std::size_t i {0}; i < nChunks; i++) {
            // Initialize values to constant values defined at top
            std::bitset<32> a {H0}, b {H1}, c {H2}, d {H3}, e {H4};

            for (std::size_t j {0}; j < 80; j++) {
                // init k & f based on where we are at the loop
                std::bitset<32> f, k;
                if (j < 20) {
                    k = std::bitset<32>{"01011010100000100111100110011001"}; 
                    f = (b & c) | (b.flip() & d);
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

                // TODO: Continue from line 99 - https://github.com/sbwheeler/SHA-1/blob/master/src/sha1.js
            }

        }

        return "";
    }
};
