#include "../include/GitRepository.hpp"
#include "../include/utils.hpp"
#include "../include/GitPack.hpp"

#include "../../misc/zhelper.hpp"
#include "../../cryptography/hashlib.hpp"

#include <sys/stat.h>
#include <filesystem>
#include <functional>
#include <ranges>
#include <regex>
#include <unordered_set>

namespace cr = std::chrono;

fs::path GitRepository::repoPath(const std::initializer_list<std::string_view> &parts) const {
    fs::path result {gitDir};
    for (const std::string_view &part: parts)
        result /= part;
    return result;
}

std::string GitRepository::getPackedRef(const std::string &key) const {
    auto it {packedRefs.find(key)};
    return it == packedRefs.end()? "": it->second;
}

void GitRepository::parsePackedRefs(const fs::path &parsedRefsFile) {
    std::ifstream ifs {parsedRefsFile};
    std::string line;
    while (std::getline(ifs, line)) {
        line = trim(line);
        if (!line.empty() && line.at(0) != '#') {
            std::vector<std::string> splits = 
                line 
                | std::views::split(' ')
                | std::views::transform([&](auto &&split) { return trim(split | std::ranges::to<std::string>()); })
                | std::ranges::to<std::vector<std::string>>(); 

            if (splits.size() != 2)
                throw std::runtime_error("Invalid packed-refs format");

            // Add '<SHA> <ref> into the map'
            packedRefs[splits[1]] = splits[0];
        }
    }
}

std::string GitRepository::writeIndexAsTree() {
    // Extract git index
    GitIndex index{GitIndex::readFromFile(repoFile({"index"}))};
    std::vector<GitIndex::GitIndexEntry> &entries{index.getEntries()};

    // Create a directory tree like structure
    std::unordered_map<fs::path, std::unordered_set<fs::path>> directoryTree;
    std::unordered_map<std::string, const GitIndex::GitIndexEntry*> lookup;
    for (const GitIndex::GitIndexEntry &entry: entries) {
        lookup.emplace(entry.name, &entry);
        fs::path curr {fs::path(entry.name)};

        // Traverse up the parents until we encounter a parent already inserted
        while (!curr.empty() && curr != curr.parent_path()) {
            fs::path parent {curr.parent_path()};
            bool parentAlreadyExists {directoryTree.find(parent) != directoryTree.end()};
            directoryTree[parent].emplace(curr);
            if (parentAlreadyExists) break;
            else curr = parent;
        }
    }

    // Recursively construct and write tree objects, ensuring child nodes are written before the parent
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

    // Start from the root and return the SHA of the final tree object
    auto [_, __, sha] = backtrack("");
    return sha;
}

