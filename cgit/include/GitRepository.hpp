#pragma once

#include "GitIgnore.hpp"
#include "GitIndex.hpp"
#include "GitObjects.hpp"

#include "../../misc/iniparser.hpp"

#include <filesystem>

namespace fs = std::filesystem;

// The Orchestrator
class GitRepository {
    private:
        // workTree is parent folder containing `.git`
        mutable fs::path workTree; 

        // gitDir is the absolute path to `.git`
        mutable fs::path gitDir;

        // Parse the confs - repo local and global and store it here
        INI::Parser conf;  

        // sometimes, git refs are packed into a single file `.git/packed-refs`
        // Read the file and store the ref contents during init
        std::unordered_map<std::string, std::string> packedRefs;

        // What python's os.path.join("a", "b") does but inserts gitDir at front
        fs::path repoPath(const std::initializer_list<std::string_view> &parts) const;

        // Similar to refResolve, return if found else return an empty string
        std::string getPackedRef(const std::string &key) const;

        // Parse 'packed-refs' and append to packedRefs container
        // If file not found or file is missing, does nothing
        void parsePackedRefs(const fs::path &parsedRefsFile);

        // Constructs a Git tree from the index and writes to the repository.
        // This func is critical for git commit operations
        std::string writeIndexAsTree();

    public:
        // Force is used when performing a `git init`
        GitRepository(const fs::path &path, bool force = false);

        // Given folder names, concats and returns full path starting at `.git`. Create parameter can be used to create the folders
        fs::path repoDir(const std::initializer_list<std::string_view> &parts, bool create = false) const;

        // If no parameter passed, simply return gitDir
        // Useful for downstream clients using `GitRepository`
        fs::path repoDir() const;

        // Same as repoDir but for files, assumes the end part is the name of the file
        fs::path repoFile(const std::initializer_list<std::string_view> &parts, bool create = false) const;

        // Starting at curr directory, traverse up until we find a 
        // level containing `.git`. If not found, throws an error
        static GitRepository findRepo(const fs::path &path_ = ".");

        // Serializes the input `GitObject` and returns it sha1 hex. If `write` is true, write ObjectContents to disk 
        std::string writeObject(const std::unique_ptr<GitObject> &obj, bool write = false) const;

        // Resolve a given `name` to its corresponding SHA-1 hash.
        // Supports full or partial hashes, tags, branches, and HEAD references.
        // If `name` is a partial hash, searches both loose objects and packfiles.
        // If `fmt` is specified, ensures the resolved object is of the expected type.
        // If `follow` is true, recursively resolves tags and commits to trees when needed.
        std::string findObject(const std::string &name, const std::string &fmt = "", bool follow = true) const;

        // Determine the type of Git Object given its SHA-1 Hash as hex
        // First checks if exists as loose obj first else checks inside packfiles
        std::string readObjectType(const std::string &objectHash) const;

        // Extracts raw content, verifies format, and constructs the appropriate Git object
        std::unique_ptr<GitObject> readObject(const std::string &objectHash) const;

        // Read a Git object and cast it to a specific subclass (GitTag, GitTree, GitBlob, or GitCommit).
        template <typename T> std::unique_ptr<T> readObject(const std::string &objectHashPart) const {
            std::unique_ptr<GitObject> obj {readObject(objectHashPart)};
            T* casted {dynamic_cast<T*>(obj.get())};
            if (!casted) throw std::runtime_error("Invalid cast: GitObject is not of requested type: " + objectHashPart);
            return std::unique_ptr<T>(static_cast<T*>(obj.release()));
        }

        // Check for matches & return the full sha match if found
        // refResolve("HEAD") can be used to determine if the repo is 
        // 'fresh' without commits. If fresh, returns empty string
        [[nodiscard]] std::string refResolve(const std::string &path) const;

        // Constructs a GitIgnore object containing global and per-directory ignore rules.
        // Reads ignore patterns from:
        // 1. `.git/info/exclude` for repository-wide ignore rules.
        // 2. `.gitignore` files staged in the index for directory-scoped ignore rules.
        GitIgnore gitIgnore() const;

        // Retrieves the currently active branch in the repository.
        // Returns a pair: (bool isDetached, string ref):
        // - `false, <branch_name>` if on a named branch.
        // - ` true,  <commit_sha>` if in a detached HEAD state.
        std::pair<bool, std::string> getActiveBranch() const;

        // Collects files from the given paths, returning absolute and relative paths as pairs.
        //  -----
        // - If a directory is provided, it recursively collects all non-gitignored files.
        // - If a file is explicitly passed, it is included even if gitignored.
        // - Throws an error if the path is empty or outside the worktree.
        std::vector<std::pair<fs::path, fs::path>> collectFiles(const std::vector<std::string> &paths) const;

        // Performs a checkout of the given reference into a *EMPTY* new folder.
        // If the reference points to a commit, it retrieves the associated tree.
        void checkout(const std::string &ref, const fs::path &checkoutPath) const;

        // Creates a tag in the repository. `createTagObj` determines whether to create a tag object (annotated tag).
        void createTag(const std::string &name, const std::string &ref, bool createTagObj = false) const;

        // Removes files from the Git index and optionally deletes them from the filesystem.
        // ----
        // - `paths` contains the absolute and relative paths of files to remove.
        // - `delete_` controls whether the files are also removed from the working directory.
        // - `skipMissing` determines whether to ignore missing files instead of throwing an error.
        GitIndex rm(const std::vector<std::pair<fs::path, fs::path>>& paths, bool delete_ = true, bool skipMissing = false);

        // Adds files to the Git index.
        GitIndex add(const std::vector<std::pair<fs::path, fs::path>>& paths);

        // Creates a new commit with the given message.
        // Equivalent to `git commit --allow-empty -m <message>`
        // Writes the commit object to disk and updates HEAD or the active branch.
        void commit(const std::string &message);

        // Command equivalent to `git log --pretty=raw -n <count>`
        std::string getLog(const std::string &commit, long maxCount) const;

        // Command equivalent to `git ls-tree -r <tree>`
        std::string lsTree(const std::string &ref, bool recurse, const fs::path &prefix = "") const;

        // Displays all references (branches, tags, etc.) in the repository.
        // ----
        // - `start` specifies the starting path for reference lookup.
        // - `withHash` determines whether to include the resolved SHA1 hash.
        // - `prefix` allows customization of the displayed path format.
        std::string showAllRefs(const std::string &start, bool withHash, const std::string &prefix) const;

        // Command similar to  `git ls-files -v`
        std::string lsFiles(bool verbose = false) const;

        // Command - `git status`. Unlike the git version, always lists paths from root of the worktree
        std::string getStatus() const;
};
