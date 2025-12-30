#include "../cryptography/hashlib.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <unordered_map>

class Bitcask {
    private:
        static std::uint32_t timestamp() {
            namespace cr = std::chrono;
            auto clkNow = cr::system_clock::now();
            auto seconds = cr::duration_cast<cr::seconds>(clkNow.time_since_epoch());
            return static_cast<std::uint32_t>(seconds.count());
        }

        // For consistency across platforms, ensure data is written as little endian
        [[nodiscard]] static auto bswap(std::integral auto val) {
            if constexpr (std::endian::native == std::endian::big)
                return std::byteswap(val);
            return val;
        }

    private:
        struct KeyDirValue { std::uint32_t id, vsize, vpos, tstamp; };
        static constexpr std::size_t logSize = 1000000000u * 2;
        const std::filesystem::path datastorePath;

        std::unordered_map<std::string, KeyDirValue> keyDir;
        std::uint32_t currId {}; std::ofstream currFile;

    private:
        struct FileEntry {
            std::uint32_t tstamp, ksz, vsz, vstart; 
            std::string key, val;

            FileEntry() = default;

            FileEntry(std::string key, std::string val, long fsz):
                tstamp {timestamp()}, 
                ksz {static_cast<uint32_t>(key.size())},
                vsz {static_cast<uint32_t>(val.size())}, 
                vstart {static_cast<std::uint32_t>(fsz) + 16 + ksz},
                key {std::move(key)}, val {std::move(val)} {}

            [[nodiscard]] static FileEntry tombstone(const std::string &key) {
                auto max = std::numeric_limits<std::uint32_t>::max();
                FileEntry res;
                res.tstamp = res.vstart = max; 
                res.val = ""; res.key = key; res.vsz = 0u;
                res.ksz = static_cast<std::uint32_t>(key.size());
                return res;
            }

            [[nodiscard]] bool isTombstone() {
                auto max = std::numeric_limits<std::uint32_t>::max();
                return tstamp == max && vsz == 0u && val.empty();
            }

            void write(std::ofstream &ofs) const {
                std::uint32_t crc_ = bswap(computeCRC()), tstamp_ = bswap(tstamp), 
                    ksz_ = bswap(ksz), vsz_ = bswap(vsz);
                ofs.write(reinterpret_cast<const char*>(&crc_), 4);
                ofs.write(reinterpret_cast<const char*>(&tstamp_), 4);
                ofs.write(reinterpret_cast<const char*>(&ksz_), 4);
                ofs.write(reinterpret_cast<const char*>(&vsz_), 4);
                ofs.write(key.c_str(), static_cast<std::streamoff>(key.size()));
                ofs.write(val.c_str(), static_cast<std::streamoff>(val.size()));
            }

            static std::optional<FileEntry> read(std::ifstream &ifs) {
                FileEntry res;

                // Load fixed size values
                std::uint32_t crc, ints[4];
                ifs.read(reinterpret_cast<char*>(ints), 16);
                if (ifs.bad() || ifs.gcount() != 16) return std::nullopt;
                crc = bswap(ints[0]), res.tstamp = bswap(ints[1]), res.ksz = bswap(ints[2]), 
                    res.vsz = bswap(ints[3]);

                // Parse the Key
                res.key = std::string(res.ksz, 0);
                ifs.read(res.key.data(), res.ksz);
                if (ifs.bad() || ifs.gcount() != res.ksz) 
                    return std::nullopt;

                // Compute and store the current position on file (start of val)
                res.vstart = static_cast<std::uint32_t>(ifs.tellg());

                // Read the Value
                res.val = std::string(res.vsz, 0);
                ifs.read(res.val.data(), res.vsz);
                if (ifs.bad() || ifs.gcount() != res.vsz) 
                    return std::nullopt;

                // Validate entry against CRC
                return res.computeCRC() == crc? std::optional{res}: std::nullopt;
            }

            [[nodiscard]] KeyDirValue kd(std::uint32_t id) const { 
                return {id, vsz, vstart, tstamp}; 
            }

            std::uint32_t computeCRC() const {
                char intRepr[12] {};
                std::memcpy(intRepr + 0, reinterpret_cast<const char*>(&tstamp), 4);
                std::memcpy(intRepr + 4, reinterpret_cast<const char*>(&ksz), 4);
                std::memcpy(intRepr + 8, reinterpret_cast<const char*>(&vsz), 4);

                hashutil::impl::CRC32 crc32;
                crc32.update({intRepr, 12});
                crc32.update(key).update(val);

                return crc32.value();
            }
        };

