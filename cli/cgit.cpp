#include <sys/stat.h>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <regex>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <ranges>

#include "argparse.hpp"
#include "../misc/iniparser.hpp"
#include "../misc/zhelper.hpp"
#include "../misc/fnmatch.hpp"
#include "../cryptography/hashlib.hpp"

/*
 * TODO:
 * 1. cgit log - when repo is empty
 * 2. cgit cat-file - add type parameter
 * 3. cgit add - add a folder directly
 * 4. Empty repo - differentiate added to index and not
 * 5. cgit commit with nothing added
 * 6. Support packfiles
 *   - findObject: name resolution failed
 *   - readObjectType: unable to locate object
 *   - readObject<>: unable to locate object
 */

namespace fs = std::filesystem;

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

// Explictly disallow second parameter as a string to prevent errors
void writeTextFile(const std::string&, const std::string&) = delete;

// Util to convert sha to binary hex string
[[nodiscard]] std::string sha2Binary(std::string_view sha) {
    std::string binSha;
    binSha.reserve(20);
    for (std::size_t i {0}; i < 40; i += 2) {
        std::string hexDigit {sha.substr(i, 2)};
        binSha.push_back(static_cast<char>(std::stoi(hexDigit, nullptr, 16)));
    }
    return binSha;
}

// Util to convert binary sha to hex string
[[nodiscard]] std::string binary2Sha(std::string_view binSha) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (const char &ch: binSha)
        oss << std::setw(2) << static_cast<int>(static_cast<unsigned char>(ch));
    return oss.str();
}

// Utils to trim a line
constexpr inline auto trim = 
    std::views::drop_while(::isspace) 
    | std::views::reverse 
    | std::views::drop_while(::isspace) 
    | std::views::reverse
    | std::ranges::to<std::string>();

// Utils to read input as Big Endian int*
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
template <typename T> requires std::integral<T>
void writeBigEndian(std::ofstream &ofs, T val) {
    unsigned char buffer[sizeof(T)];
    for (size_t i = 0; i < sizeof(T); ++i) {
        buffer[sizeof(T) - 1 - i] = val & 0xFF;
        val >>= 8;
    }
    ofs.write(reinterpret_cast<char *>(buffer), sizeof(T));
}

class GitObject {
    public:
        const std::string sha, fmt;
        GitObject(const std::string &sha, const std::string &fmt): sha(sha), fmt(fmt) {}
        virtual ~GitObject() = default;
        virtual void deserialize(const std::string&) = 0;
        virtual std::string serialize() const = 0;
};

class GitBlob: public GitObject {
    private:
        std::string data;

    public:
        GitBlob(const std::string &sha, const std::string &data): 
            GitObject(sha, "blob") { deserialize(data); }
        void deserialize(const std::string &data) override { this->data = data; }
        [[nodiscard]] std::string serialize() const override { return data; }
};

class GitCommit: public GitObject {
    private:
        stdx::ordered_map<std::string, std::vector<std::string>> data;
        std::chrono::system_clock::time_point commitUTC;

    public:
        // We will be reusing this for GitTag hence fmt is kept as a variable
        GitCommit(const std::string &sha, const std::string &raw, const std::string &fmt = "commit"): 
            GitObject(sha, fmt) { deserialize(raw); }

        void set(const std::string &key, std::vector<std::string> &&value) { data[key] = value; }
        std::vector<std::string> get(const std::string &key) const {
            return data.find(key) != data.end()? data.at(key): std::vector<std::string>{};
        }

        inline bool operator<(GitCommit &other) const { return this->commitUTC < other.commitUTC; }
        inline bool operator>(GitCommit &other) const { return this->commitUTC > other.commitUTC; }

        void deserialize(const std::string &raw) override {
            enum states: short {START, KEY_DONE, MULTILINE_VAL, BODY_START};
            short state {START}; std::string acc {""}, key{""};
            for (const char &ch: raw) {
                if (state == BODY_START || (ch != ' ' && ch != '\n') || (ch == ' ' && state != START)) {
                    acc += ch;
                } else if (ch == ' ' && state == START) {
                    if (!acc.empty()) {
                        state = KEY_DONE; key = acc; acc.clear();
                    } else if (!data.empty()) {
                        state = MULTILINE_VAL;
                    } else {
                        throw std::runtime_error("Failed to deserialize commit - Multiline value without existing key.");
                    }
                } else if (ch == '\n' && state == START) {
                    state = BODY_START; key = "";
                } else if (ch == '\n' && state == KEY_DONE) {
                    data[key].emplace_back(acc); 
                    acc.clear(); state = START;
                } else if (ch == '\n' && state == MULTILINE_VAL) {
                    data[key].back() += '\n' + acc;
                }
            }

            // We are adding all characters for the message body, need to remove '\n'
            if (!acc.empty() && acc.back() == '\n')
                acc.pop_back();

            // Add the body with empty string as header
            data[""] = {acc};

            // Set the commit time UTC if found
            if (data.find("committer") != data.end()) {
                std::string committerMsg {data["committer"][0]};
                std::size_t tzStartPos {committerMsg.rfind(' ')};
                std::size_t tsStartPos {committerMsg.rfind(' ', tzStartPos - 1)};
                std::string ts {committerMsg.substr(tsStartPos + 1, tzStartPos - tsStartPos)};
                commitUTC = std::chrono::system_clock::from_time_t(std::stol(ts));
            } else commitUTC = std::chrono::system_clock::now();
        }

        [[nodiscard]] std::string serialize() const override {
            std::ostringstream oss;
            for (const auto &[key, values]: data) {
                if (!key.empty()) {
                    // Print all as `Key Value`
                    for (const std::string &value: values) {
                        oss << key << ' ';
                        for (const char &ch: value)
                            oss << (ch != '\n'? std::string(1, ch): "\n ");
                        oss << '\n';
                    }
                }
            }

            oss << '\n' << data.at("")[0];
            return oss.str();
        }
};

class GitLeaf {
    private:
        static int pathCompare(const GitLeaf &l1, const GitLeaf &l2) {
            std::string_view path1 {l1.path}, path2 {l2.path};
            std::string modPath1, modPath2;
            if (!l1.mode.starts_with("10")) { 
                modPath1 = l1.path + '/';
                path1 = modPath1; 
            }
            if (!l2.mode.starts_with("10")) { 
                modPath2 = l2.path + '/'; 
                path2 = modPath2; 
            }
            return path1.compare(path2);
        }

    public:
        std::string mode, path, sha;

        GitLeaf(const std::string &mode, const std::string &path, const std::string &sha, bool shaInBinary = true):
            mode(mode), path(path), sha(shaInBinary? binary2Sha(sha): sha) {}

        // Copy constructor
        GitLeaf(const GitLeaf &other): mode(other.mode), path(other.path), sha(other.sha) {}

        // Move constructor
        GitLeaf(GitLeaf &&other): 
            mode(std::move(other.mode)), 
            path(std::move(other.path)), 
            sha(std::move(other.sha))
        { }

        // Move assignment
        inline GitLeaf& operator= (GitLeaf &&other) noexcept {
           if (this != &other) {
                mode = std::move(other.mode); 
                path = std::move(other.path); 
                sha = std::move(other.sha); 
           }
           return *this;
        }

        [[nodiscard]] inline std::string serialize() const {
            std::string result {mode + ' ' + path}; 
            result.push_back('\x00');
            result.append(sha2Binary(sha));
            return result;
        }

        // Comparators
        inline bool operator< (const GitLeaf &other) const { return pathCompare(*this, other)  < 0; }
        inline bool operator> (const GitLeaf &other) const { return pathCompare(*this, other)  > 0; }
        inline bool operator==(const GitLeaf &other) const { return pathCompare(*this, other) == 0; }
};

class GitTree: public GitObject {
    private:
        mutable std::vector<GitLeaf> data;

    public:
        GitTree(const std::string &sha, const std::string& raw): 
            GitObject(sha, "tree") { deserialize(raw); }

        GitTree(const std::vector<GitLeaf> &data): 
            GitObject("", "tree"), data(data) {}

        // Iterators
        inline std::vector<GitLeaf>::const_iterator begin() const { return data.cbegin(); }
        inline std::vector<GitLeaf>::const_iterator end() const { return data.cend(); }

