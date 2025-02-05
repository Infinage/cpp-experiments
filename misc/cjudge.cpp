#include <algorithm>
#include <cctype>
#include <sched.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/mount.h>
#include <sys/capability.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
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

        // Helper to set resource limits
        static void setResourceLimits(
            unsigned long memoryLimit, unsigned long nprocs,
            float cpuTime = 50, unsigned long fSize = 10
        ) {
            // Set memory limit for the process
            struct rlimit limit;
            limit.rlim_cur = limit.rlim_max = 1024 * 1024 * memoryLimit;
            if (setrlimit(RLIMIT_AS, &limit) != 0 || setrlimit(RLIMIT_STACK, &limit) != 0 
             || setrlimit(RLIMIT_DATA, &limit) != 0 || setrlimit(RLIMIT_RSS, &limit) != 0) {
                std::cerr << "Failed to set memory limit.\n";
                std::exit(1);
            }

            // Set file size limit for the process
            limit.rlim_cur = limit.rlim_max = 1024 * 1024 * fSize;
            if (setrlimit(RLIMIT_FSIZE, &limit) != 0) {
                std::cerr << "Failed to set file size limit.\n";
                std::exit(1);
            }

            // Set a limit to number of child processes
            limit.rlim_cur = limit.rlim_max = nprocs;
            if (setrlimit(RLIMIT_NPROC, &limit) != 0) {
                std::cerr << "Failed to set proc limit.\n";
                std::exit(1);
            }

            // Limit CPU time
            limit.rlim_cur = limit.rlim_max = static_cast<rlim_t>(cpuTime);
            if (setrlimit(RLIMIT_CPU, &limit) != 0) {
                std::cerr << "Failed to set proc limit.\n";
                std::exit(1);
            }
        }

        static void killAllExcept(const pid_t pid, const int signal) {
            for (const std::filesystem::directory_entry &dir: std::filesystem::directory_iterator("/proc")) {
                std::string dirStr {dir.path().filename()};
                bool isPid {dir.is_directory() && std::all_of(dirStr.cbegin(), dirStr.cend(), [](const char ch) { return std::isdigit(ch); })}; 
                if (isPid && dirStr != std::to_string(pid)) {
                    pid_t currPid {static_cast<pid_t>(std::stoul(dirStr))};
                    kill(currPid, signal);
                }
            }
        }

        // Helper to set timelimit
        static void setTimeLimit(float timeLimit, pid_t sandboxPID, pid_t cjudgePID) {
            const float MIN_TIME_LIMIT = 0.001f;
            if (timeLimit < MIN_TIME_LIMIT)
                timeLimit = MIN_TIME_LIMIT;

            // Setup a thread to monitor elapsed time, send SIGKILL on TLE
            std::thread([timeLimit, sandboxPID, cjudgePID](){
                // Wait duration in MS
                std::chrono::milliseconds waitDuration {static_cast<int>(timeLimit * 1.05 * 1000)};

                // Wait for specified time limit & kill if still running
                std::this_thread::sleep_for(waitDuration);
                if (kill(sandboxPID, 0) == 0) { 
                    std::cerr << "TLE.\n"; 
                    killAllExcept(cjudgePID, SIGKILL);
                }
            }).detach();
        }

        static void unshareAndMapUID(const int unshareFlags, bool rootUser = true) {
            // Store the UID / GID to be able to map later
            const int uid {static_cast<int>(geteuid())}, gid {static_cast<int>(getegid())};

            // Create new namespaces
            if (unshare(CLONE_NEWUSER | unshareFlags) == -1) { 
                std::cerr << "Failed to create namespaces.\n"; 
                std::exit(1); 
            }

            // UID / GID Mappings
            if (rootUser) {
                writeFile("/proc/self/uid_map", "0 " + std::to_string(uid) + " 1");
                writeFile("/proc/self/setgroups", "deny");
                writeFile("/proc/self/gid_map", "0 " + std::to_string(gid) + " 1");
            }
        }

        static void writeFile(const std::string &fpath, const std::string &line) {
            std::ofstream ofs {fpath};
            if (!ofs) { std::cerr << "Failed to open: " << fpath << "\n"; std::exit(1); }
            ofs << line << "\n";
            ofs.close();
        }

        void dropPriviledges() {
            // Unshare USER NS, map as nobody
            unshareAndMapUID(0, false);

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
        }

        void setupSandbox() {
            // Try umounting and mounting proc
			if (mount("proc", "/proc", "proc", 0, nullptr) == -1) {
				std::cerr << "Failed to mount proc filesystem.\n";
				std::exit(1);
			}

            // Mount FS as private
            unshareAndMapUID(CLONE_NEWNS);
            if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE | MS_RDONLY | MS_BIND | MS_REMOUNT, nullptr) == -1) {
                std::cerr << "Failed to remount root as private.\n"; std::exit(1);
            }

            // Create a new temp FS
            if (mount("tmpfs", "/tmp", "tmpfs", 0, nullptr) == -1) {
                std::cerr << "Failed to create a tmpfs.\n"; std::exit(1);
            }
        }

        void executeBinary() {
            // Drop the priviledges before running execvp
            dropPriviledges();

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
            args.push_back(NULL);

            // Execute the binary command
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
        const unsigned long nProcs;

    public:
        CodeJudge(
            const std::string &binaryCmd, 
            const std::string &questionsFile, 
            const std::string &answersFile, 
            const std::string &tmpActual, 
            const std::string &tmpError,
            const unsigned long memoryLimit,
            const float timeLimit,
            const unsigned long nProcs
        ):
            binaryCmd(binaryCmd), 
            questionsFile(questionsFile), 
            answersFile(answersFile),
            tmpActual(std::filesystem::current_path() / tmpActual), 
            tmpError(std::filesystem::current_path() / tmpError),
            memoryLimit(memoryLimit),
            timeLimit(timeLimit),
            nProcs(nProcs)
        {}

        bool run() {

            // Create the tmpActual / tmpError files
            std::ofstream afile{tmpActual, std::ios::trunc}, efile{tmpError, std::ios::trunc};
            if (!afile || !efile) { std::cerr << "Failed to create log files.\n"; std::exit(1); }

            // Unshare User, PID, Mount before Fork - Unshare Part 1
            unshareAndMapUID(CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET);
            int pid {fork()};
            if (pid == -1) { std::cerr << "Unable to fork.\n"; return false; }

            else if (pid == 0) {
                // Redirect stdin, stdout, stderr to respective files
                redirectStream(STDIN_FILENO, questionsFile, O_RDONLY);
                redirectStream(STDOUT_FILENO, tmpActual, O_WRONLY | O_CREAT | O_TRUNC);
                redirectStream(STDERR_FILENO, tmpError, O_WRONLY | O_CREAT | O_TRUNC);

                // Setup RLimits (stack / heap mem, nprocs, cpu time, etc) & Sandbox
                setResourceLimits(memoryLimit, nProcs);
                setupSandbox();

                int sandboxPID {fork()};
                if (sandboxPID == -1) { std::cerr << "Unable to create sandbox fork.\n"; return false; }
                else if (sandboxPID == 0) { executeBinary(); std::exit(1); } 
                else { 
                    int status {-1}; 
                    setTimeLimit(timeLimit, sandboxPID, getpid());
                    waitpid(sandboxPID, &status, 0); 
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

                    // Error log file stream
                    std::ifstream errLog{tmpError};
                    std::string errLine;
                    std::getline(errLog, errLine);

                    int signal {WTERMSIG(status)}; bool signaled {WIFSIGNALED(status)};
                    bool MLE {signaled && signal == SIGSEGV};
                    bool TLE {(signaled && signal == SIGTERM) || errLine == "TLE."};

                    if (TLE) std::cout << "Verdict: TLE\n";
                    else if (MLE) std::cout << "Verdict: MLE\n";
                    else {

                        std::cout << "Verdict: GERR\n";
                        if (WIFSIGNALED(status))
                            std::cout << "Reason: Terminated by signal " << signal << "\n";
                        else if (WIFEXITED(status))
                            std::cout << "Reason: Exited with status " << WEXITSTATUS(status) << "\n";

                        // Print out the logs
                        errLog.seekg(0);
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
            std::cout << "Usage: cjudge [--memory=256] [--time=0.5] [--nprocs=10] <binary_command> <questions_file> <answers_file>\n";
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
        unsigned long N_PROCS {10};

        // Parse the parameters
        for (int i {1}; i < argc - 3; i++) {
            std::string arg{argv[i]};
            if (arg.starts_with("--memory="))
                MEMORY_LIMIT_MB = std::stoul(arg.substr(9));
            else if (arg.starts_with("--time="))
                TIME_LIMIT_SEC = std::stof(arg.substr(7));
            else if (arg.starts_with("--nprocs="))
                N_PROCS = std::stoul(arg.substr(9));
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
            TIME_LIMIT_SEC,
            N_PROCS
        };

        // Execute the judge with provided inputs
        return judge.run();
    }
}
