#include <algorithm>
#include <bitset>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <unordered_map>

class XXD {
    public:
        // Helper to convert hex to char
        static char reprHex(char hexChar[2]) {
            unsigned int ch;
            std::istringstream iss {hexChar};
            iss >> std::hex >> ch;
            return static_cast<char>(ch);
        };

        // Helper to convert char to hex or binary
        static std::string reprChar(unsigned char ch, bool binaryMode) {
            if (binaryMode) return (std::bitset<8>{static_cast<unsigned long>(ch)}).to_string();
            else {
                std::ostringstream oss;
                oss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(ch);
                return oss.str();
            }
        };

        // Reads file in text mode and writes the hex dump into binary
        static std::string hex2Binary (std::ifstream &ifs) {

            // Determine the hex mode
            std::string buffer;
            std::getline(ifs, buffer);

            // Stream output to stringstream
            std::ostringstream oss;

            // Only plain mode and big endian hex is supported
            bool plain {std::all_of(buffer.begin(), buffer.end(), [](const char ch) { return std::isspace(ch) || std::isalnum(ch); })};
            ifs.seekg(0);

            // Placeholder to temp hold the hex char
            char hexChar[2]; bool idx{false};

            if (!plain) {
                // Figure out the column length
                std::size_t hexEnd {buffer.find("  ")};
                while (std::getline(ifs, buffer) && buffer.size() >= 10) {
                    for (const char ch: buffer.substr(10, hexEnd - 10)) {
                        if (!std::isspace(ch)) {
                            hexChar[idx] = ch;
                            idx = !idx;
                            if (!idx) oss.put(reprHex(hexChar));
                        }
                    }
                }

            } else {
                while (ifs.get(hexChar[idx])) {
                    if (!std::isspace(hexChar[idx])) {
                        idx = !idx;
                        if (!idx) oss.put(reprHex(hexChar));
                    }
                }
            }
 
            return oss.str();
        }

        // Reads in a binary file and outputs the hexadecimal / binary dump
        static std::string binary2Hex (
                std::ifstream &ifs, bool binaryMode, bool littleEndian, bool plainMode,
                bool decimalOffset, int offset, int endPos, int group, int columns
            ) {

            // Storing the intermediates and processed results
            char ch;
            std::ostringstream oss;

            // Jump to offset before starting
            ifs.seekg(offset);

            while (ifs.peek() != std::char_traits<char>::eof()) {
                // Accumulate the dump simulataneously filling the text content
                std::ostringstream dump, text;
                int count {0};
                while (count < columns) {
                    // Read one 'group'
                    std::deque<std::string> acc;
                    while (
                            static_cast<int>(acc.size()) < group && count < columns
                            && (endPos == -1 || offset + count < endPos)
                            && ifs.peek() != std::char_traits<char>::eof()
                        ) {
                        // Read in as char, but convert to unsigned char
                        ifs.get(ch);
                        count++;
                        if (binaryMode || !littleEndian)
                            acc.push_back(reprChar(static_cast<unsigned char>(ch), binaryMode));
                        else
                            acc.push_front(reprChar(static_cast<unsigned char>(ch), binaryMode));

                        // Only write the text portion if not in plain mode
                        if (!plainMode) text << (std::isprint(ch)? ch: '.');
                    }

                    // If we had stopped prematurely, fill until whichever
                    // of the group length or the column width is reached first
                    while (static_cast<int>(acc.size()) < group && count < columns) {
                        if (binaryMode || !littleEndian)
                            acc.push_back(std::string(binaryMode? 8: 2, ' '));
                        else
                            acc.push_front(std::string(binaryMode? 8: 2, ' '));
                        count++;
                    }

                    while (!acc.empty()) {
                        dump << acc.front();
                        acc.pop_front();
                    }

                    dump << " ";
                }

                if (plainMode) oss << dump.str() << "\n";
                else {
                    oss << (decimalOffset? std::dec: std::hex) << std::setw(8) << std::setfill('0') << offset << ": ";
                    oss << dump.str() << (littleEndian? std::string(2, ' '): std::string(1, ' ')) << text.str() << "\n";
                }

                offset += columns;
                if (endPos != -1 && offset >= endPos)
                    break;
            }

            return oss.str();
        }
};