        void deserialize(const std::string &raw) override {
            enum states: short {START, MODE_DONE, PATH_DONE};
            std::string acc, mode, path;
            short state {START};
            std::size_t i {0}, len {raw.size()};
            while (i < len) {
                const char &ch {raw[i]};
                if ((state == START && ch != ' ') || (state == MODE_DONE && ch != '\x00')) {
                    acc.push_back(ch);
                } 

                else if (state == START && ch == ' ') {
                    mode = acc; acc.clear(); state = MODE_DONE;
                    if (mode.size() == 5) mode = '0' + mode;
                } 

                else if (state == MODE_DONE && ch == '\x00') {
                    path = acc; acc.clear(); state = PATH_DONE;
                } 

                else if (state == PATH_DONE) {
                    if (i + 20 > len) 
                        throw std::runtime_error("Expected to have 20 bytes of char for SHA");
                    acc.assign(raw, i, 20); i += 19;
                    data.emplace_back(mode, path, acc);
                    mode.clear(); path.clear(); acc.clear();
                    state = START;
                }

                // Update for each loop
                i++;
            }
        }

        [[nodiscard]] std::string serialize() const override {
            std::string result;
            std::sort(data.begin(), data.end());
            for (const GitLeaf &leaf: data) {
                result.append(leaf.serialize());
            }
            return result;
        }
};

class GitTag: public GitCommit {
    public:
        using GitCommit::serialize, GitCommit::deserialize; 
        using GitCommit::get, GitCommit::set;
        GitTag(const std::string &sha, const std::string& raw): 
            GitCommit(sha, raw, "tag") {}
};

class GitIndex {
    public:
        struct GitTimeStamp { 
            unsigned int seconds, nanoseconds; 

            friend std::ostream &operator<<(std::ostream &os, const GitTimeStamp &ts) {
                std::time_t t = ts.seconds;
                std::tm *tm_info = std::gmtime(&t);
                os << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S") << '.' 
                   << std::setfill('0') << std::setw(9) << ts.nanoseconds;
                return os;
            }
        };

        struct GitIndexEntry {
            GitTimeStamp ctime;
            GitTimeStamp mtime;
            unsigned int dev;
            unsigned int inode;
            unsigned short modeType;
            unsigned short modePerms;
            unsigned int uid;
            unsigned int gid;
            unsigned int fsize;
            std::string sha;
            unsigned short flagStage;
            bool assumeValid;
            std::string name;
        };

    private:
        const unsigned int version;
        std::vector<GitIndexEntry> entries;

    public:
        // Getters for downstream consumers
        unsigned int getVersion() const { return version; }
        const std::vector<GitIndexEntry> &getEntries() const { return entries; }
        std::vector<GitIndexEntry> &getEntries() { return entries; }

        GitIndex(const unsigned int version = 2, const std::vector<GitIndexEntry> &entries = {}):
            version(version), entries(entries) { }

        static GitIndex readFromFile(const fs::path &path) {
            if (!fs::exists(path)) return GitIndex{};

            std::ifstream ifs{path, std::ios::binary};
            char signature[4]; unsigned int version, count;

            ifs.read(signature, 4);
            if (std::strcmp(signature, "DIRC") != 0) throw std::runtime_error("Not a valid GitIndex file.");
            
            readBigEndian(ifs, version); readBigEndian(ifs, count);
            if (version != 2) throw std::runtime_error("CGit only supports Index file version 2.");

            std::vector<GitIndexEntry> entries;
            for (unsigned int i {0}; i < count; i++) {
                // Read timestamps as seconds, nano pairs
                unsigned int ctimes, ctimens, mtimes, mtimens;
                readBigEndian(ifs, ctimes); readBigEndian(ifs, ctimens);
                readBigEndian(ifs, mtimes); readBigEndian(ifs, mtimens);
                GitTimeStamp ctime {.seconds=ctimes, .nanoseconds=ctimens};
                GitTimeStamp mtime {.seconds=mtimes, .nanoseconds=mtimens};

                // Straightforward read
                unsigned int dev, inode;
                readBigEndian(ifs, dev); readBigEndian(ifs, inode);

                // Skip 2 bytes
                ifs.seekg(2, std::ios::cur);

                // Read modeType, modePerms
                unsigned short mode, modeType, modePerms; readBigEndian(ifs, mode);
                modeType = mode >> 12; modePerms = mode & 0b0000000111111111;

                // Read user ID, Gid, File size
                unsigned int uid, gid, fsize;
                readBigEndian(ifs, uid); readBigEndian(ifs, gid); readBigEndian(ifs, fsize);

                // Read the 20 char long binary sha and convert to hex 
                // string 40 char long with padding
                char shaBin[20];  ifs.read(shaBin, 20);
                std::string sha {binary2Sha(std::string_view{shaBin, 20})};

                // Read the flags
                unsigned short flags, flagStage, nameLength; bool assumeValid;
                readBigEndian(ifs, flags);
                assumeValid = (flags & 0b1000000000000000) != 0;
                flagStage = flags & 0b0011000000000000;
                nameLength = flags & 0b0000111111111111;

                // Read the name, if name size is 0xFF git assumes name 
                // is > 4095 char long & searches until we hit 0x00
                char rawName[0xFF + 1] {}; ifs.read(rawName, nameLength + 1);
                std::string name {rawName, nameLength};
                if (nameLength == 0xFF) {
                    unsigned char ch;
                    while ((ch = static_cast<unsigned char>(ifs.get())) != 0x00)
                        name.push_back(static_cast<char>(ch));
                }

                // Seek bytes until we are in multiples of 8 
                // 62 bytes read until we started parsing the name
                std::streamoff readBytes {static_cast<std::streamoff>(ifs.tellg()) - 12};
                std::streamoff offset {(8 - (readBytes % 8)) % 8};
                ifs.seekg(offset, std::ios::cur);

                // Create the index entry
                entries.emplace_back(GitIndexEntry{
                    .ctime=ctime, 
                    .mtime=mtime, 
                    .dev=dev, 
                    .inode=inode, 
                    .modeType=modeType, 
                    .modePerms=modePerms, 
                    .uid=uid, 
                    .gid=gid, 
                    .fsize=fsize, 
                    .sha=sha, 
                    .flagStage=flagStage,
                    .assumeValid=assumeValid,
                    .name=name
                });
            }

            return GitIndex{version, entries};
        }

        void writeToFile(const fs::path &path) const {
            std::ofstream ofs{path, std::ios::binary};
            if (!ofs) throw std::runtime_error("Unable to write GitIndex to file: " + path.string());

            // Write the header
            unsigned int count {static_cast<unsigned int>(entries.size())};
            ofs.write("DIRC", 4); writeBigEndian(ofs, version); writeBigEndian(ofs, count);

            // Write the entries
            for (const GitIndexEntry &entry: entries) {
                // Create + Modified timestamp
                writeBigEndian(ofs, entry.ctime.seconds); writeBigEndian(ofs, entry.ctime.nanoseconds);
                writeBigEndian(ofs, entry.mtime.seconds); writeBigEndian(ofs, entry.mtime.nanoseconds);

                // Dev, inode
                writeBigEndian(ofs, entry.dev); writeBigEndian(ofs, entry.inode);
                
                // Write mode as unsigned int (4 bytes) + write uid, gid, fsize
                unsigned int mode {(static_cast<unsigned int>(entry.modeType) << 12) | entry.modePerms};
                writeBigEndian(ofs, mode); writeBigEndian(ofs, entry.uid); writeBigEndian(ofs, entry.gid);
                writeBigEndian(ofs, entry.fsize);

                // Write the 40 char long hex to binary
                std::string shaBin {sha2Binary(entry.sha)};
                ofs.write(shaBin.c_str(), 20);

                // Write name length, flags together
                unsigned short flagAssumeValid {static_cast<unsigned short>(entry.assumeValid? 0x1 << 15: 0)};
                unsigned short nameLength {static_cast<unsigned short>(entry.name.size() >= 0xFFF? 0xFFF: entry.name.size())};
                unsigned short flag {static_cast<unsigned short>(flagAssumeValid | entry.flagStage | nameLength)};
                writeBigEndian(ofs, flag);

                // Write the name along with 0x00
                ofs.write(entry.name.data(), static_cast<std::streamsize>(entry.name.size())); 
                ofs.put(0x00);

                // Add neccessary padding
                std::streamoff writtenBytes {static_cast<std::streamsize>(ofs.tellp()) - 12};
                std::streamoff padCount {(8 - (writtenBytes % 8)) % 8};
                for (int i {0}; i < padCount; i++) ofs.put(0x00);
            }
        }
};

class GitIgnore {
    public:
        using BS_PAIR = std::pair<bool, std::string>;

