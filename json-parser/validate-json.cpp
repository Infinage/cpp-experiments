#include <algorithm>
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

        if (std::filesystem::is_regular_file(input)) {
            std::cout << std::format("{:-<30}> {:<10}\n", input + ' ', validateFile(input)? "Valid": "Invalid");
        }

        else {
            std::vector<std::string> files;
            for (const std::filesystem::directory_entry &entry: std::filesystem::directory_iterator(input))
                if (entry.is_regular_file())
                    files.push_back(entry.path().string());

            std::ranges::sort(files);
            for (const std::string &fpath: files)
                std::cout << std::format("{:-<30}> {:<10}\n", fpath + ' ', validateFile(fpath)? "Valid": "Invalid");
        }

    }

    return 0;
}
