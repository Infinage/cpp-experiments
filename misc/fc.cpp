/*
 * A simple command line utility that provides a snapshot of files in the specified directory.
 * Summary of extension counts
 *
 * TODO:
 * - Provide option to skip 'folders / folder paths'
 * - Sort result by counts
*/

#include <algorithm>
#include <iostream>
#include <filesystem>
#include <map>
#include <string>
#include <format>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {

    if (argc == 1) {
        std::cout << "Files Count: WC for directories\nUsage: ./fc <directory_path>\n";
    }

    else {

    std::string directoryName {argv[1]};

    if (!fs::exists(directoryName) || !fs::is_directory(directoryName)) {
        std::cout << "Invalid directory provided.\n";
        return 1;
    }

    else {
        
        std::map<std::string, int> extCounts;
        std::string::size_type maxWidth = 10; // "Extension"
        int totalFiles = 0;

        // Iterate through and find all the extensions
        for (const fs::directory_entry& dirEntry: fs::recursive_directory_iterator(directoryName)) {
            if (dirEntry.is_regular_file()) {
                totalFiles++;
                std::string filename {dirEntry.path().filename()};
                std::string::size_type extStart = filename.rfind('.');
                if ((int)extStart != -1) {
                    std::string ext = filename.substr(extStart + 1);
                    extCounts[ext]++;
                    maxWidth = std::max(maxWidth, ext.size());
                } else {
                    extCounts["* noext *"]++;
                }
            }
        }

        // Print out the extensions encountered so far
        std::string fmtStr = std::format("| {{:<{}}} | {{:>10}} |\n", maxWidth + 2); 
        std::cout << std::string(maxWidth + 19, '-') << "\n";
        std::cout << std::vformat(fmtStr, std::make_format_args("Extension", "Counts"));
        std::cout << std::string(maxWidth + 19, '-') << "\n";
        for (std::pair<std::string, int> kv: extCounts) {
            std::cout << std::vformat(fmtStr, std::make_format_args(kv.first, kv.second));
        }
        std::cout << std::string(maxWidth + 19, '-') << "\n";
        std::cout << std::vformat(fmtStr, std::make_format_args("Total", totalFiles));
        std::cout << std::string(maxWidth + 19, '-') << "\n";

        }
    }

    return 0;
}
