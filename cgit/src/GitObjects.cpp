#include "../include/GitObjects.hpp"
#include "../include/utils.hpp"
#include "../../misc/ordered_map.hpp"
#include <chrono>
#include <string>
#include <vector>

namespace cr = std::chrono;

GitObject::GitObject(const std::string &sha, const std::string &fmt): sha(sha), fmt(fmt) {}

GitBlob::GitBlob(const std::string &sha, const std::string &data): 
    GitObject(sha, "blob") { deserialize(data); }

void GitBlob::deserialize(const std::string &data) { this->data = data; }
[[nodiscard]] std::string GitBlob::serialize() const { return data; }

GitCommit::GitCommit(const std::string &sha, const std::string &raw, const std::string &fmt): 
    GitObject(sha, fmt) { deserialize(raw); }

void GitCommit::set(const std::string &key, std::vector<std::string> &&value) { data[key] = value; }

std::vector<std::string> GitCommit::get(const std::string &key) const {
    return data.find(key) != data.end()? data.at(key): std::vector<std::string>{};
}

void GitCommit::deserialize(const std::string &raw) {
    // State for parsing the text string
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

    // Set the commit time UTC if found, else set curr sys time
    if (data.find("committer") != data.end()) {
        std::string committerMsg {data["committer"][0]};
        std::size_t tzStartPos {committerMsg.rfind(' ')};
        std::size_t tsStartPos {committerMsg.rfind(' ', tzStartPos - 1)};
        std::string ts {committerMsg.substr(tsStartPos + 1, tzStartPos - tsStartPos)};
        commitUTC = cr::system_clock::from_time_t(std::stol(ts));
    } else commitUTC = cr::system_clock::now();
}

[[nodiscard]] std::string GitCommit::serialize() const {
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

int GitLeaf::pathCompare(const GitLeaf &l1, const GitLeaf &l2) {
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

GitLeaf::GitLeaf(
    const std::string &mode, const std::string &path, 
    const std::string &sha, bool shaInBinary
):
    mode(mode), path(path), sha(shaInBinary? binary2Sha(sha): sha) {}

GitLeaf::GitLeaf(const GitLeaf &other): mode(other.mode), path(other.path), sha(other.sha) {}

GitLeaf::GitLeaf(GitLeaf &&other): 
    mode(std::move(other.mode)), 
    path(std::move(other.path)), 
    sha(std::move(other.sha))
{ }

// Move assignment
GitLeaf &GitLeaf::operator= (GitLeaf &&other) noexcept {
   if (this != &other) {
        mode = std::move(other.mode); 
        path = std::move(other.path); 
        sha = std::move(other.sha); 
   }
   return *this;
}

[[nodiscard]] std::string GitLeaf::serialize() const {
    std::string result {mode + ' ' + path}; 
    result.push_back('\x00');
    result.append(sha2Binary(sha));
    return result;
}

GitTree::GitTree(const std::string &sha, const std::string& raw): 
    GitObject(sha, "tree") { deserialize(raw); }

GitTree::GitTree(const std::vector<GitLeaf> &data): 
    GitObject("", "tree"), data(data) {}

void GitTree::deserialize(const std::string &raw) {
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
                throw std::runtime_error("Failed to deserialized tree - Expected to have 20 bytes of char for SHA");
            acc.assign(raw, i, 20); i += 19;
            data.emplace_back(mode, path, acc);
            mode.clear(); path.clear(); acc.clear();
            state = START;
        }

        // Update for each loop
        i++;
    }
}

// Simply sort the container and serialize the leaves
[[nodiscard]] std::string GitTree::serialize() const {
    std::string result;
    std::sort(data.begin(), data.end());
    for (const GitLeaf &leaf: data) {
        result.append(leaf.serialize());
    }
    return result;
}

GitTag::GitTag(const std::string &sha, const std::string& raw): 
    GitCommit(sha, raw, "tag") {}
