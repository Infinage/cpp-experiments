#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <istream>
#include <memory>
#include <ostream>
#include <queue>
#include <sstream>
#include <string>
#include <system_error>

// Helper to parse an int from a string - throw err on fail
void parseInt(std::string& charStr, int& result, const std::string &msgOnFail) {
    std::from_chars_result validInt {std::from_chars(charStr.data(), charStr.data() + charStr.size(), result)};
    if(validInt.ec != std::errc{} || validInt.ptr != charStr.data() + charStr.size()) {
        std::cerr << msgOnFail << "\n";
        std::exit(1);
    }
}

class Uniq {
    private:
        // Config variables
        mutable bool countFlag, repeatFlag, uniqueFlag;
        mutable bool ignoreCase, allRepeated, zeroTerminated;
        mutable int compareCharsCnt, skipCharsCnt, skipFieldsCnt;
        mutable std::shared_ptr<std::istream> ip;
        mutable std::shared_ptr<std::ostream> op;

        // Helper to tokenize and split strings based on whitespace
        std::istringstream tokenizer;
        std::string tokenizeAndSkipWhitespace(const std::string &str){
            tokenizer.clear();
            tokenizer.str(str);
            std::string line, token;
            for (int i {0}; i < skipFieldsCnt && tokenizer >> line; i++);
            std::getline(tokenizer, token, zeroTerminated? '\0': '\n');
            return token;
        };

        // Compare two char - case sensitivity changes based on config
        bool cmpChars(const char ch1, const char ch2) {
            return ignoreCase? std::tolower(ch1) == std::tolower(ch2): ch1 == ch2; 
        }

        // Helper function to compare two string based (optionally ignore case)
        bool stringMatch (const std::string &str1_, const std::string &str2_) {
            // Skip fields if set
            std::string str1 {tokenizeAndSkipWhitespace(str1_)}, str2 {tokenizeAndSkipWhitespace(str2_)};

            // Proceed as usual
            int str1Size {static_cast<int>(str1.size()) - skipCharsCnt}, str2Size {static_cast<int>(str2.size()) - skipCharsCnt};
            str1Size = std::max(0, str1Size); str2Size = std::max(0, str2Size);
            if (compareCharsCnt == -1 && str1Size != str2Size) 
                return false;
            else if (compareCharsCnt != -1 && (std::min(str1Size, compareCharsCnt) != std::min(str2Size, compareCharsCnt))) 
                return false;
            else {
                int boundedCompareChars {compareCharsCnt == -1? str1Size: std::min(str1Size, compareCharsCnt)};
                std::string::const_iterator beg1 {str1.cbegin() + skipCharsCnt}, beg2 {str2.cbegin() + skipCharsCnt};
                return std::equal(beg1, beg1 + boundedCompareChars, beg2, [this](const char ch1, const char ch2) { return cmpChars(ch1, ch2); });
            }
        }

        // Helper function to print based on condition
        void printAndClear(std::queue<std::string> &lines) {
            std::size_t linesCount {lines.size()};
            if (allRepeated) {
                if (linesCount > 1) {
                    for (std::size_t i {0}; i < linesCount; i++) {
                        std::string &line {lines.front()};
                        *op << line << (zeroTerminated? '\0': '\n');
                        lines.pop();
                    }
                }
            } else {
                if (countFlag && (!repeatFlag || linesCount > 1) && (!uniqueFlag || linesCount == 1))
                    *op << std::string(6, ' ') << linesCount << ' ';

                if ((!repeatFlag || linesCount > 1) && (!uniqueFlag || linesCount == 1)) {
                    *op << lines.front() << (zeroTerminated? '\0': '\n');
                }

            }

            // Clear the queue
            lines = {};
        }

    public:
        // Static constants
        static const std::string CUNIQ_VERSION;
        static const std::string VERSION_MESSAGE;
        static const std::string HELP_MESSAGE;

        // Default constructor
        Uniq() {
            (*this)
                .setInputFile("-")
                .setOutputFile("-")
                .setCountFlag(false)
                .setRepeatFlag(false)
                .setUniqueFlag(false)
                .setSkipCharsCount(0)
                .setSkipFieldsCount(0)
                .setAllRepeatedFlag(false)
                .setIgnoreCaseFlag(false)
                .setCompareCharsCount(-1)
                .setZeroTerminatedFlag(false);
        }

        // Builder pattern setters - bool
        Uniq          &setCountFlag(bool flag) { countFlag      = flag; return *this; }
        Uniq         &setRepeatFlag(bool flag) { repeatFlag     = flag; return *this; }
        Uniq         &setUniqueFlag(bool flag) { uniqueFlag     = flag; return *this; }
        Uniq     &setIgnoreCaseFlag(bool flag) { ignoreCase     = flag; return *this; }
        Uniq    &setAllRepeatedFlag(bool flag) { allRepeated    = flag; return *this; }
        Uniq &setZeroTerminatedFlag(bool flag) { zeroTerminated = flag; return *this; }

        // Builder pattern setters - int
        Uniq    &setSkipCharsCount(int   cnt) { skipCharsCnt    = cnt; return *this; }
        Uniq   &setSkipFieldsCount(int   cnt) { skipFieldsCnt   = cnt; return *this; }
        Uniq &setCompareCharsCount(int   cnt) { compareCharsCnt = cnt; return *this; }
        
