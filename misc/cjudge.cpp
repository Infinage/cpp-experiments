#include <linux/capability.h>
#include <sched.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <linux/prctl.h> 
#include <sys/prctl.h>
#include <sys/mount.h>
#include <sys/capability.h>

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/*
 * Objectives
 *  - Run rootless
 *  - Set runtime limit
 *  - Set memory limit
 *  - Sandbox the environment
 */

class CodeJudge {
    private:
        // Helper to redirect stream
        static void redirectStream(int originalFD, const std::string &redirectFileName, int flags, int mode = 0644) {
            int redirectFD {open(redirectFileName.c_str(), flags, mode)}, redirectStatus {-1};
            if (redirectFD != -1) { redirectStatus = dup2(redirectFD, originalFD); close(redirectFD); }
            if (redirectFD == -1 || redirectStatus == -1) {
                std::cerr << "Failed to redirect stream to file: " << redirectFileName << "\n";
                std::exit(1);
            }
        }

        // Helper to set memory limit using c function
        static void setMemoryLimit(unsigned long memoryLimit) {
            struct rlimit memLimit;
            memLimit.rlim_cur = memLimit.rlim_max = 1024 * 1024 * memoryLimit;
            if (setrlimit(RLIMIT_AS, &memLimit) != 0) {
                std::cerr << "Failed to set memory limit.\n";
                std::exit(1);
            }
        }

        // Helper to set timelimit
        static void setTimeLimit(float timeLimit) {
            const float MIN_TIME_LIMIT = 0.001f;
            if (timeLimit < MIN_TIME_LIMIT)
                timeLimit = MIN_TIME_LIMIT;

            struct itimerval timer;
            int sec = static_cast<int>(timeLimit);
            int usec = static_cast<int>((timeLimit - static_cast<float>(sec)) * 1000000);
            timer.it_value.tv_sec = sec;
            timer.it_value.tv_usec = usec;
            timer.it_interval.tv_sec = 0;
            timer.it_interval.tv_usec = 0;
            setitimer(ITIMER_VIRTUAL, &timer, NULL);
        }

        void writeFile(const std::string &fpath, const std::string &line) {
            std::ofstream ofs {fpath};
            if (!ofs) { std::cerr << "Failed to open: " << fpath << "\n"; std::exit(1); }
            ofs << line << "\n";
            ofs.close();
        }

        void setupSandbox() {
            constexpr int UNSHARE_FLAGS { CLONE_NEWUSER | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET };
            const int uid {static_cast<int>(geteuid())}, gid {static_cast<int>(getegid())};

            // Create new namespaces
            if (unshare(UNSHARE_FLAGS) == -1) { std::cerr << "Failed to create namespaces.\n"; std::exit(1); }

            // UID / GID Mappings
            writeFile("/proc/self/uid_map", "0 " + std::to_string(uid) + " 1");
            writeFile("/proc/self/setgroups", "deny");
            writeFile("/proc/self/gid_map", "0 " + std::to_string(gid) + " 1");

            // Mount FS as read only
            if (mount(nullptr, "/", nullptr, MS_PRIVATE | MS_REC | MS_BIND | MS_REMOUNT | MS_RDONLY, nullptr) == -1) {
                std::cerr << "Failed to remount root as readonly.\n"; std::exit(1);
            }

            // Create a new temp FS
            if (mount("tmpfs", "/tmp", "tmpfs", 0, nullptr) == -1) {
                std::cerr << "Failed to create a tmpfs.\n"; std::exit(1);
            }

            // Bind the tmpActual & tmpError as RW
            if (mount(tmpActual.c_str(), tmpActual.c_str(), nullptr, MS_BIND, nullptr) == -1
             || mount(tmpError.c_str(), tmpError.c_str(), nullptr, MS_BIND, nullptr) == -1) {
                std::cerr << "Failed to bind mount tmpActual / tmpError.\n"; std::exit(1);
            }

            // Only allow selected capabilities
            cap_t caps{cap_get_proc()}; cap_clear(caps);
            cap_value_t allowedCaps[] {};
            cap_set_flag(caps, CAP_PERMITTED, 1, allowedCaps, CAP_SET);
            cap_set_flag(caps, CAP_EFFECTIVE, 1, allowedCaps, CAP_SET);
            if (cap_set_proc(caps) == -1) { std::cerr << "Failed to apply capability changes.\n"; std::exit(1); }

            // Set NO_NEW_PRIVS to prevent privilege escalation
            if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
                std::cerr << "Failed to set NO_NEW_PRIVS.\n";
                std::exit(1);
            }