        GitIgnore(
            const std::vector<std::pair<bool, std::string>> &absolute = {}, 
            const std::unordered_map<std::string, std::vector<std::pair<bool, std::string>>> &scoped = {}
        ): absolute(absolute), scoped(scoped) {}

        bool check(const std::string &path) const {
            fs::path curr {path};
            if (!curr.is_relative())
                throw std::runtime_error("Input paths provided must be relative to the repo's root.");

            // Check scoped rules
            std::optional<bool> result;
            while (!curr.empty() && curr != curr.root_path()) {
                fs::path parent {curr.parent_path()};
                auto it {scoped.find(parent.string())};
                if (it != scoped.end() && (result = checkIgnore(it->second, curr.string())) && result.has_value())
                    return result.value();
                curr = parent;
            }

            // Check absolute rules, if no match return false as default
            return checkIgnore(absolute, path).value_or(false);
        }

    private:
        std::vector<std::pair<bool, std::string>> absolute;
        std::unordered_map<std::string, std::vector<std::pair<bool, std::string>>> scoped;

        static std::optional<bool> checkIgnore(const std::vector<BS_PAIR> &rules, const std::string &path) {
            // Check all rules, last rule takes precedence
            std::optional<bool> result;
            for (const BS_PAIR &rule: rules)
                if (fnmatch::match(rule.second, path))
                    result = rule.first;
            return result;
        }
};

class GitPack {
    private:
        // Store all the pack indices & files from path
       std::vector<fs::path> indexPaths, packPaths;

        static bool verifyHeader(std::ifstream &ifs, const std::string &expectedHeader, unsigned int expectedVersion) {
            // Verify magic byte header
            char header[4]; 
            ifs.read(header, 4); 
            if (std::strcmp(header, expectedHeader.c_str()) != 0) 
                return false;

            // Verify version number
            unsigned int version;
            readBigEndian(ifs, version);
            return version == expectedVersion;
        }

        // Binary search to find *first* offset from the .idx file where `part` starts at
        unsigned int getPackIdxOffsetStart(unsigned int start, unsigned int end, 
                const std::string &part, std::ifstream &ifs) const 
        {
            constexpr unsigned int skip {8 + 256 * 4}; 
            while (start <= end) {
                unsigned int mid = start + (end - start) / 2;
                ifs.seekg(skip + (mid * 20), std::ios::beg);

                char shaBin[20]; ifs.read(shaBin, 20);
                std::string sha {binary2Sha(std::string_view{shaBin, 20})};

                // We check start == 0 to prevent underflow (uint)
                if (sha.compare(0, part.size(), part) >= 0) {
                    if (start == mid) break;
                    end = mid - 1;
                } else {
                    start = mid + 1;
                }
            }

            return start;
        }

        // Returns a vector of resolved hashes along with the index
        std::vector<std::pair<std::string, unsigned int>> getHashMatchFromIndex(const std::string &part, const fs::path &path) const {
            if (part.size() < 2)
                throw std::runtime_error("Hex passed into PackIndex must be atleast 2 chars long.");

            std::ifstream ifs {path, std::ios::binary};
            if (!verifyHeader(ifs, "\xfftOc", 2))
                throw std::runtime_error("Not a valid pack idx file: " + path.string());

            // Check fanout table layer 1 - 256 entries, 4 bytes each
            int hexInt {std::stoi(part.substr(0, 2), nullptr, 16)};
            unsigned int curr, prev{0};
            ifs.seekg(8 + (hexInt * 4), std::ios::beg);
            readBigEndian(ifs, curr);
            if (hexInt > 0) {
                ifs.seekg(8 + ((hexInt - 1) * 4), std::ios::beg);
                readBigEndian(ifs, prev);
            }
            
            // If hashes exist in the pack
            std::vector<std::pair<std::string, unsigned int>> matches;
            if (curr - prev > 0) {
                unsigned int skip {8 + 256 * 4}, startOffset {getPackIdxOffsetStart(prev, curr, part, ifs)};
                ifs.seekg(skip + (startOffset * 20), std::ios::beg);
                while (startOffset <= curr) {
                    char shaBin[20];  ifs.read(shaBin, 20);
                    std::string sha {binary2Sha(std::string_view{shaBin, 20})};
                    if (!sha.starts_with(part)) break;
                    matches.emplace_back(sha, startOffset);
                    startOffset++;
                }
            }

            return matches;
        }

        std::pair<fs::path, unsigned long> getPackFileOffset(const std::string &objectHash) const {
            std::vector<std::pair<fs::path, unsigned int>> matches;
            for (const fs::path &path: indexPaths) {
                for (const std::pair<std::string, unsigned int> &match: getHashMatchFromIndex(objectHash, path)) {
                    matches.emplace_back(path, match.second);
                }
            }

            if (matches.size() != 1)
                throw std::runtime_error(objectHash + ": Expected candidates to be 1, got: " + std::to_string(matches.size()));

            // Get the Pack offset along with count of records
            std::ifstream ifs {matches[0].first, std::ios::binary};
            unsigned int offset {matches[0].second}, total;
            ifs.seekg(8 + (255 * 4), std::ios::beg);
            readBigEndian(ifs, total);

            // Read from first offset layer
            ifs.seekg(1032 + (total * 24) + (offset * 4), std::ios::beg);
            unsigned long result;
            unsigned int r1, mask {1u << 31}; 
            readBigEndian(ifs, r1);

            // First layer contains direct entries
            if (!(r1 & mask))
                result = static_cast<unsigned long>(r1);

            // First layer points to second layer
            else {
                r1 &= ~mask; ifs.seekg(1032 + (total * 28) + (r1 * 8), std::ios::beg);
                readBigEndian(ifs, result);
            } 

            return {matches[0].first.replace_extension(".pack"), result};
        }

    public:
        GitPack (const fs::path &path) {
            if (fs::exists(path)) {
                for (const fs::directory_entry &entry: fs::directory_iterator(path)) {
                    if (entry.path().extension() == ".idx")
                        indexPaths.emplace_back(entry);
                    else if (entry.path().extension() == ".pack")
                        packPaths.emplace_back(entry);
                }
            }
        }

        // Check for matches & return the full sha match if found
        [[nodiscard]] std::vector<std::string> refResolve(const std::string &part) const {
            std::vector<std::string> matches;
            for (const fs::path &path: indexPaths) {
                for (const std::pair<std::string, unsigned int> &match: getHashMatchFromIndex(part, path)) {
                    matches.emplace_back(match.first);
                }
            }

            return matches;
        }

        [[nodiscard]] std::string extract(const std::string &objectHash) const {
            fs::path packFile; unsigned long offset;
            std::tie(packFile, offset) = getPackFileOffset(objectHash);
            std::ifstream ifs {packFile, std::ios::binary};
            if (!verifyHeader(ifs, "PACK", 2))
                throw std::runtime_error("Not a valid pack file: " + packFile.string());

            // type and length
            bool msb {true}; short type{0}; 
            std::size_t length; int shift;
            ifs.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            while (msb) {
                int byte = ifs.get();
                msb = byte & 128;
                if (!type) {
                    type = (byte >> 4) & 7;
                    length = byte & 15;
                    shift = 4;
                } else {
                    length |= static_cast<std::size_t>((byte & 127) << shift);
                    shift += 7;
                }
            }

            // Reformat to the way the other functions expect
            std::string fmt;
            switch (type) {
                case 1: fmt = "commit"; break;
                case 2: fmt = "tree"; break;
                case 3: fmt = "blob"; break;
                case 4: fmt = "tag"; break;
                default: throw std::runtime_error("Unsupported format: " + std::to_string(type));
            }

            // ZDecompress and compare the inflated 
            // size against what we parsed
            std::string decompressed {zhelper::zdecompress(ifs)};
            if (decompressed.size() != length)
                throw std::runtime_error("Incorrect obj size, expected: " + 
                        std::to_string(length) + ", got: " + 
                        std::to_string(decompressed.size()));

            // "<FMT> <SIZE>\x00<DATA...>
            return fmt + ' ' + std::to_string(length) + '\x00' + decompressed;
        }
};

class GitRepository {
    private:
        mutable fs::path workTree, gitDir;
        std::unordered_map<std::string, std::string> packedRefs;
        INI::Parser conf;  

        fs::path repoPath(const std::initializer_list<std::string_view> &parts) const {
            fs::path result {gitDir};
            for (const std::string_view &part: parts)
                result /= part;
            return result;
        }

