#pragma once

#include <fstream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Reads the entire contents of a text file into a string.
// Assumes the file is encoded in a compatible text format.
[[nodiscard]] std::string readTextFile(const fs::path &path);

// Writes a string to a text file, overwriting any existing content.
void writeTextFile(const std::string &data, const fs::path &path);

// Disallow passing a string as a path to prevent accidental overload resolution
void writeTextFile(const std::string&, const std::string&) = delete;

// Converts a SHA-1 hexadecimal string (40 chars) to a 20-byte binary representation.
[[nodiscard]] std::string sha2Binary(std::string_view sha);

// Converts a 20-byte binary SHA-1 hash to a 40-character hexadecimal string.
[[nodiscard]] std::string binary2Sha(std::string_view binSha);

// Removes leading and trailing whitespace from a string using `std::ranges`.
std::string trim(const std::string &str);

// Util to read input as Big Endian int*
// Git uses BigEndian for a lot of its binary file formats
// This helper is system independent and works regardless 
// of the endianness of the client
template<typename T> requires std::integral<T>
void readBigEndian(std::ifstream &ifs, T &val) {
    using UT = std::make_unsigned_t<T>;
    unsigned char buffer[sizeof(T)];
    ifs.read(reinterpret_cast<char *>(&buffer), sizeof(T));
    val = 0;
    for (size_t i = 0; i < sizeof(T); ++i)
        val = static_cast<UT>((val << 8) | buffer[i]);
}

// Utils to write input as Big Endian int*
// Independent of the endianness of the client
template <typename T> requires std::integral<T>
void writeBigEndian(std::ofstream &ofs, T val) {
    unsigned char buffer[sizeof(T)];
    for (size_t i = 0; i < sizeof(T); ++i) {
        buffer[sizeof(T) - 1 - i] = val & 0xFF;
        val >>= 8;
    }
    ofs.write(reinterpret_cast<char *>(buffer), sizeof(T));
}

