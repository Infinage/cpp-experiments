#include "../cryptography/hashlib.hpp"

#include <algorithm>
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
            auto milliseconds = cr::duration_cast<cr::milliseconds>(clkNow.time_since_epoch());
            return static_cast<std::uint32_t>(milliseconds.count());
        }

        // For consistency across platforms, ensure data is written as little endian
        [[nodiscard]] static auto bswap(std::integral auto val) {
            if constexpr (std::endian::native == std::endian::big)
                return std::byteswap(val);
            return val;
        }

        template<typename ...Args>
        static void inplace_bswap(Args &...args) { ((args = bswap(args)), ...); }
        

        // Ensure that int values passed here are in correct endian form
        static std::uint32_t computeCRC(std::uint32_t tstamp, std::uint32_t ksz, 
            std::uint32_t vsz, const std::string &key, const std::string &val) 
        {
            char intRepr[12] {};
            std::memcpy(intRepr + 0, reinterpret_cast<const char*>(&tstamp), 4);
            std::memcpy(intRepr + 4, reinterpret_cast<const char*>(&ksz), 4);
            std::memcpy(intRepr + 8, reinterpret_cast<const char*>(&vsz), 4);

            hashutil::impl::CRC32 crc32;
            crc32.update({intRepr, 12});
            crc32.update(key).update(val);

            return crc32.value();
        }

    private:
        struct KeyDirValue { std::uint32_t id, vsize, vpos, tstamp; };
        static constexpr std::size_t logSize = 1000000000u * 2;
        std::filesystem::path datastorePath;

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
                res.tstamp = max; res.vstart = 0;
                res.val = ""; res.key = key; res.vsz = 0u;
                res.ksz = static_cast<std::uint32_t>(key.size());
                return res;
            }

            [[nodiscard]] bool isTombstone() {
                auto max = std::numeric_limits<std::uint32_t>::max();
                return tstamp == max && vsz == 0u && val.empty();
            }

            void write(std::ofstream &ofs) const {
                std::uint32_t tstamp_ = bswap(tstamp), ksz_ = bswap(ksz), vsz_ = bswap(vsz);
                std::uint32_t crc_ = bswap(computeCRC(tstamp_, ksz_, vsz_, key, val));
                ofs.write(reinterpret_cast<const char*>(&crc_), 4);
                ofs.write(reinterpret_cast<const char*>(&tstamp_), 4);
                ofs.write(reinterpret_cast<const char*>(&ksz_), 4);
                ofs.write(reinterpret_cast<const char*>(&vsz_), 4);
                ofs.write(key.data(), static_cast<std::streamoff>(key.size()));
                ofs.write(val.data(), static_cast<std::streamoff>(val.size()));
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
                if (res.vsz) {
                    res.val = std::string(res.vsz, 0);
                    ifs.read(res.val.data(), res.vsz);
                    if (ifs.bad() || ifs.gcount() != res.vsz) 
                        return std::nullopt;
                }

                // Validate entry with CRC (must use ints as written on disk)
                return computeCRC(ints[1], ints[2], ints[3], res.key, res.val) == crc? 
                    std::optional{res}: std::nullopt;
            }

            [[nodiscard]] KeyDirValue kd(std::uint32_t id) const { 
                return {id, vsz, vstart, tstamp}; 
            }
        };

    private:
        [[nodiscard]] bool loadFromHintFile(const std::filesystem::path &path) {
            std::ifstream hintFile {path, std::ios::binary};

            char header[8] {}; hintFile.read(header, 7);
            if (std::strcmp(header, "BITCASK") != 0) return false;

            std::uint64_t entryCount;
            hintFile.read(reinterpret_cast<char*>(&entryCount), 8);
            keyDir.reserve(entryCount);

            while (keyDir.size() < entryCount && hintFile.good()) {
                char intRepr[20] {};
                hintFile.read(intRepr, 20);
                if (hintFile.bad() || hintFile.gcount() != 20)
                    return false;

                std::uint32_t tstamp, id, ksz, vsz, vpos;
                std::memcpy(reinterpret_cast<char*>(&tstamp), intRepr +  0, 4);
                std::memcpy(reinterpret_cast<char*>(    &id), intRepr +  4, 4);
                std::memcpy(reinterpret_cast<char*>(   &ksz), intRepr +  8, 4);
                std::memcpy(reinterpret_cast<char*>(   &vsz), intRepr + 12, 4);
                std::memcpy(reinterpret_cast<char*>(  &vpos), intRepr + 16, 4);
                inplace_bswap(tstamp, id, ksz, vsz, vpos);

                std::string key(ksz, 0);
                hintFile.read(key.data(), ksz);
                if (hintFile.bad() || hintFile.gcount() != ksz)
                    return false;

                KeyDirValue kdVal { .id=id, .vsize=vsz, .vpos=vpos, .tstamp=tstamp };
                keyDir.emplace(std::move(key), std::move(kdVal));

                // Keep track of latest id num seen so far
                currId = std::max(currId, id);
            }

            return keyDir.size() == entryCount;
        }

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
            auto sz = std::filesystem::file_size(path);
            currId = std::max(currId, sz < logSize? id: id + 1);

            return true;
        }

        // If log file size exceeded incr to next log file
        void ensureLogSizeLimits(std::string prefix = "cask") {
            if (currFile.tellp() >= static_cast<long>(logSize)) {
                auto currFilePath = datastorePath / (prefix + "." + std::to_string(++currId));
                currFile = std::ofstream {currFilePath, std::ios::binary | std::ios::app};
            }
        }

    public:
        void load(const std::filesystem::path &datastorePath) {
            this->datastorePath = datastorePath;

            if (!std::filesystem::exists(datastorePath))
                std::filesystem::create_directories(datastorePath);

            if (!std::filesystem::is_directory(datastorePath))
                throw std::runtime_error{"Input path is not a directory: " + 
                    datastorePath.string()};

            // Load hint file: if present, skip reading from cask files
            // End user responsibilty to make sure that hint file is upto date
            if (auto hintPath = datastorePath / ".hint"; std::filesystem::exists(hintPath)) {
                if (!std::filesystem::is_regular_file(hintPath) || !loadFromHintFile(hintPath))
                    std::println("Load from hintfile: {} was unsuccessful", hintPath.c_str());
                std::println("Loaded {} entries", keyDir.size());
            }

            // Read all cask.<id> files from the datastore path
            else {
                for (auto &entry: std::filesystem::directory_iterator {datastorePath}) {
                    auto filePath = entry.path();
                    if (entry.is_regular_file() && filePath.filename().string().starts_with("cask")) {
                        auto before {keyDir.size()};
                        if (!loadFromFile(filePath))
                            std::println("Load from {} was unsuccessful", filePath.c_str());
                        auto diff = static_cast<long>(keyDir.size()) - static_cast<long>(before);
                        std::println("Loaded {} new entries from File: {}", diff, filePath.c_str());
                    }
                }
            }

            // Load latest file into memory
            auto currFilePath = datastorePath / ("cask." + std::to_string(currId));
            currFile = std::ofstream {currFilePath, std::ios::binary | std::ios::app};
        }

        Bitcask(const std::filesystem::path &datastorePath) { load(datastorePath); }

        std::uint64_t size() const { return keyDir.size(); }

        decltype(auto) begin(this auto &&self) { return self.keyDir.begin(); }
        decltype(auto) end(this auto &&self) { return self.keyDir.end(); }

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

        // Rebuilds the logs assuming keydir as source of truth
        void merge() {
            // Make use of currId and currFile variables
            currFile.close(); currId = 0;
            auto currFilePath = datastorePath / ("merged-cask." + std::to_string(currId));
            currFile = std::ofstream {currFilePath, std::ios::binary | std::ios::app};

            // Store the entries here to sort and write the hints file
            std::vector<std::pair<std::uint32_t, std::string_view>> entries;
            entries.reserve(keyDir.size());

            for (auto &[key, kdVal]: keyDir) {
                // Read existing string entry
                std::ifstream ifs {datastorePath / ("cask." + std::to_string(kdVal.id)), std::ios::binary};
                ifs.seekg(kdVal.vpos);
                std::string val(kdVal.vsize, 0);
                ifs.read(val.data(), kdVal.vsize);

                // Create a new entry and write it
                FileEntry newEntry {key, val, currFile.tellp()};
                newEntry.write(currFile);
                kdVal = newEntry.kd(currId);
                ensureLogSizeLimits("merged-cask");

                // Write into entries vector
                entries.push_back({kdVal.tstamp, key});
            }

            // Write out the file hints, sorted based on last write timestamp
            std::ranges::sort(entries);
            std::ofstream hintFile {datastorePath / ".hint", std::ios::binary | std::ios::app};
            std::uint64_t entryCount {bswap(keyDir.size())};
            hintFile.write("BITCASK", 7);
            hintFile.write(reinterpret_cast<char*>(&entryCount), 8);
            for (auto &[_, key]: entries) {
                // Write the entry to hint file
                auto kdVal {keyDir[std::string{key}]};
                std::uint32_t tstamp_ {kdVal.tstamp}, id_ {kdVal.id}, 
                    ksz_ {static_cast<std::uint32_t>(key.size())}, 
                    vsz_ {kdVal.vsize}, vpos_ {kdVal.vpos};
                inplace_bswap(tstamp_, id_, ksz_, vsz_, vpos_);
                hintFile.write(reinterpret_cast<const char*>(&tstamp_), 4);
                hintFile.write(reinterpret_cast<const char*>(    &id_), 4);
                hintFile.write(reinterpret_cast<const char*>(   &ksz_), 4);
                hintFile.write(reinterpret_cast<const char*>(   &vsz_), 4);
                hintFile.write(reinterpret_cast<const char*>(  &vpos_), 4);
                hintFile.write(key.data(), static_cast<std::streamoff>(key.size()));
            }

            // Overwrite "merged-cask" entries into "cask"
            for (std::uint32_t id {}; id <= currId; ++id) {
                auto from = datastorePath / ("merged-cask." + std::to_string(id));
                auto to = datastorePath / ("cask." + std::to_string(id));
                std::filesystem::rename(from, to);
            }
        }
};