        std::string writeIndexAsTree() {
            // Extract git index
            GitIndex index{GitIndex::readFromFile(repoFile({"index"}))};
            std::vector<GitIndex::GitIndexEntry> &entries{index.getEntries()};

            // Create a directory tree like structure
            std::unordered_map<fs::path, std::vector<fs::path>> directoryTree;
            std::unordered_map<std::string, const GitIndex::GitIndexEntry*> lookup;
            for (const GitIndex::GitIndexEntry &entry: entries) {
                lookup.emplace(entry.name, &entry);
                fs::path curr {fs::path(entry.name)};
                while (!curr.empty() && curr != curr.parent_path()) {
                    fs::path parent {curr.parent_path()};
                    directoryTree[parent].emplace_back(curr);
                    curr = parent;
                }
            }

            // Recursive func to write tree nodes starting at root for gitdir
            std::function<std::tuple<std::string, fs::path, std::string>(const fs::path&)> backtrack {[&](const fs::path &curr) {
                // A simple file
                if (directoryTree.find(curr) == directoryTree.end()) {
                    const GitIndex::GitIndexEntry* entry {lookup.at(curr)};
                    std::string modeStr {std::format("{:02o}{:04o}", entry->modeType, entry->modePerms)};
                    return std::make_tuple(modeStr, curr.filename(), entry->sha);
                } 

                // A folder
                else {
                    std::vector<GitLeaf> leaves;
                    for (const fs::path &child: directoryTree[curr]) {
                        auto [childMode, childPath, childSha] = backtrack(child);
                        leaves.emplace_back(GitLeaf{childMode, childPath, childSha, false});
                    }
                    std::string sha {writeObject(std::make_unique<GitTree>(leaves), true)};
                    return std::make_tuple(std::string{"040000"}, curr.filename(), sha);
                }
            }};

            auto [_, __, sha] = backtrack("");
            return sha;
        }

        std::string getPackedRef(const std::string &key) const {
            auto it {packedRefs.find(key)};
            return it == packedRefs.end()? "": it->second;
        }

    public:
        GitRepository(const fs::path &path, bool force = false):
            workTree(path), gitDir(path / ".git")
        {
            if (!force) {
                if (!fs::is_directory(gitDir))
                    throw std::runtime_error("Not a Git Repository: " + fs::canonical(gitDir).string());
                if (!fs::is_regular_file(gitDir / "config"))
                    throw std::runtime_error("Configuration file missing");

                // Read the config file
                conf.reads(readTextFile(gitDir / "config"));
                std::string repoVersion {"** MISSING **"};
                if (!conf.exists("core", "repositoryformatversion") || (repoVersion = conf["core"]["repositoryformatversion"]) != "0")
                    throw std::runtime_error("Unsupported repositoryformaversion: " + repoVersion);

                // Parse packed-refs
                fs::path parsedRefsFile {repoFile({"packed-refs"})};
                if (fs::exists(parsedRefsFile)) {
                    std::ifstream ifs {parsedRefsFile};
                    std::string line;
                    while (std::getline(ifs, line)) {
                        line = line | trim;
                        if (!line.empty() && line.at(0) != '#') {
                            std::vector<std::string> splits = 
                                line 
                                | std::views::split(' ')
                                | std::views::transform([&](auto &&split) { return split | trim; })
                                | std::ranges::to<std::vector<std::string>>(); 

                            if (splits.size() != 2)
                                throw std::runtime_error("Invalid packed-refs format");

                            // Add '<SHA> <ref> into the map'
                            packedRefs[splits[1]] = splits[0];
                        }
                    }
                }
            }
            
            else {
                if (!fs::exists(workTree)) 
                    fs::create_directories(workTree);
                else if (!fs::is_directory(workTree))
                    throw std::runtime_error(workTree.string() + " is not a directory");
                else if (fs::exists(gitDir) && !fs::is_empty(gitDir))
                    throw std::runtime_error(fs::canonical(gitDir).string() + " is not empty");

                // Create the folders required
                fs::create_directories(gitDir / "branches");
                fs::create_directories(gitDir / "objects");
                fs::create_directories(gitDir / "refs" / "tags");
                fs::create_directories(gitDir / "refs" / "heads");

                // .git/description
                writeTextFile("Unnamed repository; edit this file 'description' to name the repository.\n", gitDir / "description");

                // .git/HEAD
                writeTextFile("ref: refs/heads/main\n", gitDir / "HEAD");

                // .git/config - default config
                conf["core"]["repositoryformatversion"] = "0";
                conf["core"]["filemode"] = "false";
                conf["core"]["bare"] = "false";
                writeTextFile(conf.dumps(), gitDir / "config");
            }

            // Guaranteed that the paths exist, lets clean em up
            gitDir = fs::canonical(gitDir);
            workTree = fs::canonical(workTree); 
        }

        fs::path repoDir() const { return gitDir; }

        fs::path repoDir(const std::initializer_list<std::string_view> &parts, bool create = false) const {
            fs::path fpath {repoPath(parts)};
            if (create) fs::create_directories(fpath);
            return fpath;
        }

        fs::path repoFile(const std::initializer_list<std::string_view> &parts, bool create = false) const {
            fs::path fpath {repoPath(parts)};
            if (create) fs::create_directories(fpath.parent_path());
            return fpath;
        }

        static GitRepository findRepo(const fs::path &path_ = ".") {
            fs::path path {fs::absolute(path_)};
            if (fs::exists(path / ".git"))
                return GitRepository(path);
            else if (!path.has_parent_path() || path == path.parent_path())
                throw std::runtime_error("No git directory");
            else
                return findRepo(path.parent_path());
        }

        std::string writeObject(const std::unique_ptr<GitObject> &obj, bool write = false) const {
            // Serialize the object data
            std::string serialized {obj->serialize()};

            // Add header and compute hash
            serialized = obj->fmt + ' ' + std::to_string(serialized.size()) + '\x00' + serialized;
            std::string objectHash {hashutil::sha1(serialized)};

            // Write the object to disk
            if (write) {
                fs::path path {repoFile({"objects", objectHash.substr(0, 2), objectHash.substr(2)}, true)};
                zhelper::zwrite(serialized, path);
            }

            return objectHash;
        }

        std::string findObject(const std::string &name, const std::string &fmt = "", bool follow = true) const {
            std::vector<std::string> candidates;
            if (name == "HEAD") candidates.emplace_back(refResolve("HEAD"));
            else {
                // Check if name matches hash format - small or full hash
                std::regex hashRegex{R"(^[0-9A-Fa-f]{4,40}$)"}; 
                if (std::regex_match(name, hashRegex)) {
                    std::string part;
                    for (const char &ch: name) part.push_back(static_cast<char>(std::tolower(ch)));
                    std::string prefix {part.substr(0, 2)};
                    fs::path path {repoFile({"objects", prefix})};
                    std::string remaining {part.substr(2)};
                    if (fs::exists(path)) {
                        for (const fs::directory_entry &entry: fs::directory_iterator(path)) {
                            const std::string fname {entry.path().filename()};
                            if (fname.starts_with(remaining))
                                candidates.emplace_back(prefix + fname);
                        }
                    }

                    // Insert from pack indices if match found
                    std::vector<std::string> asPack {GitPack(repoDir({"objects", "pack"})).refResolve(part)};
                    candidates.insert(candidates.end(), asPack.begin(), asPack.end());
                }

               // Check for tag match
                std::string asTag {refResolve("refs/tags/" + name)};
                if (!asTag.empty()) candidates.emplace_back(asTag);

                // Check for branch match
                std::string asBranch {refResolve("refs/heads/" + name)};
                if (!asBranch.empty()) candidates.emplace_back(asBranch);
            }

            if (candidates.size() != 1)
                throw std::runtime_error(
                    "Name resolution failed: " + name + ".\nExpected to have only 1 matching"
                    " candidate, found " + std::to_string(candidates.size()));

            std::string sha {candidates[0]};
            if (fmt.empty()) return sha;

            while (1) {
                std::string objFmt {readObjectType(sha)};
                if (objFmt == fmt) 
                    return sha;
                else if (!follow) 
                    return "";
                else if (objFmt == "tag")
                    sha = readObject<GitTag>(sha)->get("object")[0];
                else if (objFmt == "commit" || fmt == "tree")
                    sha = readObject<GitCommit>(sha)->get("tree")[0];
                else 
                    return "";
            }

            return "";
        }

