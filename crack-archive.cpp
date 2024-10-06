#include <atomic>
#include <cstdlib>
#include <future>
#include <iostream>
#include <string>
#include <format>
#include <fstream>
#include <thread>
#include <vector>

// Atomic flag to track if a valid password has been found
std::atomic<bool> passwordFound(false);
std::string PASSWORD = "";

bool checkPassword(const std::string& fname, const std::string& password) {
    std::string command = std::format("unzip -P '{}' -qq -t '{}' > /dev/null 2>&1", password, fname);
    int result = std::system(command.c_str());
    return result == 0;
}

void processChunk(const std::vector<std::string> passwords, const std::string filename) {
    for (std::string password: passwords) {
        if (passwordFound.load())
            return;
        else if (checkPassword(filename, password)) {
            passwordFound.store(true);
            PASSWORD = password;
            return;
        }
    }
}

int main() {

    std::string filename, dictionary, buffer;

    std::cout << "Enter zip file path: ";
    std::cin >> filename;

    std::cout << "Enter dictionary file path: ";
    std::cin >> dictionary;

    // Read all the passwords from dictionary into vector
    std::ifstream ifs {dictionary};
    std::vector<std::string> passwords;
    while (ifs >> buffer)
        passwords.push_back(buffer);

    // Divide passwords amongs threads available
    std::size_t numThreads = std::thread::hardware_concurrency();
    std::size_t chunkSize = passwords.size() / numThreads;
    std::vector<std::future<void>> futures;
    for (std::size_t i = 0; i < numThreads; i++) {
        auto start = passwords.begin() + (i * chunkSize);
        auto end = i < numThreads - 1? start + chunkSize: passwords.end();
        std::vector<std::string> chunk(start, end);

        // Spawn thread
        futures.push_back(std::async(std::launch::async, processChunk, chunk, filename));
    }

    // Wait for all threads to finish execution
    for (std::future<void>& ft: futures)
        ft.get();

    if (!passwordFound.load()) {
        std::cout << "Password not found in this dictionary\n";
        return 1;
    } else {
        std::cout << "Password found: " << PASSWORD << "\n";
        return 0;
    }

}
