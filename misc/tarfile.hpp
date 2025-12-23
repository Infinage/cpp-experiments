/*
 * TODO:
 * - ExtractAll to set the file perms and mtime
 */

#pragma once

#include <algorithm>
#include <charconv>
#include <chrono>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace tar {
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
            std::ranges::copy(src.substr(0, N), dest.data() + offset);
        };

        inline bool chunkCopy(std::fstream &source, std::fstream &destination, std::uint64_t size, std::uint64_t chunkSize) {
            std::vector<char> buffer(chunkSize);
            while (size > 0) {
                std::size_t toRead {size > chunkSize? chunkSize: size};
                source.read(buffer.data(), static_cast<std::streamsize>(toRead));
                std::streamsize bytesRead {source.gcount()};
                if (bytesRead <= 0) return false;
                destination.write(buffer.data(), bytesRead);
                if (!destination.good()) return false;
                size -= static_cast<std::uint64_t>(bytesRead);
            }
            return !size;
        }

        inline std::string rstrip(std::string_view str) {
            auto end = str.find('\0');   
            if (end != std::string::npos)
                str = str.substr(0, end);
            return {str.data(), str.size()};
        }
    }

    struct TarInfo {
        // Members
        std::string fname;
        std::uint32_t mode, uid, gid; 
        std::uint64_t blockOffset, size;
        std::chrono::system_clock::time_point mtime;
        enum class FileType: std::uint8_t { NORMAL=0, SYMLINK=2, DIRECTORY=5 } ftype;
        std::string linkName;
        bool ustar;
        std::string uname, gname;
        std::string fprefix;

        // Full path combing both (no impact if not a ustar format)
        std::string fullPath() const {
            std::string result; result.reserve(fprefix.size() + fname.size());
            result += impl::rstrip(fprefix); result += impl::rstrip(fname);
            return result;
        }

        // Construct entry for empty directory
        static TarInfo emptyDirectory(std::string_view fname) {
            if (fname.size() > 255) 
                throw std::runtime_error {"Tarfile Error: Filename length is "
                    "too long: " + std::string{fname}};

            TarInfo tarInfo;
            tarInfo.ustar = true;
            if (fname.size() <= 100) {
                tarInfo.fname = fname;
            } else if (fname.size() <= 255) {

            }

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
                auto ustarVer = header.substr(262, 3);
                if (!std::equal(ustarVer.begin(), ustarVer.end(), "  \0")) 
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
            std::ranges::copy(impl::writeOInt< 8>( mode), header.data() + 100);
            std::ranges::copy(impl::writeOInt< 8>(  uid), header.data() + 108);
            std::ranges::copy(impl::writeOInt< 8>(  gid), header.data() + 116);
            std::ranges::copy(impl::writeOInt<12>( size), header.data() + 124);

            auto mts = std::chrono::duration_cast<std::chrono::seconds>(mtime.time_since_epoch()).count();
            std::ranges::copy(impl::writeOInt<12>(static_cast<std::uint64_t>(mts)), header.data() + 124);

            // Calculate the checksum
            std::uint16_t unsignedSum {};
            for (std::size_t i{}; i < 512; ++i) {
                char ch = 148 <= i && i < 156? ' ': header.at(i); 
                unsignedSum += static_cast<unsigned char>(ch);
            }

            // Write the checksum + NULL + ' '
            std::ranges::copy(impl::writeOInt<6>(unsignedSum), header.data() + 148);
            header.at(155) = ' ';

            // Write the file type
            header.at(156) = ftype == FileType::NORMAL? '0': 
                ftype == FileType::SYMLINK? '2': '5';

            // Check and write ustar relevant fields
            if (ustar) {
                impl::writeStr<7>(header, "ustar  ", 257);
                impl::writeStr<32>(header, uname, 265);
                impl::writeStr<32>(header, gname, 297);
                impl::writeStr<155>(header, fprefix, 345);
            }

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
            void extract(const TarInfo &member, const std::filesystem::path &destDir = ".", 
                const std::size_t copyChunkSizeInBytes = 5 * 1024 * 1024)
            {
                // Ensure that file has been opened for READ ops
                assertFileMode(Mode::READ);

                // Construct the full path for file write
                auto destPath = destDir / member.fullPath(); 
                auto destBase = destPath.parent_path();
                if (!std::filesystem::exists(destBase))
                    std::filesystem::create_directories(destBase);

                // Error handling during file / folder writes
                if (!std::filesystem::is_directory(destBase))
                    throw std::runtime_error{"Tarfile Error: Write filepath base "
                        "is not a directory: " + destBase.string()};
                if (std::filesystem::exists(destPath))
                    throw std::runtime_error{"Tarfile Error: Write filepath "
                        "already exists: " + destPath.string()};

                if (member.ftype == TarInfo::FileType::NORMAL) [[likely]] {
                    std::fstream dest {destPath, std::ios::binary};
                    file.seekg(static_cast<std::streamoff>(member.blockOffset));
                    impl::chunkCopy(file, dest, member.size, copyChunkSizeInBytes);
                } 

                else if (member.ftype == TarInfo::FileType::DIRECTORY) {
                    std::filesystem::create_directory(destPath);
                }

                else if (member.ftype == TarInfo::FileType::SYMLINK) {
                    std::filesystem::create_symlink(impl::rstrip(member.linkName), destPath);
                }
            }

            // Extracts all members of a archive to the specified dest directory
            void extractAll(const std::filesystem::path &destDir = ".", 
                const std::size_t copyChunkSizeInBytes = 5 * 1024 * 1024) 
            {
                // Ensure that file has been opened for READ ops
                assertFileMode(Mode::READ);

                // Basic eror handling
                if (std::filesystem::exists(destDir) && !std::filesystem::is_directory(destDir)) 
                    throw std::runtime_error{"Tarfile Error: Write file path "
                        "is not a directory: " + destDir.string()};

                // Process and extract each member
                for (const auto &member: getMembers()) {
                    extract(member, destDir, copyChunkSizeInBytes);
                }
            }

            // Adds a file or directory from sourcePath to the archive.
            void add(const std::filesystem::path &sourcePath, std::string_view arcname = "", bool ignoreErrors = false) {
                // Ensure that file has been opened for WRITE ops
                assertFileMode(Mode::WRITE);

                if (!std::filesystem::exists(sourcePath) && !ignoreErrors)
                    throw std::runtime_error{"Tarfile Error: No such file or directory: " 
                        + sourcePath.string()};

                if (std::filesystem::is_regular_file(sourcePath)) {

                }

                else if (std::filesystem::is_directory(sourcePath)) {
                    if (std::filesystem::is_empty(sourcePath)) {

                    } 

                    else {
                        for (auto nextPath: std::filesystem::recursive_directory_iterator {sourcePath})
                            add(sourcePath, arcname, true);
                    }
                }

                else if (std::filesystem::is_symlink(sourcePath)) {

                }

                else throw std::runtime_error {"Tarfile Error: Unsupported file type: " 
                    + sourcePath.string()};
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
            [[nodiscard]] static bool isZeroBlock(std::string_view block) {
                return block.find_first_not_of('\0') == std::string::npos;
            }

            void assertFileMode(Mode mode) const {
                std::string modeStr = mode == Mode::READ? "READ": "WRITE";
                if (this->mode != mode) 
                    throw std::runtime_error{"Tarfile Error: File mode mismatch, requires " + 
                        modeStr + " access"};
            }
    };
}