            // Mount FS as write again and ensure it fails
            if (mount(nullptr, "/", nullptr, MS_REMOUNT, nullptr) != -1) {
                std::cerr << "Remounting as writable succeeded.\n"; std::exit(1);
            } 
        }

        void executeBinary() {
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
        }

        bool compareFiles(bool showError = false) {
            // Compare the output with the expected
            std::size_t lineCount {0};
            std::ifstream actual {tmpActual}, expected {answersFile};
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

    private:
        std::string binaryCmd, questionsFile, answersFile;
        std::string tmpActual, tmpError;
        const unsigned long memoryLimit;
        const float timeLimit;

    public:
        CodeJudge(
            const std::string &binaryCmd, 
            const std::string &questionsFile, 
            const std::string &answersFile, 
            const std::string &tmpActual, 
            const std::string &tmpError,
            const unsigned long memoryLimit,
            const float timeLimit
        ):
            binaryCmd(binaryCmd), 
            questionsFile(questionsFile), 
            answersFile(answersFile),
            tmpActual(std::filesystem::current_path() / tmpActual), 
            tmpError(std::filesystem::current_path() / tmpError),
            memoryLimit(memoryLimit),
            timeLimit(timeLimit)
        {}

        bool run() {

            // Create the tmpActual / tmpError files
            std::ofstream afile{tmpActual, std::ios::trunc}, efile{tmpError, std::ios::trunc};
            if (!afile || !efile) { std::cerr << "Failed to create log files.\n"; std::exit(1); }

            int pid {fork()};
            if (pid == -1) { std::cerr << "Unable to fork.\n"; return false; }

            else if (pid == 0) {
                // Redirect stdin, stdout, stderr to respective files
                redirectStream(STDIN_FILENO, questionsFile, O_RDONLY);
                redirectStream(STDOUT_FILENO, tmpActual, O_WRONLY | O_CREAT | O_TRUNC);
                redirectStream(STDERR_FILENO, tmpError, O_WRONLY | O_CREAT | O_TRUNC);

                // Set memory, time limits
                setMemoryLimit(memoryLimit);
                setTimeLimit(timeLimit);
                setupSandbox();

                // Binary reads from stdin (piped from file)
                int sandboxPID {fork()};
                if (sandboxPID == -1) { std::cerr << "Unable to create sandbox fork.\n"; return false; }
                else if (sandboxPID == 0) { executeBinary(); std::exit(1); } 
                else { 
                    int status {-1}; 
                    waitpid(pid, &status, 0); 
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) std::exit(0);
                    else { std::cerr << "Sandboxed process failed.\n"; std::exit(1); }
                }
            }

            else {
                bool result; int status;
                if (waitpid(pid, &status, 0) == -1) {
                    std::cerr << "Failed to wait for child process.\n";
                    result = false;
                }

                else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    result = compareFiles();
                    std::cout << "Verdict: " << (result? "PASS": "FAIL") << "\n";

                } else {

                    int signal {WTERMSIG(status)};
                    bool MLE {WIFSIGNALED(status) && signal == SIGSEGV};
                    bool TLE {WIFSIGNALED(status) && (signal == SIGALRM || signal == SIGVTALRM)};

                    if (TLE) std::cout << "Verdict: TLE\n";
                    else if (MLE) std::cout << "Verdict: MLE\n";
                    else {

                        std::cout << "Verdict: GERR\n";
                        if (WIFSIGNALED(status))
                            std::cout << "Reason: Terminated by signal " << signal << "\n";
                        else if (WIFEXITED(status))
                            std::cout << "Reason: Exited with status " << WEXITSTATUS(status) << "\n";

                        // Print out the logs
                        std::ifstream errLog{tmpError};
                        std::string errLine;
                        while (errLog) {
                            std::getline(errLog, errLine); 
                            std::cout << errLine << "\n";
                        }
                    }

                    result = false;
                }

                // Cleanup logs & return
                std::filesystem::remove(tmpActual);
                std::filesystem::remove(tmpError);
                return result;
            }
        }
};

int main(int argc, char **argv) {
    if (argc < 4) {
        if (argc == 1) {
            std::cout << "Usage: cjudge [--memory=256] [--cpus=0.5] <binary_command> <questions_file> <answers_file>\n";
            return 0;
        }
        else {
            std::cout << "Invalid no. of arguments passed.\n";
            return 1;
        }
    }

    else {

        // Define Constants
        constexpr const char* actualOutput {".actual.out"}; 
        constexpr const char* actualError {".actual.err"};

        // Specify memory and time limit
        unsigned long MEMORY_LIMIT_MB {256};
        float TIME_LIMIT_SEC {0.5};

        // Parse the parameters
        for (int i {1}; i < argc - 3; i++) {
            std::string arg{argv[i]};
            if (arg.starts_with("--memory="))
                MEMORY_LIMIT_MB = std::stoul(arg.substr(9));
            else if (arg.starts_with("--cpus="))
                TIME_LIMIT_SEC = std::stof(arg.substr(7));
            else {
                std::cerr << "Invalid argument: " << arg << "\n";
                std::exit(1);
            }
        }

        std::string binaryCmd {argv[argc - 3]}, questionsFile {argv[argc - 2]}, answersFile{argv[argc - 1]};
        bool questionsFileExists(std::filesystem::is_regular_file(questionsFile)), 
             answersFileExists(std::filesystem::is_regular_file(answersFile));

        if (!questionsFileExists || !answersFileExists) {
            std::cerr << "File does not exist: " << (!questionsFileExists? questionsFile: answersFile) << "\n";
            return 1;
        }

        // Initialize the code judge
        CodeJudge judge {
            binaryCmd, 
            questionsFile, 
            answersFile, 
            actualOutput, 
            actualError, 
            MEMORY_LIMIT_MB, 
            TIME_LIMIT_SEC
        };

        // Execute the judge with provided inputs
        return judge.run();
    }
}
