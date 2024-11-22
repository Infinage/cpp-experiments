#include <cstring>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

// TODO: Test out for -c values larger than 16

int main(int argc, char **argv) {
    if (argc == 1) {
        std::cout << "An imperfect clone of CLI utilitu XXD.\nUsage: xxd [infile]\n";
    } else {
        // Parse the parameters
        std::unordered_map<std::string, int> params;
        for (int i{1}; i < argc - 1;) {
            // Booleans
            if (std::strcmp(argv[i], "-e") == 0)
                params["little-endian"] = 1;
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
            i++;
        }

        // Read the file as binary
        std::ifstream ifs{argv[argc - 1], std::ios::binary};        
        if (!ifs) std::cout << "cxxd: " << argv[argc - 1] << ": No such file or directory";
        else {
            // Extract the parameters
            int offset {params.find("offset") == params.end()? 0: params["offset"]};
            int columns {params.find("columns") == params.end()? 16: params["columns"]};
            int group {params.find("group") == params.end()? 2: params["group"]};
            int length {params.find("length") == params.end()? -1: offset + params["length"]};
            //bool littleEndian {params.find("little-endian") == params.end()};

            // TODO: Disallow groups of non powers of 2 in little-endian mode

            // Storing the intermediates and processed results
            char ch;
            std::ostringstream oss;

            // Jump to offset before starting
            ifs.seekg(offset);

            while (ifs.peek() != std::char_traits<char>::eof()) {
                oss << std::hex << std::setw(8) << std::setfill('0') << offset << ": ";

                // Accumulate the dump simulataneously filling the text content
                std::ostringstream dump, text;
                int count {0};
                while (count++ < columns) {
                    if ((length == -1 || offset + count <= length) && ifs.get(ch)) {
                        dump << std::setw(2) << std::setfill('0') << std::hex
                             << static_cast<int>(ch)
                             << (count % group == 0 || count == columns? " ": "");
                        text << (std::isprint(static_cast<unsigned char>(ch))? ch: '.');
                    } else {
                        dump << "  " << (count % group == 0 || count == columns? " ": "");
                        text << ' ';
                    }
                }

                offset += columns;
                oss << dump.str() << " " << text.str() << "\n";
                if (length != -1 && offset >= length)
                    break;
            }
            std::cout << oss.str();
        }
    }
    return 0;
}
