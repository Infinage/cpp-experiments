#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <istream>
#include <ostream>
#include <sstream>
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
    "  -c, --count             prefix lines by the number of occurrences\n"
    "  -d, --repeated          only print duplicate lines, one for each group\n"
    "  -D  --all-repeated      print all duplicate lines\n"
    "  -f, --skip-fields=N     avoid comparing the first N fields\n"
    "  -h, --help              display this help and exit\n"
    "  -i, --ignore-case       ignore differences in case when comparing\n"
    "  -s, --skip-chars=N      avoid comparing the first N characters\n"
    "  -u, --unique            only print unique lines\n"
    "  -v, --version           output version information and exit\n"
    "  -w, --check-chars=N     compare no more than N characters in lines\n"
    "  -z, --zero-terminated   line delimiter is NUL, not newline\n\n"
    "A field is a run of blanks (usually spaces and/or TABs), then non-blank\n"
    "characters.  Fields are skipped before chars.\n"
};

int main(int argc, char **argv) {

    std::string ifname, ofname;
    bool countFlag {false}, repeatFlag {false}; 
    bool uniqueFlag {false}, ignoreCase {false};
    bool allRepeated {false}, zeroTerminated {false};
    int compareChars {-1}, skipChars {0}, skipFields {0};

    // Helper to parse an int from a string - throw err on fail
    std::function<void(std::string&, int&)> parseInt {[](std::string &charStr, int &result) {
        std::from_chars_result validInt {std::from_chars(charStr.data(), charStr.data() + charStr.size(), result)};
        if(validInt.ec != std::errc{} || validInt.ptr != charStr.data() + charStr.size()) {
            std::cerr << "cuniq: " << charStr << " invalid number of bytes to compare\n";
            std::exit(1);
        }
    }};

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
        } else if (arg == "-z" || arg == "--zero-terminated") {
            zeroTerminated = true;
        } else if (arg == "-i" || arg == "--ignore-case") {
            ignoreCase = true;
        } else if (arg == "-D" || arg == "--all-repeated") {
            allRepeated = true;
        } else if (arg == "-w" || arg.starts_with("--check-chars")) {
            std::string compareCharsStr;
            if (arg == "-w" || arg == "--check-chars") {
                if (i + 1 < argc) compareCharsStr = argv[++i];
                else { std::cerr << "cuniq: option requires an argument -- 'w'\n"; std::exit(1); }
            } else if (arg.size() >= 14 && arg.at(13) == '=') {
                compareCharsStr = arg.substr(14);
            } else {
                std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
                std::exit(1);
            }

            // Check if compareCharsStr is valid and store it into compareChars
            parseInt(compareCharsStr, compareChars);

        } else if (arg == "-s" || arg.starts_with("--skip-chars")) {
            std::string skipCharsStr;
            if (arg == "-s" || arg == "--skip-chars") {
                if (i + 1 < argc) skipCharsStr = argv[++i];
                else { std::cerr << "cuniq: option requires an argument -- 's'\n"; std::exit(1); }
            } else if (arg.size() >= 13 && arg.at(12) == '=') {
                skipCharsStr = arg.substr(13);
            } else {
                std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
                std::exit(1);
            }

            // Check if skipCharsStr is valid and store it into skipChars
            parseInt(skipCharsStr, skipChars);

        } else if (arg == "-f" || arg.starts_with("--skip-fields")) {
            std::string skipFieldsStr;
            if (arg == "-f" || arg == "--skip-fields") {
                if (i + 1 < argc) skipFieldsStr = argv[++i];
                else { std::cerr << "cuniq: option requires an argument -- 'f'\n"; std::exit(1); }
            } else if (arg.size() >= 14 && arg.at(13) == '=') {
                skipFieldsStr = arg.substr(14);
            } else {
                std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
                std::exit(1);
            }

            // Check if skipFieldsStr is valid and store it into skipFields
            parseInt(skipFieldsStr, skipFields);

        } else if (arg.at(0) == '-' && arg.size() > 1) {
            std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
            std::exit(1);
        } 
        else if (ifname.empty()) { ifname = arg; } 
        else { ofname = arg; }

        i++;
    }

    // repeatFlag & countFlag are incompatible
    if (allRepeated && countFlag) {
        std::cerr << "cuniq: printing all duplicated lines and repeat counts is meaningless.\n"
                  << "Try 'cuniq --help' for more information.";
        std::exit(1);
    }

    // Create streams for buffering input / outputs
    std::istream *inp {ifname.empty() || ifname == "-"? &std::cin: new std::ifstream{ifname}};
    std::ostream *op {ofname.empty() || ofname == "-"? &std::cout: new std::ofstream{ofname}};

    if (inp->fail()) {
        std::cerr << "cuniq: test: No such file or directory\n";
        std::exit(1);
    }

    std::function<std::string(const std::string&)> tokenizeAndSkipWhitespace {[&skipFields, &zeroTerminated](const std::string &str){
        std::istringstream iss{str};
        std::string line, token;
        for (int i {0}; i < skipFields && iss >> line; i++);
        std::getline(iss, token, zeroTerminated? '\0': '\n');
        return token;
    }};

    // Helper function to compare two string based (optionally ignore case)
    std::function<bool(const std::string&, const std::string&)> stringMatch {
        [&skipChars, &compareChars, &ignoreCase, tokenizeAndSkipWhitespace]
            (const std::string &str1_, const std::string &str2_) {
                // Skip fields if set
                std::string str1 {tokenizeAndSkipWhitespace(str1_)}, str2 {tokenizeAndSkipWhitespace(str2_)};

                // Proceed as usual
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
        [op, &repeatFlag, &allRepeated, &uniqueFlag, &countFlag, &zeroTerminated] 
            (std::string &line, std::size_t count) {
                if (allRepeated) {
                    if (count > 1) {
                        for (std::size_t i {0}; i < count; i++)
                            *op << line << (zeroTerminated? '\0': '\n');
                    }
                } else {
                    if (countFlag && (!repeatFlag || count > 1) && (!uniqueFlag || count == 1))
                        *op << std::string(6, ' ') << count << ' ';

                    if ((!repeatFlag || count > 1) && (!uniqueFlag || count == 1))
                        *op << line << (zeroTerminated? '\0': '\n');
                }
        }
    };

    // Main logic to print
    std::size_t count {0};
    std::string line, prevLine;
    while (std::getline(*inp, line, zeroTerminated? '\0': '\n')) {
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
