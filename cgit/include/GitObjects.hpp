#pragma once

#include "../../misc/ordered_map.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace cr = std::chrono;

// Base class for GitBlob, GitCommit, GitTree
class GitObject {
    public:
        // Fmt is used to identify the type of git object - blob, commit, tree, tag
        const std::string sha, fmt;

    public:
        virtual ~GitObject() = default;
        GitObject(const std::string &sha, const std::string &fmt);
        virtual void deserialize(const std::string&) = 0;
        virtual std::string serialize() const = 0;
};

// Blob repr stores the file contents
class GitBlob: public GitObject {
    private:
        std::string data;

    public:
        GitBlob(const std::string &sha, const std::string &data);
        void deserialize(const std::string &data) override;
        [[nodiscard]] std::string serialize() const override;
};

// Storing Commit related information
class GitCommit: public GitObject {
    private:
        // Since multiple values can exist for a few keys such as parent, author
        // we set the value type as a vector (could use variant but this is simpler)
        stdx::ordered_map<std::string, std::vector<std::string>> data;
        cr::system_clock::time_point commitUTC;

    public:
        // This class will be inheritted by GitTag, so we store fmt as a variable instead of hardcoding
        GitCommit(const std::string &sha, const std::string &raw, const std::string &fmt = "commit");

        // Set a key-value pair into `data`
        void set(const std::string &key, std::vector<std::string> &&value);

        // Get a value from `data`, if not found return an empty vector
        std::vector<std::string> get(const std::string &key) const;

        // For Sorting Git Commit objects
        inline bool operator<(GitCommit &other) const { return this->commitUTC < other.commitUTC; }
        inline bool operator>(GitCommit &other) const { return this->commitUTC > other.commitUTC; }

        void deserialize(const std::string &raw) override;

        [[nodiscard]] std::string serialize() const override;
};

// This class abstracts a piece of the GitTree object
class GitLeaf {
    private:
        // Helper for creating comparators: ensures that when we have 
        // identically named folders & files say `example.c` and `example`, 
        // the folder comes before the file - `example/`
        static int pathCompare(const GitLeaf &l1, const GitLeaf &l2);

    public:
        std::string mode, path, sha;

        // Sometimes we set the sha as a binary str and other times sha 
        // is directly set as a hex string. Support both with `shaInBinary`
        GitLeaf(
            const std::string &mode, const std::string &path, 
            const std::string &sha, bool shaInBinary = true
        );

        // Copy constructor
        GitLeaf(const GitLeaf &other);

        // Move constructor
        GitLeaf(GitLeaf &&other); 

        // Move assignment
        GitLeaf& operator= (GitLeaf &&other) noexcept;

        [[nodiscard]] std::string serialize() const;

        // Comparators
        inline bool operator< (const GitLeaf &other) const { return pathCompare(*this, other)  < 0; }
        inline bool operator> (const GitLeaf &other) const { return pathCompare(*this, other)  > 0; }
        inline bool operator==(const GitLeaf &other) const { return pathCompare(*this, other) == 0; }
};

// This is analogous to folders, references other trees, blobs, etc
class GitTree: public GitObject {
    private:
        // Mutable since we sort it inside a const `serialize()`
        mutable std::vector<GitLeaf> data;

    public:
        GitTree(const std::string &sha, const std::string& raw);

        GitTree(const std::vector<GitLeaf> &data);

        // Iterators
        inline std::vector<GitLeaf>::const_iterator begin() const { return data.cbegin(); }
        inline std::vector<GitLeaf>::const_iterator end() const { return data.cend(); }

        void deserialize(const std::string &raw) override;

        // Simply sort the container and serialize the leaves
        [[nodiscard]] std::string serialize() const override;
};

// Same as GitCommit, only the fmt is different
class GitTag: public GitCommit {
    public:
        // Reuse parent functions
        using GitCommit::serialize, GitCommit::deserialize; 
        using GitCommit::get, GitCommit::set;

        GitTag(const std::string &sha, const std::string& raw);
};