    private:
        [[nodiscard]] bool loadFromFile(const std::filesystem::path &path) {
            std::ifstream ifs {path, std::ios::binary};

            std::uint32_t id;
            std::string idStr = path.filename().extension();
            if (!idStr.empty()) idStr = idStr.substr(1);
            auto parseResult {std::from_chars(idStr.data(), idStr.data() + idStr.size(), id)};
            if (parseResult.ec != std::errc() || parseResult.ptr != idStr.c_str() + idStr.size())
                return false;

            while (ifs) {
                auto newEntry = FileEntry::read(ifs);
                if (ifs.eof()) break;
                else if (!newEntry) return false;
                else if (newEntry->isTombstone()) keyDir.erase(newEntry->key);
                else { // Insert if entry's timestamp exceeds latest entry
                    auto keyIt = keyDir.find(newEntry->key);
                    if (keyIt == keyDir.end() || keyIt->second.tstamp < newEntry->tstamp) {
                        keyDir.insert_or_assign(newEntry->key, newEntry->kd(id));
                    }
                }
            }

            // Keep track of latest id num seen so far
            currId = std::max(currId, ifs.tellg() < static_cast<long>(logSize)? id: id + 1);

            return true;
        }

        // If log file size exceeded incr to next log file
        void ensureLogSizeLimits() {
            if (currFile.tellp() >= static_cast<long>(logSize)) {
                auto currFilePath = datastorePath / ("cask." + std::to_string(++currId));
                currFile = std::ofstream {currFilePath, std::ios::binary | std::ios::app};
            }
        }

    public:
        Bitcask(const std::filesystem::path &datastorePath): 
            datastorePath {datastorePath} 
        {
            if (!std::filesystem::exists(datastorePath))
                std::filesystem::create_directories(datastorePath);

            if (!std::filesystem::is_directory(datastorePath))
                throw std::runtime_error("Input path is not a directory: " + 
                    datastorePath.string());

            // Read all cask.<id> files from the datastore path
            for (auto &entry: std::filesystem::directory_iterator {datastorePath}) {
                auto filePath = entry.path();
                if (entry.is_regular_file() && filePath.filename().string().starts_with("cask")) {
                    auto before = keyDir.size();
                    if (!loadFromFile(filePath))
                        std::println("Load from {} was unsuccessful", filePath.c_str());
                    auto diff = static_cast<long>(keyDir.size()) - static_cast<long>(before);
                    std::println("Loaded {} entries from File: {}", diff, filePath.c_str());
                }
            }

            // Load latest file into memory
            auto currFilePath = datastorePath / ("cask." + std::to_string(currId));
            currFile = std::ofstream {currFilePath, std::ios::binary | std::ios::app};
        }

        void set(const std::string &key, const std::string &value) {
            FileEntry entry {key, value, currFile.tellp()};
            keyDir.insert_or_assign(key, entry.kd(currId));
            entry.write(currFile);
            ensureLogSizeLimits();
        }

        std::optional<std::string> get(const std::string &key) {
            auto keyDirIT = keyDir.find(key);    
            if (keyDirIT == keyDir.end()) return std::nullopt;

            auto kdVal = keyDirIT->second;
            if (kdVal.id == currId) currFile.flush();
            std::ifstream ifs {datastorePath / ("cask." + std::to_string(kdVal.id)), std::ios::binary};
            ifs.seekg(kdVal.vpos);

            std::string val(kdVal.vsize, 0);
            ifs.read(val.data(), kdVal.vsize);
            return val;
        }

        std::optional<std::string> erase(const std::string &key) {
            // Store the entry for returning to user
            auto val = get(key);

            // Write a tombstone value to mark the deletion
            auto entry = FileEntry::tombstone(key);
            entry.write(currFile);
            ensureLogSizeLimits();

            // Erase from inmemory struct
            keyDir.erase(key);
            return val;
        }
};

std::vector<std::string> split(const std::string &str) {
    std::vector<std::string> strings;
    std::istringstream iss {str};
    std::string word;
    while (iss >> word)
        strings.push_back(word);
    return strings;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::println("Usage: bitcask <datastore-path>");
        return 1;
    }

    std::filesystem::path datastorePath {argv[1]};
    Bitcask bc {datastorePath};

    std::string replHelp = "Commands: set, get, del, merge, quit\n"
        "\n set: Insert a key-value pair"
        "\n get: Retreive a value using key"
        "\n del: Delete a key-value pair"
        "\n merge: Clean up log files"
        "\n";
    ;

    std::string input;
    while (std::print(">> "), std::getline(std::cin, input)) {
        if (input == "quit" || input == "exit") break;

        else if (input.starts_with("set")) {
            auto inputs = split(input);
            if (inputs.size() == 3) bc.set(inputs[1], inputs[2]);
            else std::println("Expected syntax: set <key> <value>");
        }

        else if (input.starts_with("get") || input.starts_with("del")) {
            auto inputs = split(input);
            if (inputs.size() != 2)
                std::println("Expected syntax: get/del <key>");
            else {
                auto val = input.starts_with("get")? 
                    bc.get(inputs[1]): bc.erase(inputs[1]);
                std::println("{}", val? *val: "(nil)");
            }
        }

        else if (input == "help") {
            std::println("{}", replHelp);
        }

        else {
            std::println("Unrecognized command: {}", input);
        }
    }
}
