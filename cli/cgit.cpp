#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <regex>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string_view>
#include <unordered_set>

#include "argparse.hpp"
#include "../misc/iniparser.hpp"
#include "../misc/zhelper.hpp"
#include "../cryptography/hashlib.hpp"

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
        static std::string sha2Binary(const std::string &sha) {
            std::string binSha;
            binSha.reserve(20);
            for (std::size_t i {0}; i < 40; i++) {
                std::string hexDigit {sha.substr(i, 2)};
                binSha.push_back(static_cast<char>(std::stoi(hexDigit, nullptr, 16)));
            }
            return binSha;
        }

        static std::string binary2Sha(const std::string &binSha) {
            std::ostringstream oss;
            oss << std::hex << std::setfill('0');
            for (const char &ch: binSha)
                oss << std::setw(2) << static_cast<int>(static_cast<unsigned char>(ch));
            return oss.str();
        }

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

        GitLeaf(const std::string &mode, const std::string &path, const std::string &binSha):
            mode(mode), path(path), sha(binary2Sha(binSha)) {}

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
            return mode + ' ' + path + '\x00' + sha2Binary(sha);
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

        // Iterators
        inline std::vector<GitLeaf>::const_iterator begin() const { return data.cbegin(); }
        inline std::vector<GitLeaf>::const_iterator end() const { return data.cend(); }

        void deserialize(const std::string &raw) {
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

        [[nodiscard]] std::string serialize() const {
            std::ostringstream oss;
            std::sort(data.begin(), data.end());
            for (const GitLeaf &leaf: data) {
                oss << leaf.serialize();
            }
            return oss.str();
        }
};

class GitTag: public GitCommit {
    public:
        using GitCommit::serialize, GitCommit::deserialize; 
        using GitCommit::get, GitCommit::set;
        GitTag(const std::string &sha, const std::string& raw): 
            GitCommit(sha, raw, "tag") {}
};

class GitRepository {
    private:
        mutable fs::path workTree, gitDir;
        INI::Parser conf;  

        fs::path repoPath(const std::initializer_list<std::string_view> &parts) const {
            fs::path result {gitDir};
            for (const std::string_view &part: parts)
                result /= part;
            return result;
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
            else if (!path.has_parent_path())
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
                }

                // Check for tag match
                std::string asTag {refResolve("refs/tags/" + name)};
                if (!asTag.empty()) candidates.emplace_back(asTag);

                // Check for branch match
                std::string asBranch {refResolve("refs/heads/" + name)};
                if (!asBranch.empty()) candidates.emplace_back(asBranch);
            }

            if (candidates.size() != 1)
                throw std::runtime_error("Expected to have only 1 matching candidate, found " + std::to_string(candidates.size()));

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
            std::string raw {zhelper::zread(path)};
            std::size_t fmtEndPos {raw.find(' ')};
            return raw.substr(0, fmtEndPos);
        }

        std::unique_ptr<GitObject> readObject(const std::string &objectHash) const {
            fs::path path {repoFile({"objects", objectHash.substr(0, 2), objectHash.substr(2)})};
            std::string raw {zhelper::zread(path)};

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

        std::string getLog(const std::string &objectHash, long maxCount) const {
            // Stop early
            if (maxCount == 0) return "";

            // Store the commit objects
            std::vector<std::unique_ptr<GitCommit>> logs;

            // DFS to read all the commits (until maxCount)
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
            std::ostringstream oss; std::size_t mc {static_cast<std::size_t>(maxCount)};
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
        std::string refResolve(const std::string &path) const {
            std::string currRef {"ref: " + path};
            while (currRef.starts_with("ref: ")) {
                fs::path path {repoFile({currRef.substr(5)})};
                if (!fs::is_regular_file(path)) return "";
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
};

int main(int argc, char **argv) {
    argparse::ArgumentParser argparser{"git"};
    argparser.description("CGit: A lite C++ clone of Git");

    // init command
    argparse::ArgumentParser initParser{"init"};
    initParser.description("Initialize a new, empty repository.");
    initParser.addArgument(argparse::Argument("path", argparse::POSITIONAL)
            .defaultValue(".").help("Where to create the repository."));

    // cat-file command
    argparse::ArgumentParser catFileParser{"cat-file"};
    catFileParser.description("Provide content of repository objects.");
    catFileParser.addArgument(argparse::Argument("object", argparse::POSITIONAL)
            .required().help("The object to display."));

    // hash-object command
    argparse::ArgumentParser hashObjectParser{"hash-object"};
    hashObjectParser.description("Compute object ID and optionally creates a blob from a file.");
    hashObjectParser.addArgument(argparse::Argument("type").alias("t")
            .help("Specify the type.").defaultValue("blob"));
    hashObjectParser.addArgument(argparse::Argument("write", argparse::NAMED)
            .alias("w").help("Actually write the object into the database.")
            .implicitValue(true).defaultValue(false));
    hashObjectParser.addArgument(argparse::Argument("path")
            .required().help("Read object from <path>."));

    // log command
    argparse::ArgumentParser logParser{"log"};
    logParser.description("Display history of a given commit.")
        .epilog("Equivalent to `git log --pretty=raw`");
    logParser.addArgument(argparse::Argument("commit").defaultValue("HEAD").help("Commit to start at."));
    logParser.addArgument(argparse::Argument("max-count").scan<long>().defaultValue(-1l)
            .alias("n").help("Limit the number of commits displayed."));

    // ls-tree command
    argparse::ArgumentParser lsTreeParser{"ls-tree"};
    lsTreeParser.description("Pretty-print a tree object.");
    lsTreeParser.addArgument(argparse::Argument("tree", argparse::POSITIONAL)
            .help("A tree-ish object.").required());
    lsTreeParser.addArgument(argparse::Argument("recursive", argparse::NAMED).alias("r")
            .defaultValue(false).implicitValue(true).help("Recurse into subtrees."));

    // checkout commnad
    argparse::ArgumentParser checkoutParser{"checkout"};
    checkoutParser.description("Checkout a commit inside of a directory.");
    checkoutParser.addArgument(argparse::Argument("commit", argparse::POSITIONAL)
            .help("The commit or tree to checkout.").required());
    checkoutParser.addArgument(argparse::Argument("path", argparse::POSITIONAL)
            .help("The EMPTY directory to checkout on.").required());

    // show-ref command
    argparse::ArgumentParser showRefParser{"show-ref"};
    showRefParser.description("List all references.");

    // tag command
    argparse::ArgumentParser tagParser{"tag"};
    tagParser.description("List and create tags.");
    tagParser.addArgument(argparse::Argument("create-tag-object", argparse::NAMED).alias("a")
        .help("Whether to create a tag object.").defaultValue(false).implicitValue(true));
    tagParser.addArgument(argparse::Argument("name").help("The new tag's name."));
    tagParser.addArgument(argparse::Argument("object").help("The object the new tag will point to")
        .defaultValue("HEAD"));

    // rev-parse command
    argparse::ArgumentParser revPParser{"rev-parse"};
    revPParser.description("Parse revision (or other objects) identifiers");
    revPParser.addArgument(argparse::Argument("name", argparse::POSITIONAL)
            .help("The name to parse.").required());
    revPParser.addArgument(argparse::Argument("type", argparse::NAMED).alias("t").defaultValue("")
            .help("Specify the expected type - ['blob', 'commit', 'tag', 'tree']"));

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
    
    else {
        std::cout << argparser.getHelp() << '\n';
    }

    return 0;
}
