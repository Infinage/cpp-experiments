#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace hashutil {
    namespace impl {
        struct Sha1BlockFeeder {
            const std::string &bytes; std::size_t pos {};
            enum State {CopyBytes, Add80, ZeroPad, WriteLen, Finished} state {CopyBytes};

            constexpr bool nextBlock(std::array<uint8_t, 64> &chunk) {
                if (state == Finished) return false;
                std::size_t filledChars {}; chunk.fill(0);

                if (state == CopyBytes) {
                    filledChars = pos + 64 <= bytes.size()? 64: bytes.size() - pos;
                    auto beg {bytes.begin() + static_cast<long>(pos)}; 
                    auto end {beg + static_cast<long>(filledChars)};
                    std::copy(beg, end, chunk.begin());
                    if (pos + filledChars >= bytes.size()) state = Add80;
                }

                if (filledChars < 64 && state == Add80) { 
                    chunk[filledChars++] = 0x80; state = ZeroPad; 
                }

                if (filledChars < 64 && state == ZeroPad) {
                    filledChars = filledChars <= 56? 56: 64;
                    state = WriteLen;
                }

                if (filledChars < 64 && state == WriteLen) {
                    std::uint64_t bitLen {bytes.size() * 8ull};
                    for (int i = 7; i >= 0; --i)
                        chunk[filledChars++] = static_cast<uint8_t>(bitLen >> (i * 8));
                    state = Finished;
                }

                pos += filledChars; return true;
            }
        };
        
        class CRC32 {
            private:
                const std::uint32_t POLY = 0xedb88320;
                std::uint32_t crc = 0xFFFFFFFFu;

            public:
                constexpr std::uint32_t value() const { return crc ^ 0xFFFFFFFFu; }
                constexpr void reset() { crc = 0xFFFFFFFFu; }

                constexpr CRC32& update(std::string_view data) {
                    for (auto ch: data) {
                        crc ^= static_cast<std::uint8_t>(ch);
                        for (int k = 0; k < 8; k++)
                            crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
                    }
                    return *this;
                }
        };
    }

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
        // Define constants for SHA1 we will overwrite these for each chunk
        // Final sha string is just the concatenation of these values
        uint32_t h0{0x67452301};
        uint32_t h1{0xefcdab89};
        uint32_t h2{0x98badcfe};
        uint32_t h3{0x10325476};
        uint32_t h4{0xc3d2e1f0};

        // Process 64 chars at a time (chunk), each chunk is processed into 80 word blocks
        std::array<uint8_t, 64> chunk; std::array<uint32_t, 80> word;

        // Get and process the chunks
        impl::Sha1BlockFeeder blockFeeder {.bytes=raw};
        while (blockFeeder.nextBlock(chunk)) {
            for (std::size_t j {}; j < 16; ++j) {
                word[j] = (
                    static_cast<uint32_t>(chunk[(j * 4) + 0]) << 24 |
                    static_cast<uint32_t>(chunk[(j * 4) + 1]) << 16 |
                    static_cast<uint32_t>(chunk[(j * 4) + 2]) <<  8 |
                    static_cast<uint32_t>(chunk[(j * 4) + 3]) <<  0 
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
            out.append(x2s(h0, 8));
            out.append(x2s(h1, 8));
            out.append(x2s(h2, 8));
            out.append(x2s(h3, 8));
            out.append(x2s(h4, 8));
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

    [[nodiscard]] constexpr std::uint32_t crc32(std::string_view data) {
        impl::CRC32 crc;
        return crc.update(data).value();
    }
    
};

// ---------------------- TEST CASES (SHA1) ---------------------- //

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

// ---------------------- TEST CASES (CRC32) ---------------------- //

static_assert(hashutil::crc32("") == 0);
static_assert(hashutil::crc32("123456789") == 0xCBF43926u);
static_assert(hashutil::crc32("The quick brown fox jumps over the lazy dog") 
        == 0x414FA339u);
