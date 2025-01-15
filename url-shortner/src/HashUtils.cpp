#include "../include/HashUtils.hpp"

#include <random>

std::vector<std::uint8_t> generateKey(const std::string &key, const std::size_t length) {
    std::seed_seq seed(key.begin(), key.end());
    std::mt19937 generator(seed);
    std::vector<std::uint8_t> keyVec(length);
    for (std::size_t i {0}; i < length; i++)
        keyVec[i] = static_cast<std::uint8_t>(generator() % 256);
    return keyVec;
}

std::string encryptSizeT(std::size_t value, const std::string &key) {
    std::size_t valSize {sizeof(std::size_t)};
    std::uint8_t* valueBytes {reinterpret_cast<std::uint8_t*>(&value)};
    std::vector<std::uint8_t> keyVec {generateKey(key, valSize)}, result {valueBytes, valueBytes + valSize};
    for (std::size_t i {0}; i < valSize; i++)
        result[i] ^= keyVec[i];
    return base62EncodeBytes(result);
}

std::string base62Encode(std::size_t value) {
    constexpr const char* base62_chars {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
    std::string result;
    while (value > 0) {
        result = base62_chars[value % 62] + result;
        value /= 62;
    }
    return result;
}

std::string base62EncodeBytes(std::vector<std::uint8_t> &bytes) {
    std::size_t combined {0};
    for (std::uint8_t byte: bytes) 
        combined = (combined << 8) | byte;
    return base62Encode(combined);
}