int main(int argc, char **argv) {
    if (argc == 1) {
        std::cout << "An imperfect clone of CLI utility XXD.\n"
                  << "Usage:\n\tcxxd [options] [infile]\n"
                  << "Options:\n\t"
                  << "-b      binary digit dump.\n\t"
                  << "-e      little-endian dump (incompatible with -p, -r).\n\t"
                  << "-d      show offset in decimal instead of hex.\n\t"
                  << "-p      output in plain hexdump style, overrides binary, little-endian & resets formatting.\n\t"
                  << "-c      format octets per line. Default 16 (-b:6, -p:30).\n\t"
                  << "-g      number of octets per group in normal output. Default 2 (-b:1, -e:4, -p:30).\n\t"
                  << "-l      stop after specified octets.\n\t"
                  << "-s      start at specified bytes (abs).\n\t"
                  << "-r      reverse: convert (or patch) hexdump into binary. Ignores all params except -s, -l, -op.\n\t"
                  << "-op     specify output file, writes to console if not specified.\n";
    } else {

        // Parse the parameters
        std::string outputFile {""};
        std::unordered_map<std::string, int> params;
        for (int i{1}; i < argc - 1;) {
            // Booleans
            if (std::strcmp(argv[i], "-e") == 0)
                params["little-endian"] = 1;
            else if (std::strcmp(argv[i], "-r") == 0)
                params["reverse"] = 1;
            else if (std::strcmp(argv[i], "-b") == 0)
                params["binary"] = 1;
            else if (std::strcmp(argv[i], "-p") == 0)
                params["plain"] = 1;
            else if (std::strcmp(argv[i], "-d") == 0)
                params["decimal-offset"] = 1;

            // Params with values
            else if (std::strcmp(argv[i], "-g") == 0)
                params["group"] = std::stoi(argv[++i]);
            else if (std::strcmp(argv[i], "-l") == 0)
                params["length"] = std::stoi(argv[++i]);
            else if (std::strcmp(argv[i], "-s") == 0)
                params["offset"] = std::stoi(argv[++i]);
            else if (std::strcmp(argv[i], "-c") == 0)
                params["columns"] = std::stoi(argv[++i]);
            else if (std::strcmp(argv[i], "-op") == 0)
                outputFile = std::string(argv[++i]);
            i++;
        }

        // Extract the parameters
        bool binaryMode {params.find("binary") != params.end()};
        bool littleEndian {params.find("little-endian") != params.end()};
        bool plainMode {params.find("plain") != params.end()};
        bool reverseDump {params.find("reverse") != params.end()};
        bool decimalOffset {params.find("decimal-offset") != params.end()};
        int offset {params.find("offset") == params.end()? 0: params["offset"]};
        int endPos {params.find("length") == params.end()? -1: offset + params["length"]};
        int group {params.find("group") == params.end()? binaryMode? 1: (littleEndian? 4: 2): params["group"]};
        int columns {params.find("columns") == params.end()? binaryMode? 6: 16: params["columns"]};

        // Placeholder to hold the output string
        std::string dump{""};
        std::ifstream ifs{argv[argc - 1], reverseDump? std::ios::in: std::ios::binary};

        // ********************* Convert the Binary to Hex *********************

        if (offset < 0 || group < 0 || columns < 0) {
            std::cerr << "cxxd: negative parameters are not supported: (-s=" << offset << ", -g=" << group << ", -c=" << columns << ").\n";
            std::exit(1);
        }

        else if (!ifs) {
            std::cerr << "cxxd: " << argv[argc - 1] << ": No such file or directory.\n";
            std::exit(1);
        }

        else if (reverseDump) { dump = XXD::hex2Binary(ifs); }

        else {
            // In plain all parameters except offset and length are reset
            if (plainMode) { binaryMode = false; littleEndian = false; group = 30; columns = 30; }

            // Check if group is a power of two little endian mode is set
            bool powerOfTwo {group > 0 && !(group & (group - 1))};
            if (littleEndian && !powerOfTwo) {
                std::cerr << "cxxd: number of octets per group must be a power of 2 with -e.\n";
                std::exit(1);
            }

            dump = XXD::binary2Hex(ifs, binaryMode, littleEndian, plainMode, decimalOffset, offset, endPos, group, columns);
        }

        // ********************* Write the dump to console or the output file *********************

        if (outputFile.empty())
            std::cout << dump;

        else  {
            // If we would require patching existing binary file in rev mode and the replaced patch is of a different size
            bool mismatchedPatch {std::filesystem::exists(outputFile) && reverseDump && endPos != -1 && static_cast<std::size_t>(endPos - offset) != dump.size()};
            if (mismatchedPatch) std::filesystem::copy_file(outputFile, ".tmp.cxxd");

            // Decide mode based on parameters
            std::ios::openmode mode {std::ios::out};
            if (reverseDump) {
                mode |= std::ios::binary;
                if (offset > 0 || endPos != -1) mode |= std::ios::in;
            }

            // Open the file and dump contents
            std::fstream fs {outputFile, mode};
            if (fs) {
                fs.seekp(reverseDump? offset: 0);
                fs << dump;

                // Handle remaining content if the patch size is mismatched
                // If end pos is unspecified we do not care about remaining portion
                // Just ensure that it is truncated
                if (mismatchedPatch) {
                    std::ifstream ifs {".tmp.cxxd", std::ios::binary};
                    ifs.seekg(endPos + 1);
                    char ch;
                    while (ifs.get(ch)) fs.put(ch);
                    std::filesystem::remove(".tmp.cxxd");
                }

                // Truncate / resize the file if patch applied is diff from original
                // Does not have any impact in hex dumps
                fs.flush();
                std::filesystem::resize_file(outputFile, static_cast<std::size_t>(fs.tellp()));
            }

            // If unable to open, throw error
            else {
                std::cerr << "cxxd: " << outputFile << ": error writing output file.\n";
                std::exit(1);
            }
        }
    }
    return 0;
}
