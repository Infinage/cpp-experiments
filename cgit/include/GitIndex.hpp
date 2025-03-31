#pragma once

#include <cstring>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Represents the Git Index .git/index
class GitIndex {
    public:
        struct GitTimeStamp { 
            unsigned int seconds, nanoseconds; 
            friend std::ostream &operator<<(std::ostream &os, const GitTimeStamp &ts);
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
        unsigned int getVersion() const;
        const std::vector<GitIndexEntry> &getEntries() const;
        std::vector<GitIndexEntry> &getEntries();

        // Default values to ensure default initialization
        GitIndex(const unsigned int version = 2, const std::vector<GitIndexEntry> &entries = {});

        // Parse from an index filepath and return GitIndex Object
        static GitIndex readFromFile(const fs::path &path);

        // Write the git index to disk
        void writeToFile(const fs::path &path) const;
};