        std::string readObjectType(const std::string &objectHash) const {
            fs::path path {repoFile({"objects", objectHash.substr(0, 2), objectHash.substr(2)})};
            GitPack pack{repoDir({"objects", "pack"})};

            // If neither loose object nor packed, return error
            bool isLooseObj {fs::exists(path)};
            bool isPackedObj {!isLooseObj && !pack.refResolve(objectHash).empty()};
            if (!isLooseObj && !isPackedObj)
                throw std::runtime_error("Unable to locate object: " + objectHash);

            // Parse as a loose obj or a packed object and return the type
            std::string raw;
            if (isLooseObj) raw = zhelper::zread(path);
            else raw = pack.extract(objectHash);
            return raw.substr(0, raw.find(' '));
        }

        std::unique_ptr<GitObject> readObject(const std::string &objectHash) const {
            fs::path path {repoFile({"objects", objectHash.substr(0, 2), objectHash.substr(2)})};
            GitPack pack{repoDir({"objects", "pack"})};

            // If neither loose object nor packed, return error
            bool isLooseObj {fs::exists(path)};
            bool isPackedObj {!isLooseObj && !pack.refResolve(objectHash).empty()};
            if (!isLooseObj && !isPackedObj)
                throw std::runtime_error("Unable to locate object: " + objectHash);

            // Parse as a loose obj or a packed object and return the type
            std::string raw;
            if (isLooseObj) raw = zhelper::zread(path);
            else raw = pack.extract(objectHash);

            // Format: "<FMT> <SIZE>\x00<DATA...>"
            std::size_t fmtEndPos {raw.find(' ')};
            std::string fmt {raw.substr(0, fmtEndPos)};
            std::size_t sizeEndPos {raw.find('\x00', fmtEndPos)};
            std::size_t size {std::stoull(raw.substr(fmtEndPos + 1, sizeEndPos - fmtEndPos))};

            if (size != raw.size() - sizeEndPos - 1)
                throw std::runtime_error("Malformed object " + objectHash + ": bad length");

            std::string data {raw.substr(sizeEndPos + 1)};
            if (fmt == "tag") 
                return    std::make_unique<GitTag>(objectHash, data);
            else if (fmt == "tree") 
                return   std::make_unique<GitTree>(objectHash, data);
            else if (fmt == "blob") 
                return   std::make_unique<GitBlob>(objectHash, data);
            else if (fmt == "commit") 
                return std::make_unique<GitCommit>(objectHash, data);
            else
                throw std::runtime_error("Unknown type " + fmt + " for object " + objectHash);
        }

        template <typename T>
        std::unique_ptr<T> readObject(const std::string &objectHashPart) const {
            std::unique_ptr<GitObject> obj {readObject(objectHashPart)};
            T* casted {dynamic_cast<T*>(obj.get())};
            if (!casted) throw std::runtime_error("Invalid cast: GitObject is not of requested type.");
            return std::unique_ptr<T>(static_cast<T*>(obj.release()));
        }

        std::string getLog(const std::string &commit, long maxCount) const {
            // Stop early
            if (maxCount == 0) return "";

            // Store the commit objects
            std::vector<std::unique_ptr<GitCommit>> logs;

            // DFS to read all the commits (until maxCount)
            std::string objectHash {findObject(commit, "commit")};
            std::stack<std::pair<std::string, int>> stk {{{objectHash, 1}}};
            std::unordered_set<std::string> visited {objectHash};
            while (!stk.empty()) {
                std::pair<std::string, int> curr {std::move(stk.top())}; stk.pop();
                logs.emplace_back(readObject<GitCommit>(curr.first));
                for (const std::string &parent: logs.back()->get("parent")) {
                    if ((maxCount == -1 || curr.second < maxCount) && visited.find(parent) == visited.end()) {
                        visited.insert(parent); 
                        stk.push({parent, curr.second + 1});
                    }
                }
            }

            // Sort based on committer date in desc order
            std::sort(logs.begin(), logs.end(), [](const auto &c1, const auto &c2) { return *c1 > *c2; });

            // Print out the logs
            std::ostringstream oss; std::size_t mc {std::min(logs.size(), static_cast<std::size_t>(maxCount))};
            for (std::size_t i {0}; i < mc; i++)  {
                const std::unique_ptr<GitCommit> &commit {logs[i]};
                oss << "commit " << commit->sha << '\n'
                    << commit->serialize() << "\n\n";
            }

            // Remove extra space
            std::string result {oss.str()};
            while (!result.empty() && result.back() == '\n') 
                result.pop_back();

            return result;
        }

        std::string lsTree(const std::string &ref, bool recurse, const fs::path &prefix = "") const {
            std::string sha {findObject(ref, "tree")};
            std::unique_ptr<GitTree> tree {readObject<GitTree>(sha)};
            std::ostringstream oss;
            for (const GitLeaf &leaf: *tree) {
                std::string type;
                if (leaf.mode.starts_with("04"))
                    type = "tree"; // directory
                else if (leaf.mode.starts_with("10"))
                    type = "blob"; // regular file
                else if (leaf.mode.starts_with("12"))
                    type = "blob"; // symlinked contents
                else if (leaf.mode.starts_with("16"))
                    type = "commit"; // Submodule
                else
                    throw std::runtime_error("Unkwown tree mode: " + leaf.mode);

                fs::path leafPath {prefix / leaf.path};
                if (!recurse || type != "tree")
                    oss << leaf.mode << ' ' << type << ' ' << leaf.sha << '\t' << leafPath.string() << '\n';
                else
                    oss << lsTree(leaf.sha, recurse, leafPath);
            }

            // Remove extra space
            std::string result {oss.str()};
            while (!result.empty() && result.back() == '\n') 
                result.pop_back();

            return result;
        }

        void checkout(const std::string &ref, const fs::path &checkoutPath) const {
            // Ensure empty before proceeding
            if (!fs::exists(checkoutPath)) fs::create_directories(checkoutPath);
            else if (!fs::is_directory(checkoutPath)) throw std::runtime_error("Not a directory: " + checkoutPath.string());
            else if (!fs::is_empty(checkoutPath)) throw std::runtime_error("checkoutPath is not empty: " + checkoutPath.string());

            // Get the absolute path
            fs::path basePath {fs::canonical(fs::absolute(checkoutPath))};

            // Read the ref, if commit grab its tree
            std::unique_ptr<GitObject> obj {readObject(findObject(ref))};
            if (obj->fmt == "commit") {
                std::unique_ptr<GitCommit> commit {static_cast<GitCommit*>(obj.release())};
                obj = readObject(commit->get("tree")[0]);
            }

            // Recursively write the tree contents
            std::stack<std::pair<std::unique_ptr<GitTree>, fs::path>> stk {};
            stk.emplace(static_cast<GitTree*>(obj.release()), basePath);
            while (!stk.empty()) {
                std::unique_ptr<GitTree> tree {std::move(stk.top().first)};
                fs::path path {std::move(stk.top().second)}; stk.pop();
                for (const GitLeaf &leaf: *tree) {
                    std::unique_ptr<GitObject> obj {readObject(leaf.sha)};
                    fs::path dest {path / leaf.path};
                    if (obj->fmt == "tree") {
                        fs::create_directories(dest);
                        stk.emplace(static_cast<GitTree*>(obj.release()), dest);
                    } else if (obj->fmt == "blob") {
                        std::unique_ptr<GitBlob> blob {static_cast<GitBlob*>(obj.release())};
                        std::string blobData {blob->serialize()};
                        std::ofstream ofs {dest, std::ios::binary};
                        ofs.write(blobData.c_str(), static_cast<std::streamsize>(blobData.size()));
                    }
                }
            }
        }

        // Recursively resolve ref until we have a sha hash
        [[nodiscard]] std::string refResolve(const std::string &path) const {
            std::string currRef {"ref: " + path};
            while (currRef.starts_with("ref: ")) {
                fs::path path {repoFile({currRef.substr(5)})};
                if (!fs::is_regular_file(path))
                    return getPackedRef(fs::relative(path, gitDir).string());
                currRef = readTextFile(path);
                currRef.pop_back();
            }

            return currRef;
        }

        // Start - starting path; withHash - whether to display the sha post resolving; prefix - sometimes
        // we require that we cut the prefix short or have a custom prefix diplayed, hence sep variable is used
        [[nodiscard]] std::string showAllRefs(const std::string &start, bool withHash, const std::string &prefix) const {
            // Gather all refs starting from prefix
            fs::path startPath {repoPath({start})};
            std::vector<std::string> paths;
            for (const fs::directory_entry &entry: fs::recursive_directory_iterator(startPath))
                if (entry.is_regular_file()) paths.emplace_back(prefix / fs::relative(entry, startPath));

            // Sort alphabetically
            std::sort(paths.begin(), paths.end());

            std::ostringstream oss;
            for (const std::string &path: paths) {
                if (withHash)
                    oss << refResolve(path) << ' ';
                oss << path << '\n';
            }

            std::string result {oss.str()};
            if (!result.empty()) result.pop_back();
            return result;
        }

