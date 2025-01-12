#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <system_error>

const std::string CUNIQ_VERSION = "1.0.0";
const std::string versionMessage {
    "cuniq (CPP Experiments) " + CUNIQ_VERSION + "\n"
    "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n\n"
    "Written by Naresh Jagadeesan.\n"
};
const std::string helpMessage {
    "Usage: cuniq [OPTION]... [INPUT [OUTPUT]]\n"
    "Filter adjacent matching lines from INPUT (or standard input),\n"
    "writing to OUTPUT (or standard output).\n\n"
    "With no options, matching lines are merged to the first occurrence.\n\n"
    "Mandatory arguments to long options are mandatory for short options too.\n"
    "  -c, --count           prefix lines by the number of occurrences\n"
    "  -d, --repeated        only print duplicate lines, one for each group\n"
    "  -D                    print all duplicate lines\n"
    "      --all-repeated[=METHOD]  like -D, but allow separating groups\n"
    "                                 with an empty line;\n"
    "                                 METHOD={none(default),prepend,separate}\n"
    "  -f, --skip-fields=N   avoid comparing the first N fields\n"
    "      --group[=METHOD]  show all items, separating groups with an empty line;\n"
    "                          METHOD={separate(default),prepend,append,both}\n"
    "  -i, --ignore-case     ignore differences in case when comparing\n"
    "  -s, --skip-chars=N    avoid comparing the first N characters\n"
    "  -u, --unique          only print unique lines\n"
    "  -z, --zero-terminated     line delimiter is NUL, not newline\n"
    "  -w, --check-chars=N   compare no more than N characters in lines\n"
    "      --help        display this help and exit\n"
    "      --version     output version information and exit\n\n"
    "A field is a run of blanks (usually spaces and/or TABs), then non-blank\n"
    "characters.  Fields are skipped before chars.\n"
};

int main(int argc, char **argv) {

    std::string ifname, ofname;
    bool countFlag {false}, repeatFlag {false}; 
    bool uniqueFlag {false}, ignoreCase {false};
    int compareChars {-1}, skipChars {0};

    int i {1};
    while (i < argc) {
        std::string arg {argv[i]};
        if (arg == "-c" || arg == "--count") {
            countFlag = true;
        } else if (arg == "-d" || arg == "--repeated") {
            repeatFlag = true;
        } else if (arg == "-h" || arg.starts_with("--h")) {
            std::cout << helpMessage;
            std::exit(0);
        } else if (arg == "-v" || arg == "--version") {
            std::cout << versionMessage;
            std::exit(0);
        } else if (arg == "-u" || arg == "--unique") {
            uniqueFlag = true;
        } else if (arg == "-i" || arg == "--ignore-case") {
            ignoreCase = true;
        } else if (arg == "-w" || arg.starts_with("--check-chars")) {
            std::string compareCharsStr;

            if (arg == "-w" || arg == "--check-chars") {
                compareCharsStr = argv[++i];
            } else if (arg.size() >= 14 && arg.at(13) == '=') {
                compareCharsStr = arg.substr(14);
            } else {
                std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
                std::exit(1);
            }

            // Check if compareCharsStr is valid
            std::from_chars_result validInt {std::from_chars(compareCharsStr.data(), compareCharsStr.data() + compareCharsStr.size(), compareChars)};
            if(validInt.ec != std::errc{} || validInt.ptr != compareCharsStr.data() + compareCharsStr.size()) {
                std::cerr << "cuniq: " << compareCharsStr << " invalid number of bytes to compare\n";
                std::exit(1);
            }
        } else if (arg == "-s" || arg.starts_with("--skip-chars")) {
            std::string skipCharsStr;

            if (arg == "-s" || arg == "--skip-chars") {
                skipCharsStr = argv[++i];
            } else if (arg.size() >= 13 && arg.at(12) == '=') {
                skipCharsStr = arg.substr(13);
            } else {
                std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
                std::exit(1);
            }

            // Check if compareCharsStr is valid
            std::from_chars_result validInt {std::from_chars(skipCharsStr.data(), skipCharsStr.data() + skipCharsStr.size(), skipChars)};
            if(validInt.ec != std::errc{} || validInt.ptr != skipCharsStr.data() + skipCharsStr.size()) {
                std::cerr << "cuniq: " << skipCharsStr << " invalid number of bytes to compare\n";
                std::exit(1);
            }
        } else if (arg.at(0) == '-' && arg.size() > 1) {
            std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
            std::exit(1);
        } else if (ifname.empty()) {
            ifname = arg;
        } else {
            ofname = arg;
        }
        i++;
    }

    // Create streams for buffering input / outputs
    std::istream *inp {ifname.empty() || ifname == "-"? &std::cin: new std::ifstream{ifname}};
    std::ostream *op {ofname.empty() || ofname == "-"? &std::cout: new std::ofstream{ofname}};

    if (inp->fail()) {
        std::cerr << "cuniq: test: No such file or directory\n";
        std::exit(1);
    }

    // Helper function to compare two string based (optionally ignore case)
    std::function<bool(const std::string&, const std::string&)> stringMatch {
        [&skipChars, &compareChars, &ignoreCase](const std::string &str1, const std::string &str2) {
            int str1Size {static_cast<int>(str1.size()) - skipChars}, str2Size {static_cast<int>(str2.size()) - skipChars};
            str1Size = std::max(0, str1Size); str2Size = std::max(0, str2Size);
            if (compareChars == -1 && str1Size != str2Size) 
                return false;
            else if (compareChars != -1 && (std::min(str1Size, compareChars) != std::min(str2Size, compareChars))) 
                return false;
            else {
                int boundedCompareChars {compareChars == -1? str1Size: std::min(str1Size, compareChars)};
                std::string::const_iterator beg1 {str1.cbegin() + skipChars}, beg2 {str2.cbegin() + skipChars};
                return std::equal(beg1, beg1 + boundedCompareChars, beg2, [&ignoreCase](const char ch1, const char ch2) { 
                    return ignoreCase? std::tolower(ch1) == std::tolower(ch2): ch1 == ch2; 
                });
            }
        }
    };

    // Helper function to print based on condition
    std::function<void(std::string&, std::size_t)> print {
        [op, &repeatFlag, &uniqueFlag, &countFlag](std::string &line, std::size_t count){
            if (countFlag && (!repeatFlag || count > 1) && (!uniqueFlag || count == 1))
                *op << std::string(6, ' ') << count << ' ';
            if ((!repeatFlag || count > 1) && (!uniqueFlag || count == 1))
                *op << line << "\n";
        }
    };

    // Main logic to print
    std::size_t count {0};
    std::string line, prevLine;
    while (std::getline(*inp, line)) {
        if (count == 0) {
            prevLine = line;
            count = 1;
        } else if (!stringMatch(line, prevLine)) {
            print(prevLine, count);
            prevLine = line;
            count = 1;
        } else { 
            count++; 
        }
    }

    // Print the left over line
    print(prevLine, count);

    // Clean up
    if (inp != &std::cin) delete inp;
    if (op != &std::cout) delete op;
}
