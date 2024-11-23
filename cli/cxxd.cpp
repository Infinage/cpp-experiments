#include <algorithm>
#include <bitset>
#include <cctype>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

/*
 * TODO:
 * Revert operation
    - Ensure that dump is hexadecimal and big endian
    - XXD Auto infers the groups, columns based on the provided input dump
    - Whats more, it only writes to the binary file at the specified offsets in the
      dump if the binary file writing to already exists
    - Ensure that if the patch is larger than original, it is handled without errors

 * Double check invalid inputs: "-l 0.5 -g 9.1"
 */

class XXD {
    public:
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
            if (!plain) {
                std::cerr << "cxxd: non plain mode is not supported at the moment.\n";
                std::exit(1);
            } else {
                char hexChar[2];
                bool idx{false};
                while (ifs.get(hexChar[idx])) {
                    if (!std::isspace(hexChar[idx])) {
                        idx = !idx;
                        if (!idx) {
                            unsigned int ch;
                            std::istringstream iss{std::string{hexChar}};
                            iss >> std::hex >> ch;
                            oss.put(static_cast<char>(ch));
                        }
                    }
                }
            }
 
            return oss.str();
        }

        // Reads in a binary file and outputs the hexadecimal / binary dump
        static std::string binary2Hex (
                std::ifstream &ifs, bool binaryMode, bool littleEndian,
                bool plainMode, int offset, int length, int group, int columns
            ) {

            // Helper to convert char to hex or binary
            std::function<std::string(unsigned char)> repr{[&binaryMode](unsigned char ch) {
                if (binaryMode) return (std::bitset<8>{static_cast<unsigned long>(ch)}).to_string();
                else {
                    std::ostringstream oss;
                    oss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(ch);
                    return oss.str();
                }
            }};

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
                            && (length == -1 || offset + count < length)
                            && ifs.peek() != std::char_traits<char>::eof()
                        ) {
                        // Read in as char, but convert to unsigned char
                        ifs.get(ch);
                        count++;
                        if (binaryMode || !littleEndian)
                            acc.push_back(repr(static_cast<unsigned char>(ch)));
                        else
                            acc.push_front(repr(static_cast<unsigned char>(ch)));

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
                    oss << std::hex << std::setw(8) << std::setfill('0') << offset << ": ";
                    oss << dump.str() << (littleEndian? std::string(2, ' '): std::string(1, ' ')) << text.str() << "\n";
                }

                offset += columns;
                if (length != -1 && offset >= length)
                    break;
            }

            return oss.str();
        }
};

int main(int argc, char **argv) {
    if (argc == 1) {
        std::cout << "An imperfect clone of CLI utilitu XXD.\nUsage: xxd [infile]\n";
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
            else if (std::strcmp(argv[i], "-r") == 0)
                params["reverse"] = 1;

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
        int offset {params.find("offset") == params.end()? 0: params["offset"]};
        int length {params.find("length") == params.end()? -1: offset + params["length"]};
        int group {params.find("group") == params.end()? binaryMode? 1: (littleEndian? 4: 2): params["group"]};
        int columns {params.find("columns") == params.end()? binaryMode? 6: 16: params["columns"]};

        // Placeholder to hold the output string
        std::string dump{""};

        // Read the file as binary
        std::ifstream ifs{argv[argc - 1], (reverseDump? std::ios::in: std::ios::binary)};        
        if (!ifs) {
            std::cerr << "cxxd: " << argv[argc - 1] << ": No such file or directory";
            std::exit(1);
        }

        else if (reverseDump) {
            dump = XXD::hex2Binary(ifs);
        }

        else {
            // In plain all parameters except offset and length are reset
            if (plainMode) { binaryMode = false; littleEndian = false; group = 30; columns = 30; }

            // Check if group is a power of two little endian mode is set
            bool powerOfTwo {group > 0 && !(group & (group - 1))};
            if (littleEndian && !powerOfTwo) {
                std::cerr << "cxxd: number of octets per group must be a power of 2 with -e.\n";
                std::exit(1);
            }

            dump = XXD::binary2Hex(ifs, binaryMode, littleEndian, plainMode, offset, length, group, columns);
        }

        // Read and write the XXD dump per parameters passed
        if (outputFile.empty())
            std::cout << dump;

        else  {
            std::ofstream ofs {outputFile};
            if (ofs)
                ofs << dump;

            else {
                std::cerr << "cxxd: " << outputFile << ": error writing output file.\n";
                std::exit(1);
            }
        }
    }
    return 0;
}
