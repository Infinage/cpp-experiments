#include <cstdint>
#include <string>
#include <vector>

std::vector<std::uint8_t> generateKey(const std::string &key, const std::size_t length);
std::vector<std::uint8_t> encryptSizeT(std::size_t value, const std::string &key);
std::string base62Encode(std::size_t value);
std::string base62EncodeBytes(std::vector<std::uint8_t> &bytes);