GitRepository::GitRepository(const fs::path &path, bool force):
    workTree(path), gitDir(path / ".git")
{
    if (!force) {
        if (!fs::is_directory(gitDir))
            throw std::runtime_error("Not a Git Repository: " + fs::canonical(gitDir).string());
        if (!fs::is_regular_file(gitDir / "config"))
            throw std::runtime_error("Configuration file missing: " + repoFile({"config"}).string());

        // Read the config file
        conf.reads(readTextFile(gitDir / "config"));
        std::string repoVersion {"** MISSING **"};
        if (!conf.exists("core", "repositoryformatversion") || (repoVersion = conf["core"]["repositoryformatversion"]) != "0")
            throw std::runtime_error("Unsupported `repositoryformatversion`: " + repoVersion);

        // Parse packed-refs - silently fails if missing
        parsePackedRefs(repoFile({"packed-refs"}));
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

    // Guaranteed that the paths exist now, lets clean em up
    gitDir = fs::canonical(gitDir);
    workTree = fs::canonical(workTree); 
}

fs::path GitRepository::repoDir(const std::initializer_list<std::string_view> &parts, bool create) const {
    fs::path fpath {repoPath(parts)};
    if (create) fs::create_directories(fpath);
    return fpath;
}

fs::path GitRepository::repoDir() const { return gitDir; }

fs::path GitRepository::repoFile(const std::initializer_list<std::string_view> &parts, bool create) const {
    fs::path fpath {repoPath(parts)};
    if (create) fs::create_directories(fpath.parent_path());
    return fpath;
}

GitRepository GitRepository::findRepo(const fs::path &path_) {
    fs::path path {fs::absolute(path_)};
    if (fs::exists(path / ".git"))
        return GitRepository(path);
    else if (!path.has_parent_path() || path == path.parent_path())
        throw std::runtime_error("Not a git directory");
    else
        return findRepo(path.parent_path());
}

std::string GitRepository::writeObject(const std::unique_ptr<GitObject> &obj, bool write) const {
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

std::string GitRepository::findObject(const std::string &name, const std::string &fmt, bool follow) const {
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

            // Search for matching loose object
            if (fs::exists(path)) {
                for (const fs::directory_entry &entry: fs::directory_iterator(path)) {
                    const std::string fname {entry.path().filename()};
                    if (fname.starts_with(remaining))
                        candidates.emplace_back(prefix + fname);
                }
            }

            // Search for matching packed object
            std::vector<std::string> asPack {GitPack(repoDir({"objects", "pack"})).refResolve(part)};
            candidates.insert(candidates.end(), asPack.begin(), asPack.end());
        }

       // Check if `name` matches a tag reference
        std::string asTag {refResolve("refs/tags/" + name)};
        if (!asTag.empty()) candidates.emplace_back(asTag);

       // Check if `name` matches a branch reference
        std::string asBranch {refResolve("refs/heads/" + name)};
        if (!asBranch.empty()) candidates.emplace_back(asBranch);
    }

    // Ensure only 1 candidate was found
    if (candidates.size() != 1)
        throw std::runtime_error(
            "Name resolution failed: " + name + ".\nExpected to have only 1 matching"
            " candidate, found " + std::to_string(candidates.size()));

    std::string sha {candidates[0]};
    if (fmt.empty()) return sha;

    // Follow references to match the expected object type
    while (true) {
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

std::string GitRepository::readObjectType(const std::string &objectHash) const {
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

std::unique_ptr<GitObject> GitRepository::readObject(const std::string &objectHash) const {
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

[[nodiscard]] std::string GitRepository::refResolve(const std::string &path) const {
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

GitIgnore GitRepository::gitIgnore() const {
    std::vector<GitIgnore::BS_PAIR> absolute;
    std::unordered_map<std::string, std::vector<GitIgnore::BS_PAIR>> scoped;

    // Helper to categories a string into a pair<bool, string>
    const std::function<GitIgnore::BS_PAIR(const std::string&)> parseLine {[](const std::string &line_) -> GitIgnore::BS_PAIR {
        std::string line {trim(line_)};
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
                const GitIgnore::BS_PAIR p {parseLine(view | std::ranges::to<std::string>())};
                if (!p.second.empty())
                    scoped[dirName].emplace_back(p.first, p.second);
            }
        }
    }

    return GitIgnore{absolute, scoped};
}

std::pair<bool, std::string> GitRepository::getActiveBranch() const {
    std::string headContents {readTextFile(repoFile({"HEAD"}))};
    if (headContents.starts_with("ref: refs/heads/"))
        return {false, headContents.substr(16, headContents.size() - 17)};
    return {true, headContents.substr(headContents.size() - 1)};
}

std::vector<std::pair<fs::path, fs::path>> GitRepository::collectFiles(const std::vector<std::string> &paths) const {
    std::vector<std::pair<fs::path, fs::path>> result;

    // Explictly allow adding gitignored files if directly passed
    // When adding directories we skip gitignored files
    GitIgnore ignore {gitIgnore()};

    for (std::string path: paths) {
        if (path.empty()) 
            throw std::runtime_error("Path input provided cannot be empty.");

        // Expand dot to CWD
        if (path == ".") path = fs::current_path();

        fs::path absPath {fs::absolute(path)};
        fs::path relativePath {fs::relative(path, workTree)};
        if (relativePath.string().substr(0, 2) == "..")
            throw std::runtime_error("Cannot include paths outside of worktree: " + path);

        if (fs::is_directory(absPath)) {
            for (fs::recursive_directory_iterator it {absPath}, end; it != end; it++) {
                const fs::directory_entry &entry {*it};
                fs::path currAbsPath{fs::absolute(entry)}, currRelPath{fs::relative(entry, workTree)};

                // Skip any path that contains ".git/" at any level
                bool skip {ignore.check(currRelPath)};
                if (!skip && fs::is_directory(entry)) {
                    for (const fs::path &pathComponent: currRelPath) {
                        if (pathComponent == ".git") {
                            skip = true; break;
                        }
                    }
                }

                // Disable backtracking into skipped folders
                if (skip) it.disable_recursion_pending();

                // Only include files not skipped
                else if (!skip && !fs::is_directory(currAbsPath))
                    result.emplace_back(currAbsPath, currRelPath);
            }
        }

        else {
            result.emplace_back(absPath, relativePath);
        }
    }

    return result;
}

void GitRepository::checkout(const std::string &ref, const fs::path &checkoutPath) const {
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

void GitRepository::createTag(const std::string &name, const std::string &ref, bool createTagObj) const {
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

GitIndex GitRepository::rm(const std::vector<std::pair<fs::path, fs::path>>& paths, bool delete_, bool skipMissing) {
    // Get list of abs paths to remove, ensure no path 
    // exists outside of git workdir by using collectFiles
    std::unordered_set<fs::path> absPaths;
    for (const std::pair<fs::path, fs::path> &pathPair: paths) {
        absPaths.insert(pathPair.first);
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

GitIndex GitRepository::add(const std::vector<std::pair<fs::path, fs::path>>& paths) {
    // Remove existing paths from index before writing them back
    // Two bool paramters to not physically delete them 
    // & to silently fail if missing few paths
    GitIndex index {rm(paths, false, true)};

    std::vector<GitIndex::GitIndexEntry> &entries{index.getEntries()};
    for (const auto &[fullPath, relPath]: paths) {
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

void GitRepository::commit(const std::string &message) {
    // Write the index as loose tree objects to disk
    std::string treeSha {writeIndexAsTree()};

    // Below logic to create the actual commit object
    std::string parentSha {findObject("HEAD")}; 

    // Get user.name and user.email from ~/.gitconfig
    char *HOME_DIR {std::getenv("HOME")};
    INI::Parser parser; 
    parser.reads(readTextFile(fs::path{HOME_DIR? HOME_DIR: ""} / ".gitconfig"));
    parser.reads(readTextFile(gitDir / "config"), true); // ignore duplicates & override
    if (!parser.exists("user", "name") || !parser.exists("user", "email"))
        throw std::runtime_error("user.name / user.email not set.");

    // Get the time related info
    cr::time_point now {cr::system_clock::now()};
    std::time_t now_c {cr::system_clock::to_time_t(now)};
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
        << trim(message) << "\n";

    // Write the commit object to disk
    std::string commitSha {writeObject(std::make_unique<GitCommit>("", oss.str()), true)};

    // Update HEAD if detached, else update active branch ref
    bool isDetached; std::string currentBranchDetails;
    std::tie(isDetached, currentBranchDetails) = getActiveBranch();
    fs::path writePath {isDetached? repoFile({"HEAD"}): repoFile({"refs", "heads", currentBranchDetails})};
    writeTextFile(commitSha + '\n', writePath);
}

std::string GitRepository::getLog(const std::string &commit, long maxCount) const {
    // Stop early
    if (maxCount == 0) return "";

    // Check if no commits
    if (refResolve("HEAD").empty())
        throw std::runtime_error("HEAD does not have any commits yet.");

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
    return trim(oss.str());
}

std::string GitRepository::lsTree(const std::string &ref, bool recurse, const fs::path &prefix) const {
    std::string sha {findObject(ref, "tree")};
    std::unique_ptr<GitTree> tree {readObject<GitTree>(sha)};
    std::ostringstream oss;
    for (const GitLeaf &leaf: *tree) {
        std::string type;
        if (leaf.mode.starts_with("04"))
            type = "tree";   // directory
        else if (leaf.mode.starts_with("10"))
            type = "blob";   // regular file
        else if (leaf.mode.starts_with("12"))
            type = "blob";   // symlinked contents
        else if (leaf.mode.starts_with("16"))
            type = "commit"; // Submodule
        else
            throw std::runtime_error(ref + ": Unknown tree mode: " + leaf.mode);

        fs::path leafPath {prefix / leaf.path};
        if (!recurse || type != "tree")
            oss << leaf.mode << ' ' << type << ' ' << leaf.sha << '\t' << leafPath.string();
        else
            oss << lsTree(leaf.sha, recurse, leafPath);
        oss << '\n';
    }

    // Remove extra space
    return trim(oss.str());
}

std::string GitRepository::showAllTags() const {
    // Gather all refs starting from prefix
    fs::path startPath {repoPath({"refs", "tags"})};
    std::vector<std::string> paths;
    for (const fs::directory_entry &entry: fs::recursive_directory_iterator(startPath)) {
        if (entry.is_regular_file()) 
            paths.emplace_back(fs::relative(entry, startPath));
    }

    // Gather all packed refs
    for (const std::pair<const std::string, std::string> &kv: packedRefs) {
        if (kv.first.starts_with("refs/tags/")) 
            paths.emplace_back(kv.first.substr(10));
    }

    // Sort alphabetically & write to stream
    std::sort(paths.begin(), paths.end());
    std::ostringstream oss;
    for (const std::string &path: paths) {
        oss << path << '\n';
    }

    // Remove extra space
    return trim(oss.str());
}

std::string GitRepository::showAllRefs() const {
    // Gather all refs starting from prefix
    fs::path startPath {repoPath({"refs"})};
    std::vector<std::pair<std::string, std::string>> paths;
    for (const fs::directory_entry &entry: fs::recursive_directory_iterator(startPath))
        if (entry.is_regular_file()) {
            fs::path relPath {"refs" / fs::relative(entry, startPath)};
            paths.emplace_back(relPath, refResolve(relPath));
        }

    // Gather all packed refs
    for (const std::pair<const std::string, std::string> &kv: packedRefs) {
        if (kv.first.starts_with("refs/")) 
            paths.emplace_back(kv.first, kv.second);
    }

    // Sort alphabetically
    std::sort(paths.begin(), paths.end(), [](auto p1, auto p2) { return p1.first < p2.first; });
    std::ostringstream oss;
    for (const std::pair<std::string, std::string> &p: paths) {
        oss << p.second << ' ' << p.first << '\n';
    }

    // Remove extra space
    return trim(oss.str());
}

std::string GitRepository::lsFiles(bool verbose) const {
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

    // Remove extra space
    return trim(oss.str());
}

std::string GitRepository::getStatus() const {
    // Accumulate to string stream
    std::ostringstream oss;

    // Get current branch details
    bool isDetached; std::string currentBranchDetails;
    std::tie(isDetached, currentBranchDetails) = getActiveBranch();
    if (isDetached) oss << "HEAD detached at " << currentBranchDetails << "\n";
    else oss << "On branch " << currentBranchDetails << "\n";

    // Get all the files in the repo into map for easy lookup
    GitIgnore ignore {gitIgnore()};
    stdx::ordered_map<std::string, short> allFiles;
    for (fs::recursive_directory_iterator it {workTree}, end; it != end; it++) {
        fs::path relPath {fs::relative(*it, workTree)};
        if (*relPath.begin() == ".git" || ignore.check(relPath))
            it.disable_recursion_pending();
        else
            allFiles.insert(relPath.string(), 1);
    }

    // If HEAD cannot be resolved, it is a fresh repo. 
    // We can skip most of these portions & jump to else
    bool freshRepo {refResolve("HEAD").empty()};
    if (freshRepo) oss << "\nNo commits yet\n";

    // Get a flat map of all tree entires in head recursively with its sha
    std::unordered_map<std::string, std::string> head;
    if (!freshRepo) {
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
    }

    // Compare diff between HEAD and index
    fs::path indexFilePath {repoFile({"index"})};
    if (fs::exists(indexFilePath)) {
        oss << "\nChanges to be committed:\n";
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
                cr::duration fmtime {
                    cr::time_point_cast<cr::nanoseconds>(
                            fs::last_write_time(fullPath)).time_since_epoch()};

                long emtimeNS {static_cast<long>(entry.mtime.seconds * 10e9 + entry.mtime.nanoseconds)};
                long fmtimeNS {cr::duration_cast<cr::nanoseconds>(fmtime).count()};
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
            // These are untracked ones not in the index - erase until parent dir
            if (allFiles.exists(entry.name)) {
                fs::path curr {entry.name};
                while (!curr.empty() && curr.parent_path() != curr) {
                    allFiles.erase(curr);
                    curr = curr.parent_path();
                }
            }
        }
    }

    // List untracked files
    std::unordered_set<std::string> untracked;
    oss << "\nUntracked files:\n";
    for (const std::pair<std::string, short> &kv: allFiles) {
        const std::string &entry {kv.first};

        // Skip ignored files
        if (ignore.check(entry)) continue;

        // If parent not untracked and is a file or a non empty folder
        if (untracked.find(fs::path(entry).parent_path()) == untracked.end() 
            && (!fs::is_directory(entry) || !fs::is_empty(entry))) {
            oss << "  " << entry;
            if (fs::is_directory(entry)) oss << '/';
            oss << '\n';
        }

        // Always add
        untracked.insert(entry);
    }

    // Remove extra space
    return trim(oss.str());
}
