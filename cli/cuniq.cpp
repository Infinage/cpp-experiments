#include <cstring>
#include <fstream>
#include <iostream>
#include <istream>
#include <ostream>
#include <string>

const std::string CUNIQ_VERSION = "1.0.0"; // Example version

int main(int argc, char **argv) {

    std::string ifname, ofname;
    bool countFlag {false}, repeatFlag {false}; 
    bool printAllDuplicatesFlag {false};

    int i {1};
    while (i < argc) {
        std::string arg {argv[i]};
        if (arg == "-c" || arg == "--count") {
            countFlag = true;
        } else if (arg == "-d" || arg == "--repeated") {
            repeatFlag = true;
        } else if (arg == "-D") {
            printAllDuplicatesFlag = true;
        } else if (arg == "-h" || arg.starts_with("--h")) {
            std::cout << "Usage: cuniq [OPTION]... [INPUT [OUTPUT]]\n"
                      << "Filter adjacent matching lines from INPUT (or standard input),\n"
                      << "writing to OUTPUT (or standard output).\n\n"
                      << "With no options, matching lines are merged to the first occurrence.\n\n"
                      << "Mandatory arguments to long options are mandatory for short options too.\n"
                      << "  -c, --count           prefix lines by the number of occurrences\n"
                      << "  -d, --repeated        only print duplicate lines, one for each group\n"
                      << "  -D                    print all duplicate lines\n"
                      << "      --all-repeated[=METHOD]  like -D, but allow separating groups\n"
                      << "                                 with an empty line;\n"
                      << "                                 METHOD={none(default),prepend,separate}\n"
                      << "  -f, --skip-fields=N   avoid comparing the first N fields\n"
                      << "      --group[=METHOD]  show all items, separating groups with an empty line;\n"
                      << "                          METHOD={separate(default),prepend,append,both}\n"
                      << "  -i, --ignore-case     ignore differences in case when comparing\n"
                      << "  -s, --skip-chars=N    avoid comparing the first N characters\n"
                      << "  -u, --unique          only print unique lines\n"
                      << "  -z, --zero-terminated     line delimiter is NUL, not newline\n"
                      << "  -w, --check-chars=N   compare no more than N characters in lines\n"
                      << "      --help        display this help and exit\n"
                      << "      --version     output version information and exit\n\n"
                      << "A field is a run of blanks (usually spaces and/or TABs), then non-blank\n"
                      << "characters.  Fields are skipped before chars.\n";
            std::exit(0);
        } else if (arg == "--version") {
            std::cout << "cuniq (CPP Experiments) " << CUNIQ_VERSION << "\n"
                      << "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>.\n"
                      << "This is free software: you are free to change and redistribute it.\n"
                      << "There is NO WARRANTY, to the extent permitted by law.\n\n"
                      << "Written by Naresh Jagadeesan.\n";
          std::exit(0);
        } else if (arg.at(0) == '-' && arg.size() > 1) {
            std::cerr << "cuniq: unrecognized option '--q'\nTry 'cuniq --help' for more information.\n";
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
        *op << "cuniq: test: No such file or directory\n";
        std::exit(1);
    }

    std::string line, prevLine;
    while (std::getline(*inp, line)) {
        if (line != prevLine) {
            *op << line << "\n";
            prevLine = line;
        }
    }

    if (inp != &std::cin) delete inp;
    if (op != &std::cout) delete op;
}