constexpr std::vector<std::string> split(const std::string &str) {
    auto isspace = [](char ch) { 
        std::string_view spaces {" \n\r\t\f"};
        return spaces.find(ch) != std::string::npos;
    };

    std::vector<std::string> strings;
    std::string buffer; 
    char insideQuote {0}, prevCh {0};
    for (char ch: str) {
        if (!insideQuote && prevCh != '\\' && (ch == '\'' || ch == '"')) {
            insideQuote = ch;
        } else if (prevCh != '\\' && insideQuote == ch) {
            insideQuote = 0;
        } else if (insideQuote || !isspace(ch)) {
            if (prevCh == '\\') buffer.pop_back();
            buffer += ch;
        } else if (!buffer.empty()) {
            strings.push_back(buffer);
            buffer.clear();
        }
        prevCh = ch;
    }

    if (!buffer.empty()) 
        strings.push_back(buffer);

    return strings;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::println("Usage: bitcask <datastore-path>");
        return 1;
    }

    std::filesystem::path datastorePath {argv[1]};
    Bitcask bc {datastorePath};

    std::string replHelp = "Commands:"
        "\n set:    Insert a key-value pair"
        "\n get:    Retreive a value using key"
        "\n del:    Delete a key-value pair"
        "\n merge:  Clean up log files"
        "\n size:   Count number of entries"
        "\n list:   List all keys"
        "\n reload: Reload data from disk"
        "\n clear:  Clear screen"
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

        else if (input == "merge") {
            bc.merge();
        }

        else if (input == "size") {
            std::println("{}", bc.size());
        }

        else if (input == "list") {
            for (const auto &[key, _]: bc)
                std::println("{}", key);
        }

        else if (input.starts_with("reload")) {
            auto inputs = split(input);
            if (inputs.size() == 1) bc.load(datastorePath);
            else if (inputs.size() == 2) bc.load(inputs[1]);
            else std::println("Expected syntax: reload [<directory>]");
        }

        else if (input == "clear") {
            std::print("\x1b[2J\x1b[H");
        }

        else if (input == "help") {
            std::println("{}", replHelp);
        }

        else {
            if (!input.empty()) 
                std::println("Unrecognized command: {}", input);
        }
    }
}
