#include "../include/utils.hpp"
#include <ranges>

[[nodiscard]] std::string readTextFile(const fs::path &path) {
    std::ifstream ifs{path};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

void writeTextFile(const std::string &data, const fs::path &path) {
    std::ofstream ofs{path, std::ios::trunc};
    if (!ofs) throw std::runtime_error("Failed to open file for writing: " + path.string());
    ofs << data;
    if (!ofs) throw std::runtime_error("Failed to write to file: " + path.string());
}

[[nodiscard]] std::string sha2Binary(std::string_view sha) {
    std::string binSha;
    binSha.reserve(20);
    for (std::size_t i {0}; i < 40; i += 2) {
        std::string hexDigit {sha.substr(i, 2)};
        binSha.push_back(static_cast<char>(std::stoi(hexDigit, nullptr, 16)));
    }
    return binSha;
}

[[nodiscard]] std::string binary2Sha(std::string_view binSha) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const char &ch: binSha)
        oss << std::setw(2) << static_cast<int>(static_cast<unsigned char>(ch));
    return oss.str();
}

std::string trim(const std::string &str) {
    return 
        str 
        | std::views::drop_while(::isspace) 
        | std::views::reverse 
        | std::views::drop_while(::isspace) 
        | std::views::reverse
        | std::ranges::to<std::string>();
}
