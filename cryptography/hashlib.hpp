#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace hashutil {
    constexpr char x2c(std::uint8_t val) { return "0123456789abcdef"[val]; }

    template<std::unsigned_integral t>
    constexpr std::string x2s(t val, std::size_t minWidth = 0) {
        // How many hex digits from converting the val
        std::size_t bits {sizeof(t) * 8}, hexDigits {bits / 4};
        minWidth = minWidth < hexDigits? hexDigits: minWidth;

        // Extract hex digits from MSB → LSB
        std::string out; out.reserve(hexDigits);
        for (std::size_t shift {(hexDigits - 1) * 4}; ; shift -= 4) {
            out.push_back(x2c((val >> shift) & 0xF));
            if (shift == 0) break;
        }

        // Trim leading zeros unless minWidth forces them
        std::size_t firstNonZero = out.find_first_not_of('0');
        if (firstNonZero == std::string::npos) out = "0";
        else out.erase(0, firstNonZero);

        // Pad on the left to minWidth
        if (out.size() < minWidth)
            out.insert(out.begin(), minWidth - out.size(), '0');

        return out;
    }

    constexpr uint32_t rotate_left(uint32_t b, unsigned shift) {
        shift = shift & 31u;
        return (b << shift) | (b >> (32u - shift));
    }

    [[nodiscard]] constexpr std::string sha1(const std::string &raw, bool asBytes = false) {
        // Define constants for SHA1
        uint32_t h0{0x67452301};
        uint32_t h1{0xefcdab89};
        uint32_t h2{0x98badcfe};
        uint32_t h3{0x10325476};
        uint32_t h4{0xc3d2e1f0};

        // Expected layout: [<msg bytes> <0x80> <8 byte msg len>]
        // Store the raw bytes of the input into a bytes vector
        std::vector<uint8_t> bytes {raw.begin(), raw.end()};

        // Append 128 (0x80) to the bytes vector
        bytes.push_back(0x80);

        // Pad until size % 64 == 56, so they can be processed as 
        // 64 byte chunks after combining with the 8 byte msg length
        while (bytes.size() % 64 != 56) bytes.push_back(0);

        // Add the bitstring length
        std::uint64_t bitLen {raw.size() * 8ull};
        for (int i = 7; i >= 0; --i)
            bytes.push_back(static_cast<uint8_t>(bitLen >> (i * 8)));

        // Split the vector<uint8_t> into 64 byte chunks, each chunk further into 
        // 16 words each containing 4 bytes (nested vector<array<uint32_t, 16>>)
        // Using these 16 words that we created, we create additional 80 - 16 words
        std::size_t nChunks {bytes.size() / 64};
        std::array<uint32_t, 80> word;
        for (std::size_t i {}; i < nChunks; ++i) {
            for (std::size_t j {}; j < 16; ++j) {
                std::size_t pos {(i * 64) + (j * 4)};
                word[j] = (
                    static_cast<uint32_t>(bytes[pos + 0]) << 24 |
                    static_cast<uint32_t>(bytes[pos + 1]) << 16 |
                    static_cast<uint32_t>(bytes[pos + 2]) <<  8 |
                    static_cast<uint32_t>(bytes[pos + 3]) <<  0 
                );
            }

            // Convert 16 word array into 80 worded array using bit wise formula
            for (std::size_t j{16}; j <= 79; j++) {
                word[j] = rotate_left(
                    word[j - 3] ^ word[j - 8] ^ word[j - 14] ^ word[j - 16], 
                    1
                );
            }

            // Meat of the actual algorithm --->
            // Initialize values to constant values defined at top
            uint32_t a {h0}, b {h1}, c {h2}, d {h3}, e {h4};
            for (std::size_t j {0}; j < 80; j++) {
                // init k & f based on where we are at the loop
                uint32_t f, k;

                if (j < 20) {
                    k = 0x5A827999; 
                    f = (b & c) | (~b & d);
                } else if (j < 40) {
                    k = 0x6ED9EBA1; 
                    f = b ^ c ^ d;
                } else if (j < 60) {
                    k = 0x8F1BBCDC; 
                    f = (b & c) | (b & d) | (c & d);
                } else {
                    k = 0xCA62C1D6; 
                    f = b ^ c ^ d;
                }

                // Execute regardless of j's position
                uint32_t temp {rotate_left(a, 5) + f + e + k + word[j]};

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
            std::string out; out.reserve(40);
            out.append_range(x2s(h0, 8));
            out.append_range(x2s(h1, 8));
            out.append_range(x2s(h2, 8));
            out.append_range(x2s(h3, 8));
            out.append_range(x2s(h4, 8));
            return out;
        }
        
        // Convert h0..h4 to hex string (40 char long ascii)
        else {
            std::string digest; digest.reserve(20);
            for (uint32_t part: {h0, h1, h2, h3, h4}) {
                for (int i = 3; i >= 0; --i) {
                    digest.push_back(static_cast<char>((part >> (i * 8)) & 0xFF));
                }
            }
            return digest;
        }
    }
};

// ---------------------- TEST CASES ---------------------- //

static_assert(hashutil::sha1("", false) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");

static_assert(hashutil::sha1("The quick brown fox jumps over the lazy dog", false) 
    == "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

static_assert(
    hashutil::sha1("", true) ==
    std::string{
        "\xda\x39\xa3\xee\x5e\x6b\x4b\x0d\x32\x55"
        "\xbf\xef\x95\x60\x18\x90\xaf\xd8\x07\x09",
        20
    }
);

static_assert(
    hashutil::sha1("The quick brown fox jumps over the lazy dog", true) ==
    std::string{
        "\x2f\xd4\xe1\xc6\x7a\x2d\x28\xfc\xed\x84"
        "\x9e\xe1\xbb\x76\xe7\x39\x1b\x93\xeb\x12",
        20
    }
);
