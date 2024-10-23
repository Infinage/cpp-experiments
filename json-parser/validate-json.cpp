#include <algorithm>
#include <chrono>
#include <exception>
#include <format>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "json.hpp"

bool validateFile(const std::string &fname) {
    // Read file
    std::ifstream ifs({fname});

    if (!ifs)
        return false;

    else {

        // Read from file
        std::ostringstream raw;
        std::string buffer;
        while (std::getline(ifs, buffer))
            raw << buffer << "\n";

        // Convert to string
        std::string jsonStr = raw.str();

        // If invalid throws an exception
        try {
            JSON::JSONNode_Ptr root = JSON::Parser::loads(jsonStr);
            return true;
        }

        catch (std::exception &e) { return false; }
    }
}

int main(int argc, char* argv[]) {

    if (argc != 2)
        std::cout << "Usage: ./validate-json <filepath/dirpath>\n";

    else {

        std::string input{argv[1]};

        // Store the files here
        std::vector<std::string> files;
        if (std::filesystem::is_regular_file(input))
            files.push_back(input);
        else {
            for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(input))
                if (entry.is_regular_file() && entry.path().extension() == ".json")
                    files.push_back(entry.path().string());
        }

        // Sort lexiographically
        std::ranges::sort(files);

        // For formatting
        std::string lineSep = std::string(105, '-');

        // Header line
        std::cout << std::format(
            "{}\n| {:<55} | {:>15} | {:>15} | {:^7} |\n{}\n", lineSep,
            "File", "Size", "Time Taken", "Status", lineSep
        );

        for (const std::string &fpath: files) {
            // Time the execution
            std::chrono::time_point start {std::chrono::high_resolution_clock::now()};
            bool isValid {validateFile(fpath)};
            std::chrono::duration elapsed = std::chrono::high_resolution_clock::now() - start;

            // For display
            double fsize {(double) std::filesystem::file_size(fpath) / 1024};
            double elapsedMS {(double) std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() / 1000.0};
            std::string status {isValid ? "✅": "❌"};
            std::string fpathDisplay {fpath};
            if (fpath.size() > 55) 
                fpathDisplay = "..." + fpath.substr(fpath.size() - 52);

            std::cout << std::format(
                "| {:<55} | {:>12.2f} KB | {:>12.2f} ms | {:^8} |\n", 
                fpathDisplay, fsize, elapsedMS, status
            );
        }

        // Footer line
        std::cout << lineSep << "\n";
    }

    return 0;
}
