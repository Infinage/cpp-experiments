#pragma once

#include <algorithm>
#include <charconv>
#include <chrono>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iterator>

#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace tar {
    enum class FileType: std::uint8_t { NORMAL=0, SYMLINK=2, DIRECTORY=5 };

    struct FileStat {
        std::string fname;
        std::uint32_t mode, uid, gid; 
        std::uint64_t size;
        std::chrono::system_clock::time_point mtime;
        FileType ftype;
        std::string linkName;
        std::string uname, gname;
    };

    namespace impl {
        template<std::unsigned_integral T>
        T parseOInt(std::string_view str) {
            T res;
            auto [ptr, ec] = std::from_chars(str.begin(), str.end(), res, 8);
            if (ec != std::errc()) 
                throw std::runtime_error{"Tarfile Error: Invalid Int read, got: " 
                    + std::string{str}};
            return res;
        }

        template<std::size_t PAD, std::unsigned_integral T>
        std::string writeOInt(T val) { return std::format("{:0{}o}", val, PAD); }

        // Helper to truncate and write string at offset
        template<std::size_t N>
        void writeStr(std::string &dest, std::string_view src, std::size_t offset) {
            if (dest.size() < offset + N) [[unlikely]] 
                throw std::runtime_error{"TarFile Error: Attempt to write bytes larger than capacity"};
            std::ranges::copy(src.begin(), src.begin() + std::min(N, src.size()), 
                dest.data() + offset);
        };

        inline void chunkCopy(std::fstream &source, std::fstream &destination, 
            std::uint64_t size, std::uint64_t chunkSize = 1024 * 1024 * 5) 
        {
            std::vector<char> buffer(chunkSize);
            while (size > 0) {
                std::size_t toRead {size > chunkSize? chunkSize: size};
                source.read(buffer.data(), static_cast<std::streamsize>(toRead));
                std::streamsize bytesRead {source.gcount()};
                if (bytesRead <= 0) throw std::runtime_error{"TarFile Error: Chunked READ->write failed"};
                destination.write(buffer.data(), bytesRead);
                if (!destination.good()) throw std::runtime_error{"TarFile Error: Chunked read->WRITE failed"};
                size -= static_cast<std::uint64_t>(bytesRead);
            }
        }

        inline std::string rstrip(std::string_view str) {
            auto end = str.find('\0');   
            if (end != std::string::npos)
                str = str.substr(0, end);
            return {str.data(), str.size()};
        }

        inline std::pair<std::string, std::string> splitPathUSTAR(std::string_view path) {
            if (path.size() > 255) throw std::runtime_error{"Tarfile Error: "
                "Path exceeds USTAR limit: " + std::string(path)};

            // Fits entirely in name
            if (path.size() <= 100) return { "", std::string(path) };

            // Try splitting from the right on '/'
            // prefix = path[0..i-1]; name = path[i+1..end]
            for (std::size_t i = path.size(); i-- > 0; ) {
                if (path[i] != '/') continue;
                if (path.size() - i - 1 <= 100 && i <= 155) {
                    return {
                        std::string(path.substr(0, i)),
                        std::string(path.substr(i + 1))
                    };
                }
            }

            throw std::runtime_error{"Tarfile Error: No valid USTAR split for path: " 
                + std::string(path)};
        }

        inline FileStat stat(const std::filesystem::path &path) {
            struct ::stat st{}; 
            if (::lstat(path.c_str(), &st) != 0)
                throw std::runtime_error {"TarFile Error: Stat failed: " + path.string()};

            FileStat fst {};

            // File Permissions, User and Group IDs 
            fst.mode = static_cast<std::uint32_t>(st.st_mode & 0777);
            fst.uid  = static_cast<std::uint32_t>(st.st_uid);
            fst.gid  = static_cast<std::uint32_t>(st.st_gid);

            // Modified time (ignore tv_nsec)
            fst.mtime = std::chrono::system_clock::time_point {
                std::chrono::seconds{st.st_mtime}};

            // File type
            fst.size = 0;
            if (S_ISDIR(st.st_mode)) {
                fst.ftype = FileType::DIRECTORY;
            } else if (S_ISREG(st.st_mode)) {
                fst.ftype = FileType::NORMAL;
                fst.size = static_cast<std::uint64_t>(st.st_size);
            } else if (S_ISLNK(st.st_mode)) {
                fst.ftype = FileType::SYMLINK;
                std::vector<char> buf(static_cast<std::size_t>(st.st_size + 1));
                ssize_t len = ::readlink(path.c_str(), buf.data(), buf.size());
                if (len < 0) throw std::runtime_error{"Tarfile Error: readlink failed: " + path.string()};
                fst.linkName.assign(buf.data(), static_cast<std::size_t>(len));
            } else {
                throw std::runtime_error{"Tarfile Error: unsupported file type: " + path.string()};
            }

            // Username & Group name (best effort)
            if (auto *pw = ::getpwuid(st.st_uid)) fst.uname = pw->pw_name;
            if (auto *gr = ::getgrgid(st.st_gid)) fst.gname = gr->gr_name;

            return fst;
        }

        // Unfortunately, `fs::last_write_time` follows the symlink instead 
        // of setting mtime for the link itself so we use `utimensat`
        inline void setLastWriteTime(const std::filesystem::path &destPath, 
            const std::chrono::system_clock::time_point &mftime) 
        {
            namespace cr = std::chrono; 
            struct timespec times[2];
            auto seconds = cr::duration_cast<cr::seconds>(mftime.time_since_epoch()).count();
            times[0] = { .tv_sec = seconds, .tv_nsec = 0 }; // access time
            times[1] = { .tv_sec = seconds, .tv_nsec = 0 }; // modification time
            ::utimensat(AT_FDCWD, destPath.c_str(), times, AT_SYMLINK_NOFOLLOW);
        }
    }

    struct TarInfo {
        // Members
        std::string fname;
        std::uint32_t mode, uid, gid; 
        std::uint64_t blockOffset, size;
        std::chrono::system_clock::time_point mtime;
        FileType ftype;
        std::string linkName;
        bool ustar;
        std::string uname, gname;
        std::string fprefix;

        // Full path combing both (no impact if not a ustar format)
        std::string fullPath() const {
            std::string result; result.reserve(fprefix.size() + fname.size());
            if (!fprefix.empty()) result += impl::rstrip(fprefix) + "/"; 
            result += impl::rstrip(fname);
            return result;
        }

        // Construct entry for an input file, block offset is set to 0
        static TarInfo readFile(const std::filesystem::path &fpath) {
            auto fname = fpath.string(); 
            auto [fpath_left, fpath_right] = impl::splitPathUSTAR(fname);
            auto st = impl::stat(fpath);

            TarInfo tarInfo {
                .fname = fpath_right, .mode = st.mode, .uid = st.uid, .gid = st.gid, 
                .blockOffset = 0, .size = st.size, .mtime = st.mtime, .ftype = st.ftype, 
                .linkName = st.linkName, .ustar = true, .uname = st.uname, .gname = st.gname,
                .fprefix = fpath_left
            };

            return tarInfo; 
        }

        // Construct from string, header offset is used to store the beginning idx of file
        static TarInfo readHeader(std::string_view header, std::size_t blockOffset) {
            if (header.size() != 512) 
                throw std::runtime_error{"Tarfile Error: Invalid header size: " 
                        + std::to_string(header.size())};

            // Calculate the signed and unsigned checksum
            std::uint16_t unsignedSum {}; std::int32_t signedSum {};
            for (std::size_t i{}; i < 512; ++i) {
                char ch = 148 <= i && i < 156? ' ': header.at(i); 
                signedSum += static_cast<char>(ch);
                unsignedSum += static_cast<unsigned char>(ch);
            }

            // Validate the checksum
            auto checkSum = impl::parseOInt<std::uint32_t>(header.substr(148, 6));
            if (checkSum != unsignedSum && checkSum != static_cast<unsigned>(signedSum))
                throw std::runtime_error{"Tarfile Error: Checksum validation failed"};

            // Parse the headers
            TarInfo tarInfo;
            tarInfo.fname = impl::rstrip(header.substr(0, 100));
            tarInfo.mode  = impl::parseOInt<std::uint32_t>(header.substr(100,  8));
            tarInfo.uid   = impl::parseOInt<std::uint32_t>(header.substr(108,  8));
            tarInfo.gid   = impl::parseOInt<std::uint32_t>(header.substr(116,  8));
            tarInfo.size  = impl::parseOInt<std::uint64_t>(header.substr(124, 12));
            tarInfo.blockOffset = blockOffset;

            // Parse modified timestamp
            auto mts = impl::parseOInt<std::uint64_t>(header.substr(136, 12));
            tarInfo.mtime = std::chrono::system_clock::time_point{std::chrono::seconds{mts}};

            // Parse file type
            auto type = header.at(156);
            switch (type) {
                case '0': tarInfo.ftype = FileType::NORMAL; break;
                case '2': tarInfo.ftype = FileType::SYMLINK; break;
                case '5': tarInfo.ftype = FileType::DIRECTORY; break;
                default: throw std::runtime_error{"Tarfile Error: File type unsupported: " 
                         + std::string{1, type}};
            }

            // Parse the link name
            tarInfo.linkName = impl::rstrip(header.substr(157, 100));

            // Check for ustar format
            if (header.substr(257, 5) == "ustar") {
                tarInfo.ustar = true;
                auto ustarVer = header.substr(263, 2);
                if (!std::equal(ustarVer.begin(), ustarVer.end(), "00")) 
                    throw std::runtime_error("Tarfile Error: "
                    "Unknown ustar version: " + std::string{ustarVer});

                // Parse username and group names
                tarInfo.uname = impl::rstrip(header.substr(265, 32));
                tarInfo.gname = impl::rstrip(header.substr(297, 32));

                // Parse file name prefix
                tarInfo.fprefix = impl::rstrip(header.substr(345, 155));
            }

            return tarInfo;
        }

        // Write back to header
        std::string writeHeader() const {
            std::string header(512, '\0');
            impl::writeStr<100>(header, fname, 0);
            std::ranges::copy(impl::writeOInt< 7>(mode), header.data() + 100);
            std::ranges::copy(impl::writeOInt< 7>( uid), header.data() + 108);
            std::ranges::copy(impl::writeOInt< 7>( gid), header.data() + 116);
            std::ranges::copy(impl::writeOInt<11>(size), header.data() + 124);

            auto mts = std::chrono::duration_cast<std::chrono::seconds>(mtime.time_since_epoch()).count();
            std::ranges::copy(impl::writeOInt<11>(static_cast<std::uint64_t>(mts)), header.data() + 136);

            // Write the file type
            header.at(155) = ' ';
            header.at(156) = ftype == FileType::NORMAL? '0': ftype == FileType::SYMLINK? '2': '5';
            impl::writeStr<100>(header, linkName, 157);

            // Check and write ustar relevant fields
            if (ustar) {
                impl::writeStr<5>(header, "ustar", 257);
                impl::writeStr<2>(header, "00", 263);
                impl::writeStr<32>(header, uname, 265);
                impl::writeStr<32>(header, gname, 297);
                impl::writeStr<155>(header, fprefix, 345);
            }

            // Calculate the checksum at the end
            std::uint16_t unsignedSum {};
            for (std::size_t i{}; i < 512; ++i) {
                char ch = 148 <= i && i < 156? ' ': header.at(i); 
                unsignedSum += static_cast<unsigned char>(ch);
            }

            // Write the checksum + NULL + ' '
            std::ranges::copy(impl::writeOInt<6>(unsignedSum), header.data() + 148);

            return header;
        }
    };

    class TarFile {
        public:
            enum class Mode { READ, WRITE };

            // Warning: Write mode overwrites the file contents
            TarFile(std::string_view path, Mode mode = Mode::READ): 
                filePath{path}, mode{mode} 
            {
                // Determine fstream flags based on access mode
                std::ios::openmode modeFlag = std::ios::binary;
                if (mode == Mode::READ) modeFlag |= std::ios::in;
                else modeFlag |= std::ios::out | std::ios::trunc;

                // Open file & throw on failure
                file = std::fstream {filePath, modeFlag};
                if (!file.is_open()) throw std::runtime_error{"Tarfile Error: "
                    "file cannot be read: " + filePath.string()};
            }

            // Disable copy semantics
            TarFile(const TarFile&) = delete;
            TarFile &operator=(const TarFile&) = delete;

            // Allow move semantics
            TarFile(TarFile&&) = default;
            TarFile &operator=(TarFile&&) = default;

            [[nodiscard]] std::vector<TarInfo> getMembers() {
                // Ensure that file has been opened for READ ops
                assertFileMode(Mode::READ);

                // Read in chunks
                char buffer[512];
                std::vector<TarInfo> members; 
                while (file && file.read(buffer, 512)) {
                    if (isZeroBlock({buffer, 512})) {
                        file.read(buffer, 512);
                        if (!file || !isZeroBlock({buffer, 512}))
                            throw std::runtime_error{"Tarfile Error: Invalid end-of-archive"};
                        break;
                    }

                    // We use offsets to easily detected block start pos
                    auto offset = static_cast<std::size_t>(file.tellg());
                    members.push_back(TarInfo::readHeader({buffer, 512}, offset));

                    // Read the entry size and skip as much bytes as required
                    auto fSize = members.back().size;
                    if (fSize % 512) fSize += 512 - (fSize % 512);
                    file.seekg(static_cast<std::streamoff>(fSize), std::ios::cur);
                }

                if (!file) {
                    throw std::runtime_error{"Tarfile Error: File is corrupt: " 
                        + filePath.string()};
                }

                return members;
            }

            // Extracts a given tarfile member to specified dest directory
            void extract(const TarInfo &member, const std::filesystem::path &destDir = ".") {
                // Ensure that file has been opened for READ ops
                assertFileMode(Mode::READ);

                // Construct the full path and path's parent for file write
                auto [destBase, destPath] = splitPathForDestination(member.fullPath(), destDir);

                if (!std::filesystem::exists(destBase))
                    std::filesystem::create_directories(destBase);

                // Error handling during file / folder writes
                if (!std::filesystem::is_directory(destBase))
                    throw std::runtime_error{"Tarfile Error: Write filepath base "
                        "is not a directory: " + destBase.string()};
                if (std::filesystem::exists(destPath))
                    throw std::runtime_error{"Tarfile Error: Write filepath "
                        "already exists: " + destPath.string()};

                if (member.ftype == FileType::NORMAL) [[likely]] {
                    std::fstream dest {destPath, std::ios::binary | std::ios::out};
                    file.seekg(static_cast<std::streamoff>(member.blockOffset));
                    impl::chunkCopy(file, dest, member.size);
                } 

                else if (member.ftype == FileType::DIRECTORY) {
                    std::filesystem::create_directory(destPath);
                }

                else if (member.ftype == FileType::SYMLINK) {
                    std::filesystem::create_symlink(impl::rstrip(member.linkName), destPath);
                }

                // Set file last modified time and file perms
                impl::setLastWriteTime(destPath, member.mtime);
                ::lchmod(destPath.c_str(), member.mode);
            }

            // Extracts all members of a archive to the specified dest directory
            void extractAll(const std::filesystem::path &destDir = ".") {
                // Ensure that file has been opened for READ ops
                assertFileMode(Mode::READ);

                // Basic eror handling
                if (std::filesystem::exists(destDir) && !std::filesystem::is_directory(destDir)) 
                    throw std::runtime_error{"Tarfile Error: Write file path "
                        "is not a directory: " + destDir.string()};

                // Process and extract each member
                std::vector<TarInfo> members {getMembers()};
                for (const auto &member: members) {
                    extract(member, destDir);
                }

                // Set the modified time again for directories since we end up 
                // modifying the mtime when we write files again
                for (const auto &member: members) {
                    if (member.ftype == FileType::DIRECTORY) {
                        auto [_, destPath] = splitPathForDestination(member.fullPath(), destDir);
                        impl::setLastWriteTime(destPath, member.mtime);
                    }
                }
            }

            // Adds a file or directory from sourcePath to the archive.
            void add(const std::filesystem::path &sourcePath, std::string_view arcname = "", 
                    bool ignoreErrors = false) 
            {
                // Ensure that file has been opened for WRITE ops
                assertFileMode(Mode::WRITE);

                auto fStatus = std::filesystem::symlink_status(sourcePath);
                if (fStatus.type() == std::filesystem::file_type::not_found && !ignoreErrors)
                    throw std::runtime_error{"Tarfile Error: No such file or directory: " 
                        + sourcePath.string()};

                // Split the arcname such that it fits inside 255 char limit
                auto [aleft, aright] = impl::splitPathUSTAR(arcname);

                // Regardless of what we are dealing with, write entry to archive
                auto header = TarInfo::readFile(sourcePath);
                header.blockOffset = static_cast<std::uint64_t>(file.tellp()) + 512;
                if (!arcname.empty()) { header.fprefix = aleft, header.fname = aright; }
                if (header.ftype == FileType::DIRECTORY && header.fname.back() != '/')
                    header.fname.push_back('/');
                auto headerStr = header.writeHeader();
                file.write(headerStr.c_str(), 512);

                // If entry is a file, write its content as well
                if (header.ftype == FileType::NORMAL) {
                    std::fstream ifs {sourcePath, std::ios::binary | std::ios::in};
                    impl::chunkCopy(ifs, file, header.size);
                    auto padBytes = header.size % 512? 512 - (header.size % 512): 0;
                    std::fill_n(std::ostream_iterator<char>(file), padBytes, '\0');
                }

                // If entry is a directory and it has additional files under, recurse
                else if (header.ftype == FileType::DIRECTORY && !std::filesystem::is_empty(sourcePath)) {
                    std::vector<std::pair<std::filesystem::path, std::string>> nextFiles;
                    std::filesystem::path arcPath{arcname};
                    for (auto nextPath: std::filesystem::directory_iterator {sourcePath}) {
                        auto nextArcPath = arcname.empty()? "": 
                            (arcPath / nextPath.path().filename()).string();
                        nextFiles.push_back({nextPath, nextArcPath});
                    }

                    // Sort lexiographically before adding to the archive
                    std::ranges::sort(nextFiles, [](auto &pr1, auto &pr2) { 
                            return pr1.first < pr2.first; });

                    // Add the sorted files
                    for (auto &[nextPath, nextArcPath]: nextFiles) {
                        add(nextPath, nextArcPath, true);
                    }
                }

            }

            ~TarFile() {
                // If file was opened in write mode, adds the final zero blocks
                if (mode == Mode::WRITE) {
                    std::fill_n(std::ostream_iterator<char>(file), 1024, '\0');
                }
            }

        private:
            std::filesystem::path filePath;
            std::fstream file; Mode mode;

        private:
            static bool isZeroBlock(std::string_view block) {
                return block.find_first_not_of('\0') == std::string::npos;
            }

            void assertFileMode(Mode mode) const {
                std::string modeStr = mode == Mode::READ? "READ": "WRITE";
                if (this->mode != mode) 
                    throw std::runtime_error{"Tarfile Error: File mode mismatch, requires " + 
                        modeStr + " access"};
            }

            static std::pair<std::filesystem::path, std::filesystem::path> 
            splitPathForDestination(std::string destPathStr, 
                    const std::filesystem::path &destDir) 
            {
                if (destPathStr.back() == '/') destPathStr.pop_back();
                std::filesystem::path destPath = destDir / destPathStr, 
                    destBase = destPath.parent_path();
                return {destBase, destPath};
            }
    };

    // Helper alias
    using FMode = TarFile::Mode;
}
