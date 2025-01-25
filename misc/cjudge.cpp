#include <csignal>
#include <cstring>
#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Define Constants
constexpr const char* actualOutput {".actual.out"}; 
constexpr const char* actualError {".actual.err"};
constexpr int MEMORY_LIMIT_MB {256};
constexpr float TIME_LIMIT_SEC {0.5};

// Helper to redirect stream
void redirectStream(int originalFD, const std::string &redirectFileName, int flags, int mode = 0644) {
    int redirectFD {open(redirectFileName.c_str(), flags, mode)}, redirectStatus {-1};
    if (redirectFD != -1) { redirectStatus = dup2(redirectFD, originalFD); close(redirectFD); }
    if (redirectFD == -1 || redirectStatus == -1) {
        std::cerr << "Failed to redirect stream to file: " << redirectFileName << "\n";
        std::exit(1);
    }
}

void executeBinary(const std::string &binaryCmd) {
    // Get the binaryCmd split into tokens
    std::istringstream cmdStream{binaryCmd};
    std::vector<std::string> argStrs;
    std::string token;
    while (cmdStream >> token)
        argStrs.push_back(token.c_str()); 

    // Convert vector of strs into char*
    std::vector<char*> args;
    for (std::string &arg: argStrs)
        args.push_back(const_cast<char*>(arg.c_str()));

    // Execute the binary command
    args.push_back(NULL);
    execvp(args[0], args.data());

    // If this line is reached, execvp failed
    std::cerr << "Execution of binary failed: " << binaryCmd << "\n";
    std::exit(1);
}

bool compareFiles(const std::string &actualFile, const std::string &expectedFile, bool showError = false) {
    // Compare the output with the expected
    std::size_t lineCount {0};
    std::ifstream actual {actualFile}, expected {expectedFile};
    std::string actualLine, expectedLine;
    bool ok {true};
    while (actual.good() && expected.good() && ok) {
        lineCount++;
        std::getline(actual, actualLine); 
        std::getline(expected, expectedLine);
        ok = actualLine == expectedLine;
    }

    if (!ok && showError) { 
        std::cout << "Line#: " << lineCount << " differs.\n"
                  << std::string(20, '-') << "\n"
                  << "Actual  : " << actualLine << "\n"
                  << "Expected: " << expectedLine << "\n\n";
    }

    return ok;
}

int main(int argc, char **argv) {
    if (argc != 4) 
        std::cout << "Usage: cjudge ./solution inputs.txt answers.txt\n";

    else {
        std::string binaryCmd {argv[1]}, inputFile {argv[2]}, expectedFile{argv[3]};
        bool inputFileExists(std::filesystem::is_regular_file(inputFile)), 
             answersFileExists(std::filesystem::is_regular_file(expectedFile));

        if (!inputFileExists || !answersFileExists) {
            std::cerr << "File does not exist: " << (!inputFileExists? inputFile: expectedFile) << "\n";
            return 1;
        }

        int pid {fork()};

        if (pid == -1) { 
            std::cerr << "Unable to fork.\n"; 
            return 1;
        } 

        else if (pid == 0) {
            // Redirect stdin, stdout, stderr to respective files
            redirectStream(STDIN_FILENO, inputFile, O_RDONLY);
            redirectStream(STDOUT_FILENO, actualOutput, O_WRONLY | O_CREAT | O_TRUNC);
            redirectStream(STDERR_FILENO, actualError, O_WRONLY | O_CREAT | O_TRUNC);

            // Set memory limit
            struct rlimit memLimit;
            memLimit.rlim_cur = memLimit.rlim_max = 1024 * 1024 * MEMORY_LIMIT_MB;
            if (setrlimit(RLIMIT_AS, &memLimit) != 0) {
                std::cerr << "Failed to set memory limit.\n";
                return 1;
            }

            // Set Runtime limit
            struct itimerval timer;
            int sec = static_cast<int>(TIME_LIMIT_SEC);
            int usec = static_cast<int>((TIME_LIMIT_SEC - static_cast<float>(sec)) * 1000000);
            timer.it_value.tv_sec = sec;
            timer.it_value.tv_usec = usec;
            timer.it_interval.tv_sec = 0;
            timer.it_interval.tv_usec = 0;
            setitimer(ITIMER_REAL, &timer, NULL);

            // Set event handlers for TLE & MLE: TODO: not working
            std::signal(SIGALRM, [](int){ std::cerr << "TLE\n"; });
            std::signal(SIGSEGV, [](int){ std::cerr << "MLE\n"; });

            // Binary reads from stdin (piped from file)
            executeBinary(binaryCmd);
        }

        else {
            int status;
            if (waitpid(pid, &status, 0) == -1) {
                std::cerr << "Failed to wait for child process.\n";
                return 1;
            }

            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                bool verdict {compareFiles(actualOutput, expectedFile)};
                std::cout << "Verdict: " << (verdict? "PASS": "FAIL") << "\n";

            } else {
                std::cout << "Execution failed:\n";
                std::ifstream errLog{actualError};
                std::string errLine;
                while (errLog) {
                    std::getline(errLog, errLine); 
                    std::cout << errLine << "\n";
                }
            }

            // Cleanup files
            std::filesystem::remove(actualOutput);
            std::filesystem::remove(actualError);
        }
    }

    return 0;
}
