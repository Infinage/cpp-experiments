#include "argparse.hpp"
#include <clocale>
#include <fstream>
#include <print>

int main(int argc, char **argv) {
    argparse::ArgumentParser parser {"CWC"};
    parser.description("Word count implementation in C++");
    parser.addArgument("bytes", argparse::NAMED).help("print the byte counts")
          .alias("c").defaultValue(false).implicitValue(true);
    parser.addArgument("lines", argparse::NAMED).help("print the newline counts")
          .alias("l").defaultValue(false).implicitValue(true);
    parser.addArgument("words", argparse::NAMED).help("print the word counts")
          .alias("w").defaultValue(false).implicitValue(true);
    parser.addArgument("chars", argparse::NAMED).help("print the character counts")
          .alias("m").defaultValue(false).implicitValue(true);
    parser.addArgument("file", argparse::POSITIONAL).defaultValue("-")
        .help("File to count on, if not provided, reads from stdin");
    parser.parseArgs(argc, argv);

    bool countChars {parser.get<bool>("chars")},
         countBytes {parser.get<bool>("bytes")}, 
         countWords {parser.get<bool>("words")},
         countLines {parser.get<bool>("lines")};

    if (!countWords && !countLines && !countBytes && !countChars)
        countWords = true, countLines = true, countBytes = true;
    
    std::string fileName {parser.get("file")};

    std::ifstream ifs {fileName, std::ios::binary | std::ios::in};
    std::istream &is {fileName == "-"? std::cin: ifs};

    long bytes = 0, chars = 0, lines = 0, words = 0;

    // conversion state for multibyte decoding
    mbstate_t state{}; if (countChars) std::setlocale(LC_ALL, "");

    if (countLines || countWords || countBytes || countChars) {
        char ch; bool prevSpace {true}; 
        while (is.get(ch)) {
            if (countBytes) ++bytes;

            if (countLines && ch == '\n') ++lines;

            if (countWords) {
                if (!std::isspace(ch) && prevSpace) {
                    ++words; prevSpace = false; 
                } else if (std::isspace(ch)) {
                    prevSpace = true;
                }
            }

            if (countChars) {
                // mbrtowc converts char to wchar; below func call passes a single char type at a time
                // sz => -2; wchar is incomplete, read next char into wc (state will take care of decoding)
                // sz => -1; invalid byte char encountered, reset and proceed
                wchar_t wc; std::size_t sz {std::mbrtowc(&wc, &ch, 1, &state)};
                if (sz == static_cast<std::size_t>(-2)) continue;
                else if (sz == static_cast<std::size_t>(-1)) state = {};
                else ++chars;
            }
        }
    }

    std::println("L: {} W: {} C: {} B: {} F: {}", lines, words, chars, bytes, fileName);
}