        // Builder pattern setters - I/O
        Uniq &setInputFile(const std::string &fname) {
            if (fname.empty() || fname == "-")
                // No deletion for std::cin
                ip = std::shared_ptr<std::istream>(&std::cin, [](std::istream*) {});
            else {
                ip = std::make_shared<std::ifstream>(fname); 
                if (ip->fail()) {
                    std::cerr << "cuniq: " << fname << ": No such file or directory\n";
                    std::exit(1);
                }
            }


            return *this;
        }

        Uniq &setOutputFile(const std::string &fname) {
            if (fname.empty() || fname == "-")
                op = std::shared_ptr<std::ostream>(&std::cout, [](std::ostream*) {});
            else {
                op = std::make_shared<std::ofstream>(fname); 
                if (op->fail()) {
                    std::cerr << "cuniq: " << fname << ": No such file or directory\n";
                    std::exit(1);
                }
            }

            return *this;
        }

        // Execute command and output
        void execute() {
            std::queue<std::string> prevLines;
            std::string line;
            while (std::getline(*ip, line, zeroTerminated? '\0': '\n')) {
                if (!prevLines.empty() && !stringMatch(line, prevLines.front()))
                    printAndClear(prevLines);
                prevLines.push(line);
            }

            // Print the left over line
            printAndClear(prevLines);
        }
};

const std::string Uniq::CUNIQ_VERSION = "1.0.0";
const std::string Uniq::VERSION_MESSAGE {
    "cuniq (CPP Experiments) " + CUNIQ_VERSION + "\n"
    "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.\n\n"
    "Written by Naresh Jagadeesan.\n"
};
const std::string Uniq::HELP_MESSAGE {
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

    // Config variables
    std::string ifname, ofname;
    bool countFlag {false}, repeatFlag {false}, uniqueFlag {false};
    bool ignoreCase {false}, allRepeated {false}, zeroTerminated {false};
    int compareChars {-1}, skipChars {0}, skipFields {0};
    int argIdx {1};

    std::function<void(std::string&, int&, int&, const std::string&, const std::string&)> processIntArg {
        [&argc, &argv]
            (std::string &arg, int &argIdx, int &result, const std::string &shortP, const std::string &longP) {
                std::string charsStr;
                if (arg == shortP || arg == longP) {
                    if (argIdx + 1 < argc) charsStr = argv[++argIdx];
                    else { std::cerr << "cuniq: option requires an argument -- '" << shortP.back() << "'\n"; std::exit(1); }
                } else if (arg.size() >= longP.size() + 1 && arg.at(longP.size()) == '=') {
                    charsStr = arg.substr(longP.size() + 1);
                } else {
                    std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
                    std::exit(1);
                }

                // Check if compareCharsStr is valid and store it into compareChars
                parseInt(charsStr, result, "cuniq: " + charsStr + " invalid number of bytes to compare.");
            }
    };

    while (argIdx < argc) {
        std::string arg {argv[argIdx]};
        if (arg == "-c" || arg == "--count")
            countFlag = true;

        else if (arg == "-d" || arg == "--repeated")
            repeatFlag = true;

        else if (arg == "-h" || arg.starts_with("--h")) {
            std::cout << Uniq::HELP_MESSAGE;
            std::exit(0);
        } 

        else if (arg == "-v" || arg == "--version") {
            std::cout << Uniq::VERSION_MESSAGE;
            std::exit(0);
        } 

        else if (arg == "-u" || arg == "--unique")
            uniqueFlag = true;

        else if (arg == "-z" || arg == "--zero-terminated")
            zeroTerminated = true;

        else if (arg == "-i" || arg == "--ignore-case")
            ignoreCase = true;

        else if (arg == "-D" || arg == "--all-repeated")
            allRepeated = true;

        else if (arg == "-w" || arg.starts_with("--check-chars"))
            processIntArg(arg, argIdx, compareChars, "-w", "--check-chars");

        else if (arg == "-s" || arg.starts_with("--skip-chars"))
            processIntArg(arg, argIdx, skipChars, "-s", "--skip-chars");

        else if (arg == "-f" || arg.starts_with("--skip-fields"))
            processIntArg(arg, argIdx, skipFields, "-f", "--skip-fields");

        else if (arg.at(0) == '-' && arg.size() > 1) {
            std::cerr << "cuniq: unrecognized option '" << arg << "'\nTry 'cuniq --help for more information.\n'";
            std::exit(1);
        } 

        else if (ifname.empty()) ifname = arg;
        else ofname = arg;

        argIdx++;
    }

    // Validate params - repeatFlag & countFlag are incompatible
    if (allRepeated && countFlag) {
        std::cerr << "cuniq: printing all duplicated lines and repeat counts is meaningless.\n"
                  << "Try 'cuniq --help' for more information.\n";
        std::exit(1);
    }

    // Initalize the object
    Uniq uniq;
    uniq
        .setInputFile(ifname)
        .setOutputFile(ofname)
        .setCountFlag(countFlag)
        .setRepeatFlag(repeatFlag)
        .setUniqueFlag(uniqueFlag)
        .setSkipCharsCount(skipChars)
        .setSkipFieldsCount(skipFields)
        .setCompareCharsCount(compareChars)
        .setIgnoreCaseFlag(ignoreCase)
        .setAllRepeatedFlag(allRepeated)
        .setZeroTerminatedFlag(zeroTerminated);

    // Execute command
    uniq.execute();
}