        void createTag(const std::string &name, const std::string &ref, bool createTagObj = false) const {
            std::string sha {findObject(ref)};
            if (createTagObj) {
                std::ostringstream oss;
                oss << "object " << sha << '\n'
                    << "type commit\n"
                    << "tag " << name << '\n'
                    << "tagger CGit user@example.com\n\n"
                    << "A tag created by CGit.\n";
                sha = writeObject(std::make_unique<GitTag>("", oss.str()), true);
            }

            // Write the contents to the file
            sha.push_back('\n');
            writeTextFile(sha, repoFile({"refs", "tags", name}));
        }

        std::string lsFiles(bool verbose = false) const {
            std::ostringstream oss;
            fs::path indexFilePath {repoFile({"index"})};
            GitIndex index{GitIndex::readFromFile(indexFilePath)};
            const std::vector<GitIndex::GitIndexEntry> &indexEntries {index.getEntries()};

            if (verbose)
                oss << "Index file format v" << index.getVersion() 
                    << ", containing " << indexEntries.size() << " entires.\n";

            for (const GitIndex::GitIndexEntry &entry: indexEntries) {
                oss << entry.name << '\n';
                if (verbose) {
                    std::string entryType;
                    switch (entry.modeType) {
                        case 0b1000: entryType = "regular file"; break;
                        case 0b1010: entryType = "symlink"; break;
                        case 0b1110: entryType = "gitlink"; break;
                    }

                    // Write to string output stream
                    oss << "  " << entryType << " with perms: " << entry.modePerms << '\n';
                    oss << "  on blob: " << entry.sha << '\n';
                    oss << "  created: " << entry.ctime << ", modified: " << entry.mtime << '\n';
                    oss << "  device: " << entry.dev << ", inode: " << entry.inode << '\n';
                    oss << "  user: (" << entry.uid << ") group: (" << entry.gid << ")\n";
                    oss << "  flags: stage=" << entry.flagStage << " assume valid=" << entry.assumeValid << "\n\n";
                }
            }

            std::string result {oss.str()};
            while (!result.empty() && result.back() == '\n')
                result.pop_back();
            return result;
        }

        GitIgnore gitIgnore() const {
            std::vector<GitIgnore::BS_PAIR> absolute;
            std::unordered_map<std::string, std::vector<GitIgnore::BS_PAIR>> scoped;

            // Helper to categories a string into a pair<bool, string>
            const std::function<GitIgnore::BS_PAIR(std::string_view)> parseLine {[](std::string_view line) -> GitIgnore::BS_PAIR {
                line = line | trim;
                if (line.empty() || line.at(0) == '#') return GitIgnore::BS_PAIR{false, ""};
                else {
                    char first {line.at(0)};
                    bool includePattern {first != '!'};
                    return GitIgnore::BS_PAIR{includePattern, first == '!' || first == '\\'? line.substr(1): line};
                }
            }};

            // Read local configuration in .git/info/exclude
            fs::path ignoreFile {repoFile({"info", "exclude"})};
            if (fs::exists(ignoreFile)) {
                std::ifstream ifs {ignoreFile};
                std::string line;
                while (std::getline(ifs, line)) {
                    const GitIgnore::BS_PAIR p {parseLine(line)};
                    if (!p.second.empty()) 
                        absolute.emplace_back(p.first, p.second);
                }
            }

            // Read from index (.git/index) - all staged .gitignore files
            fs::path indexFilePath {repoFile({"index"})};
            GitIndex index {GitIndex::readFromFile(indexFilePath)};
            for (const GitIndex::GitIndexEntry &entry: index.getEntries()) {
                if (entry.name == ".gitignore" || entry.name.ends_with("/.gitignore")) {
                    const std::string dirName {fs::path(entry.name).parent_path().string()};
                    std::string contents {readObject<GitBlob>(entry.sha)->serialize()};
                    for (auto view: std::views::split(contents, '\n')) {
                        const GitIgnore::BS_PAIR p {parseLine(std::string_view{view})};
                        if (!p.second.empty())
                            scoped[dirName].emplace_back(p.first, p.second);
                    }
                }
            }

            return GitIgnore{absolute, scoped};
        }

        std::pair<bool, std::string> getActiveBranch() const {
            std::string headContents {readTextFile(repoFile({"HEAD"}))};
            if (headContents.starts_with("ref: refs/heads/"))
                return {false, headContents.substr(16, headContents.size() - 17)};
            return {true, headContents.substr(headContents.size() - 1)};
        }

        std::string getStatus() const {
            // Accumulate to string stream
            std::ostringstream oss;

            // Get current branch details
            bool isDetached; std::string currentBranchDetails;
            std::tie(isDetached, currentBranchDetails) = getActiveBranch();
            if (isDetached) oss << "HEAD detached at " << currentBranchDetails << "\n";
            else oss << "On branch " << currentBranchDetails << "\n";

            // Get all the files in the repo into map for easy lookup
            stdx::ordered_map<std::string, short> allFiles;
            for (const fs::directory_entry &entry: fs::recursive_directory_iterator(workTree)) {
                fs::path relPath {fs::relative(entry, workTree)};
                if (*relPath.begin() != ".git")
                    allFiles.insert(relPath.string(), 1);
            }

            // If HEAD cannot be resolved, it is a fresh repo. 
            // We can skip most of these portions
            if (!findObject("HEAD").empty()) {

                // Get a flat map of all tree entires in head recursively with its sha
                std::unordered_map<std::string, std::string> head;
                std::stack<std::pair<std::string, std::string>> stk {{{"HEAD", ""}}};
                while (!stk.empty()) {
                    std::string ref, prefix;
                    std::tie(ref, prefix) = std::move(stk.top()); stk.pop();
                    std::unique_ptr<GitTree> tree {readObject<GitTree>(findObject(ref, "tree"))};
                    for (const GitLeaf &leaf: *tree) {
                        std::string fullPath {(fs::path(prefix) / leaf.path).string()};
                        if (leaf.mode.starts_with("04"))
                            stk.emplace(leaf.sha, fullPath);
                        else
                            head.emplace(fullPath, leaf.sha);
                    }
                }

                // Compare diff between HEAD and index
                oss << "\nChanges to be committed:\n";
                fs::path indexFilePath {repoFile({"index"})};
                GitIndex index {GitIndex::readFromFile(indexFilePath)};
                for (const GitIndex::GitIndexEntry &entry: index.getEntries()) {
                    auto it {head.find(entry.name)};
                    if (it != head.end()) {
                        if (it->second != entry.sha)
                            oss << "  modified: " << entry.name << '\n';
                        head.erase(it);
                    } else {
                        oss << "  added: " << entry.name << '\n';
                    }
                }

                // Keys still left head are ones that weren't found in the index
                for (const std::pair<const std::string, std::string> &kv: head)
                    oss << "  deleted: " << kv.first << '\n';

                // Travel the index, comparing real files against cached versions
                oss << "\nChanges not staged for commit:\n";
                for (const GitIndex::GitIndexEntry &entry: index.getEntries()) {
                    fs::path fullPath {workTree / entry.name};
                    if (!fs::exists(fullPath))
                        oss << "  deleted: " << entry.name << '\n';
                    else {
                        std::chrono::duration fmtime {
                            std::chrono::time_point_cast<std::chrono::nanoseconds>(
                                    fs::last_write_time(fullPath)).time_since_epoch()};

                        long emtimeNS {static_cast<long>(entry.mtime.seconds * 10e9 + entry.mtime.nanoseconds)};
                        long fmtimeNS {std::chrono::duration_cast<std::chrono::nanoseconds>(fmtime).count()};
                        if (emtimeNS != fmtimeNS) {
                            // Read as binary - create a blob and pass into writeObj func to get sha 
                            std::ifstream ifs {fullPath, std::ios::binary};
                            std::ostringstream ofs; ofs << ifs.rdbuf();
                            std::string sha {writeObject(std::make_unique<GitBlob>("", ofs.str()))};
                            if (sha != entry.sha)
                                oss << "  modified: " << entry.name << '\n';
                        }
                    }

                    // Removed visited entries, if something exists after the loop
                    // we know these are untracked ones not in the index
                    if (allFiles.exists(entry.name)) allFiles.erase(entry.name);
                }
            } 

            else {
                oss << "\nNo commits yet\n";
            }

            // List untracked files
            GitIgnore ignore {gitIgnore()};
            oss << "\nUntracked files:\n";
            for (const std::pair<std::string, short> &kv: allFiles)
                if (!ignore.check(kv.first))
                    oss << "  " << kv.first << '\n';

            std::string result {oss.str()};
            if (!result.empty() && result.back() == '\n')
                result.pop_back();
            return result;
        }

