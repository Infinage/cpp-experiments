#include <fstream>

#include "../include/utils.hpp"
#include "../include/GitIndex.hpp"

std::ostream &operator<<(std::ostream &os, const GitIndex::GitTimeStamp &ts) {
    std::time_t t = ts.seconds;
    std::tm *tm_info = std::gmtime(&t);
    os << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S") << '.' 
       << std::setfill('0') << std::setw(9) << ts.nanoseconds;
    return os;
}

unsigned int GitIndex::getVersion() const { return version; }

const std::vector<GitIndex::GitIndexEntry> &GitIndex::getEntries() const { return entries; }
std::vector<GitIndex::GitIndexEntry> &GitIndex::getEntries() { return entries; }

GitIndex::GitIndex(const unsigned int version, const std::vector<GitIndexEntry> &entries):
    version(version), entries(entries) { }

GitIndex GitIndex::readFromFile(const fs::path &path) {
    if (!fs::exists(path)) return GitIndex{};

    std::ifstream ifs{path, std::ios::binary};
    char signature[5] {}; 
    unsigned int version, count;

    ifs.read(signature, 4);
    if (std::strcmp(signature, "DIRC") != 0) 
        throw std::runtime_error("Not a valid GitIndex file: " + path.string());
    
    readBigEndian(ifs, version); readBigEndian(ifs, count);
    if (version != 2) 
        throw std::runtime_error("CGit only supports Index file version 2: " + path.string());

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

        // Read the name, if `nameLength` is 0xFF git assumes name 
        // is more than 4095 chars & searches until we hit 0x00
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

void GitIndex::writeToFile(const fs::path &path) const {
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