        GitIndex rm(const std::vector<std::string>& paths, bool delete_ = true, bool skipMissing = false) {
            // Get list of abs paths to remove, ensure 
            // no path exists outside of git workdir
            std::unordered_set<fs::path> absPaths;
            for (const std::string &path: paths) {
                fs::path absPath {fs::absolute(path)};
                std::string relativePath {std::filesystem::relative(path, workTree)};
                if (relativePath.substr(0, 2) == "..")
                    throw std::runtime_error("Cannot remove paths outside of worktree: " + path);
                absPaths.insert(absPath);
            }

            // Iterate through index, check which files to remove and which to keep
            GitIndex index{GitIndex::readFromFile(repoFile({"index"}))};
            std::vector<GitIndex::GitIndexEntry> &entries{index.getEntries()};
            std::vector<fs::path> toDelete;
            for (auto it {entries.begin()}; it != entries.end();) {
                fs::path fullPath {workTree / it->name}; 
                if (absPaths.find(fullPath) != absPaths.end()) {
                    toDelete.emplace_back(fullPath);
                    it = entries.erase(it);
                    absPaths.erase(fullPath);
                } else {
                    ++it;
                }
            }

            // If we still have undeleted entries, user provided incorrect pathspec
            if (!absPaths.empty() && !skipMissing)
                throw std::runtime_error("Cannot remove paths not in index: " + absPaths.begin()->string());

            // If delete flag set, remove from file system
            if (delete_) for (const fs::path &path: toDelete) fs::remove(path);

            // Write index to file
            index.writeToFile(repoFile({"index"}));
            return index;
        }

        GitIndex add(const std::vector<std::string>& paths) {
            // Remove existing paths from index
            GitIndex index {rm(paths, false, true)};

            // Get list of abs paths to add, ensure 
            // path exists within worktree & is a valid file
            std::vector<std::pair<fs::path, fs::path>> absPaths;
            for (const std::string &path: paths) {
                fs::path absPath {fs::absolute(path)};
                std::string relPath {std::filesystem::relative(path, workTree)};
                if (relPath.substr(0, 2) == ".." || !fs::exists(absPath))
                    throw std::runtime_error("Not a file inside the worktree: " + path);
                absPaths.emplace_back(absPath, relPath);
            }

            std::vector<GitIndex::GitIndexEntry> &entries{index.getEntries()};
            for (const auto &[fullPath, relPath]: absPaths) {
                std::ifstream ifs {fullPath, std::ios::binary};
                std::ostringstream ofs; ofs << ifs.rdbuf();
                std::string sha {writeObject(std::make_unique<GitBlob>("", ofs.str()), true)};

                // Get the file stat
                struct stat statBuf; std::string fullPathStr {fullPath.string()};
                if (stat(fullPath.c_str(), &statBuf) != 0) {
                    throw std::runtime_error("Failed to stat file: " + fullPath.string());
                }

                // Extract Fields from file stat
                unsigned int  ctime_s {static_cast<unsigned int>(statBuf.st_ctime)};
                unsigned int  mtime_s {static_cast<unsigned int>(statBuf.st_mtime)};
                unsigned int ctime_ns {static_cast<unsigned int>(statBuf.st_ctim.tv_nsec % 1'000'000'000)};
                unsigned int mtime_ns {static_cast<unsigned int>(statBuf.st_mtim.tv_nsec % 1'000'000'000)};
                unsigned int      dev {static_cast<unsigned int>(statBuf.st_dev)};
                unsigned int    inode {static_cast<unsigned int>(statBuf.st_ino)};
                unsigned int      uid {static_cast<unsigned int>(statBuf.st_uid)};
                unsigned int      gid {static_cast<unsigned int>(statBuf.st_gid)};
                unsigned int    fsize {static_cast<unsigned int>(statBuf.st_size)};

                // Add to git index
                entries.emplace_back(GitIndex::GitIndexEntry{
                    .ctime={ctime_s, ctime_ns}, .mtime={mtime_s, mtime_ns}, 
                    .dev=dev, .inode=inode, .modeType=0b1000, .modePerms=0644,
                    .uid=uid, .gid=gid, .fsize=fsize, .sha=sha,
                    .flagStage=false, .assumeValid=false, .name=relPath
                });
            }

            // Write index to file
            index.writeToFile(repoFile({"index"}));
            return index;
        }

        void commit(const std::string &message) {
            std::string treeSha {writeIndexAsTree()}; 
            std::string parentSha {findObject("HEAD")}; 

            // Get user.name and user.email from ~/.gitconfig
            char *HOME_DIR {std::getenv("HOME")};
            INI::Parser parser; 
            parser.reads(readTextFile(fs::path{HOME_DIR? HOME_DIR: ""} / ".gitconfig"));
            parser.reads(readTextFile(gitDir / "config"), true); // ignore duplicates & override
            if (!parser.exists("user", "name") || !parser.exists("user", "email"))
                throw std::runtime_error("user.name / user.email not set.");

            // Get the time related info
            std::chrono::time_point now {std::chrono::system_clock::now()};
            std::time_t now_c {std::chrono::system_clock::to_time_t(now)};
            std::tm local_tm = *std::localtime(&now_c);
            std::tm utc_tm = *std::gmtime(&now_c);
            int offset_seconds = (local_tm.tm_hour - utc_tm.tm_hour) * 3600 + (local_tm.tm_min - utc_tm.tm_min) * 60;
            std::string tz {std::format("{}{:02}{:02}", (offset_seconds >= 0 ? "+" : "-"), 
                    std::abs(offset_seconds) / 3600, (std::abs(offset_seconds) % 3600) / 60)};

            // Committer details
            std::string author {std::format("{} <{}> {} {}", parser["user"]["name"], 
                    parser["user"]["email"], now_c, tz)};

            std::ostringstream oss;
            oss << "tree " << treeSha << '\n';
            if (!parentSha.empty()) 
                oss << "parent " << parentSha << '\n';
            oss << "author " << author << "\n"
                << "committer " << author << "\n\n"
                << (message | trim) << "\n";

            // Write the commit object to disk
            std::string commitSha {writeObject(std::make_unique<GitCommit>("", oss.str()), true)};

            // Update HEAD if detached, else update active branch ref
            bool isDetached; std::string currentBranchDetails;
            std::tie(isDetached, currentBranchDetails) = getActiveBranch();
            fs::path writePath {isDetached? repoFile({"HEAD"}): repoFile({"refs", "heads", currentBranchDetails})};
            writeTextFile(commitSha + '\n', writePath);
        }
};

int main(int argc, char **argv) {
    argparse::ArgumentParser argparser{"git"};
    argparser.description("CGit: A lite C++ clone of Git");

    // init command
    argparse::ArgumentParser initParser{"init"};
    initParser.description("Initialize a new, empty repository.");
    initParser.addArgument("path", argparse::POSITIONAL)
        .defaultValue(".").help("Where to create the repository.");

    // cat-file command
    argparse::ArgumentParser catFileParser{"cat-file"};
    catFileParser.description("Provide content of repository objects.");
    catFileParser.addArgument("object", argparse::POSITIONAL)
        .required().help("The object to display.");

    // hash-object command
    argparse::ArgumentParser hashObjectParser{"hash-object"};
    hashObjectParser.description("Compute object ID and optionally creates a blob from a file.");
    hashObjectParser.addArgument("type").alias("t").help("Specify the type.").defaultValue("blob");
    hashObjectParser.addArgument("path").required().help("Read object from <path>.");
    hashObjectParser.addArgument("write", argparse::NAMED).alias("w")
        .help("Actually write the object into the database.")
        .implicitValue(true).defaultValue(false);

    // log command
    argparse::ArgumentParser logParser{"log"};
    logParser.description("Display history of a given commit.")
        .epilog("Equivalent to `git log --pretty=raw`");
    logParser.addArgument("commit").defaultValue("HEAD").help("Commit to start at.");
    logParser.addArgument("max-count").scan<long>().defaultValue(-1l).alias("n").help("Limit the number of commits displayed.");

    // ls-tree command
    argparse::ArgumentParser lsTreeParser{"ls-tree"};
    lsTreeParser.description("Pretty-print a tree object.");
    lsTreeParser.addArgument("tree", argparse::POSITIONAL).help("A tree-ish object.").required();
    lsTreeParser.addArgument("recursive", argparse::NAMED).alias("r").defaultValue(false)
        .implicitValue(true).help("Recurse into subtrees.");

    // checkout commnad
    argparse::ArgumentParser checkoutParser{"checkout"};
    checkoutParser.description("Checkout a commit inside of a directory.");
    checkoutParser.addArgument("commit", argparse::POSITIONAL)
        .help("The commit or tree to checkout.").required();
    checkoutParser.addArgument("path", argparse::POSITIONAL)
        .help("The EMPTY directory to checkout on.").required();

    // show-ref command
    argparse::ArgumentParser showRefParser{"show-ref"};
    showRefParser.description("List all references.");

    // tag command
    argparse::ArgumentParser tagParser{"tag"};
    tagParser.description("List and create tags.");
    tagParser.addArgument("create-tag-object", argparse::NAMED).alias("a")
        .help("Whether to create a tag object.").defaultValue(false).implicitValue(true);
    tagParser.addArgument("name").help("The new tag's name.");
    tagParser.addArgument("object").help("The object the new tag will point to").defaultValue("HEAD");

    // rev-parse command
    argparse::ArgumentParser revPParser{"rev-parse"};
    revPParser.description("Parse revision (or other objects) identifiers");
    revPParser.addArgument("name", argparse::POSITIONAL).help("The name to parse.").required();
    revPParser.addArgument("type", argparse::NAMED).alias("t").defaultValue("")
        .help("Specify the expected type - ['blob', 'commit', 'tag', 'tree']");

    // ls-files command
    argparse::ArgumentParser lsFilesParser{"ls-files"};
    lsFilesParser.description("List all staged files.");
    lsFilesParser.addArgument("verbose", argparse::NAMED).alias("v")
        .defaultValue(false).implicitValue(true).help("Show everything.");

    // check-ignore command
    argparse::ArgumentParser checkIgnoreParser{"check-ignore"};
    checkIgnoreParser.description("Check path(s) against ignore rules.");
    checkIgnoreParser.addArgument("path", argparse::POSITIONAL).required()
        .scan<std::vector<std::string>>().help("Paths to check.");

    // status command
    argparse::ArgumentParser statusParser{"status"};
    statusParser.description("Show the working tree status.");

    // rm command
    argparse::ArgumentParser rmParser{"rm"};
    rmParser.description("Remove files from the working tree and the index.");
    rmParser.addArgument("cached", argparse::NAMED).defaultValue(false).implicitValue(true)
        .help("Unstage and remove paths only from the index.");
    rmParser.addArgument("path", argparse::POSITIONAL).required().help("Files to remove.")
        .scan<std::vector<std::string>>();

    // add command
    argparse::ArgumentParser addParser{"add"};
    addParser.description("Add files contents to the index.");
    addParser.addArgument("path", argparse::POSITIONAL).required().help("Files to add.")
        .scan<std::vector<std::string>>();

    // commit command
    argparse::ArgumentParser commitParser{"commit"};
    commitParser.description("Record changes to the repository.");
    commitParser.addArgument("message", argparse::NAMED).required().alias("m")
        .help("Message to associate with this commit.");

    // Add all the subcommands
    argparser.addSubcommand(initParser);
    argparser.addSubcommand(catFileParser);
    argparser.addSubcommand(hashObjectParser);
    argparser.addSubcommand(logParser);
    argparser.addSubcommand(lsTreeParser);
    argparser.addSubcommand(checkoutParser);
    argparser.addSubcommand(showRefParser);
    argparser.addSubcommand(tagParser);
    argparser.addSubcommand(revPParser);
    argparser.addSubcommand(lsFilesParser);
    argparser.addSubcommand(checkIgnoreParser);
    argparser.addSubcommand(statusParser);
    argparser.addSubcommand(rmParser);
    argparser.addSubcommand(addParser);
    argparser.addSubcommand(commitParser);

    // Parser the arguments
    argparser.parseArgs(argc, argv);

    if (initParser.ok()) {
        std::string path {initParser.get("path")};
        GitRepository repo(path, true);
        std::cout << "Initialized empty Git repository in " << repo.repoDir() << '\n';
    }

    else if (catFileParser.ok()) {
        std::string objectHashPart {catFileParser.get("object")};
        GitRepository repo {GitRepository::findRepo()};
        std::string objectHash {repo.findObject(objectHashPart)};
        std::cout << repo.readObject(objectHash)->serialize() << '\n';
    }

    else if (hashObjectParser.ok()) {
        bool writeFile {catFileParser.get<bool>("write")};
        std::string fmt {catFileParser.get("type")}, path {catFileParser.get("path")};
        std::string data {readTextFile(path)};

        std::unique_ptr<GitObject> obj;
        if (fmt == "tag") 
            obj =    std::make_unique<GitTag>("", data);
        else if (fmt == "tree") 
            obj =   std::make_unique<GitTree>("", data);
        else if (fmt == "blob") 
            obj =   std::make_unique<GitBlob>("", data);
        else if (fmt == "commit") 
            obj = std::make_unique<GitCommit>("", data);
        else
            throw std::runtime_error("Unknown type " + fmt + "!");

        std::cout << GitRepository::findRepo().writeObject(obj, writeFile) << '\n';
    }

    else if (logParser.ok()) {
        long maxCount {logParser.get<long>("max-count")};
        std::string objectHash {logParser.get("commit")};
        GitRepository repo {GitRepository::findRepo()}; 
        std::cout << repo.getLog(objectHash, maxCount);
        if (maxCount != 0) std::cout << '\n';
    }

    else if (lsTreeParser.ok()) {
        bool recurse {lsTreeParser.get<bool>("recursive")};
        std::string ref {lsTreeParser.get("tree")};
        std::cout << GitRepository::findRepo().lsTree(ref, recurse) << '\n';
    }

    else if (checkoutParser.ok()) {
        std::string ref {checkoutParser.get("commit")}, 
            path {checkoutParser.get("path")};
        GitRepository::findRepo().checkout(ref, path);
    }

    else if (showRefParser.ok()) {
        std::cout << GitRepository::findRepo().showAllRefs("refs", true, "refs") << '\n';
    }

    else if (tagParser.ok()) {
        GitRepository repo {GitRepository::findRepo()};
        if (tagParser.exists("name")) {
            bool createTagObj {tagParser.get<bool>("create-tag-object")};
            std::string name {tagParser.get("name")}, 
                ref {tagParser.get("object")};
            repo.createTag(name, ref, createTagObj);
        } else {
            std::string result {repo.showAllRefs("refs/tags", false, "")};
            std::cout << result;
            if (!result.empty()) std::cout << '\n';
        }
    }

    else if (revPParser.ok()) {
        std::string name {revPParser.get("name")}, 
            type {revPParser.get("type")};
        std::string result {GitRepository::findRepo().findObject(name, type, true)};
        std::cout << result;
        if (!result.empty()) std::cout << '\n';
    }

    else if (lsFilesParser.ok()) {
        bool verbose {lsFilesParser.get<bool>("verbose")};
        std::cout << GitRepository::findRepo().lsFiles(verbose) << '\n';
    }

    else if (checkIgnoreParser.ok()) {
        std::vector<std::string> paths {checkIgnoreParser.get<std::vector<std::string>>("path")};
        GitIgnore rules {GitRepository::findRepo().gitIgnore()};
        for (const std::string &path: paths)
            if (rules.check(path))
                std::cout << path << '\n';
    }

    else if (statusParser.ok()) {
        std::cout << GitRepository::findRepo().getStatus() << '\n';
    }

    else if (rmParser.ok()) {
        bool cached {rmParser.get<bool>("cached")};
        std::vector<std::string> paths {rmParser.get<std::vector<std::string>>("path")};
        GitRepository::findRepo().rm(paths, cached);
    }

    else if (addParser.ok()) {
        std::vector<std::string> paths {addParser.get<std::vector<std::string>>("path")};
        GitRepository::findRepo().add(paths);
    }

    else if (commitParser.ok()) {
        GitRepository::findRepo().commit(commitParser.get("message"));
    }
    
    else {
        std::cout << argparser.getHelp() << '\n';
    }

    return 0;
}
