#include <sys/resource.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <seccomp.h>

#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

class CodeJudge {
    private:
        // Syscalls that would be blocked with seccomp
        static constexpr int blackList[] {
            // Network related calls
            SCMP_SYS(socket), SCMP_SYS(connect), SCMP_SYS(accept), SCMP_SYS(bind), 
            SCMP_SYS(send), SCMP_SYS(recv), SCMP_SYS(sendto), SCMP_SYS(recvfrom), 

            // Dangerous sys calls
            SCMP_SYS(unlink), SCMP_SYS(rename), SCMP_SYS(link), SCMP_SYS(sendfile),
            SCMP_SYS(truncate), SCMP_SYS(ftruncate), SCMP_SYS(chmod), SCMP_SYS(chown),
            SCMP_SYS(setgid), SCMP_SYS(setuid), SCMP_SYS(setresuid), SCMP_SYS(setresgid),
            SCMP_SYS(setpgid), SCMP_SYS(setsid), SCMP_SYS(kill), SCMP_SYS(tgkill), 
            SCMP_SYS(rmdir),

            // Process management calls
            SCMP_SYS(fork), SCMP_SYS(vfork), SCMP_SYS(clone), SCMP_SYS(clone3),

            // Additional syscalls to block
            SCMP_SYS(creat), SCMP_SYS(mkdir), SCMP_SYS(mknod), SCMP_SYS(umount), 
            SCMP_SYS(mount), SCMP_SYS(prctl), SCMP_SYS(ptrace), SCMP_SYS(syslog), 
            SCMP_SYS(capget), SCMP_SYS(capset), SCMP_SYS(symlink),
        };

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

        // Set seccomp rules - block unneeded sys calls
        static void setSeccompRules() {
            scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
            if (!ctx) {
                std::cerr << "Failed to initialize seccomp.\n";
                std::exit(1);
            }

            // Blacklist the sys calls
            for (int syscall: blackList)
                seccomp_rule_add(ctx, SCMP_ACT_KILL, syscall, 0);

            // Activate seccomp
            if (seccomp_load(ctx) != 0) {
                std::cerr << "Failed to load seccomp rules.\n";
                std::exit(1);
            }

            // Release context (rules are already loaded)
            seccomp_release(ctx);
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
            std::exit(1);
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
            tmpActual(tmpActual), 
            tmpError(tmpError),
            memoryLimit(memoryLimit),
            timeLimit(timeLimit)
        {}

        ~CodeJudge() {
            std::filesystem::remove(tmpActual);
            std::filesystem::remove(tmpError);
        }

        bool run() {
            int pid {fork()};
            if (pid == -1) {
                std::cerr << "Unable to fork.\n"; 
                return false;
            }

            else if (pid == 0) {
                // Redirect stdin, stdout, stderr to respective files
                redirectStream(STDIN_FILENO, questionsFile, O_RDONLY);
                redirectStream(STDOUT_FILENO, tmpActual, O_WRONLY | O_CREAT | O_TRUNC);
                redirectStream(STDERR_FILENO, tmpError, O_WRONLY | O_CREAT | O_TRUNC);

                // Set memory, time limits & security rules
                setMemoryLimit(memoryLimit);
                setTimeLimit(timeLimit);
                setSeccompRules();

                // Binary reads from stdin (piped from file)
                executeBinary();
                return false;
            }

            else {
                int status;
                if (waitpid(pid, &status, 0) == -1) {
                    std::cerr << "Failed to wait for child process.\n";
                    return false;
                }

                else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    bool verdict {compareFiles()};
                    std::cout << "Verdict: " << (verdict? "PASS": "FAIL") << "\n";
                    return verdict;

                } else {

                    int signal {WTERMSIG(status)};
                    bool MLE {WIFSIGNALED(status) && signal == SIGSEGV};
                    bool TLE {WIFSIGNALED(status) && (signal == SIGALRM || signal == SIGVTALRM)};
                    bool SECCOMPVIOLATION {WIFSIGNALED(status) && signal == SIGSYS};

                    if (TLE) std::cout << "Verdict: TLE\n";
                    else if (MLE) std::cout << "Verdict: MLE\n";
                    else if (SECCOMPVIOLATION) std::cout << "Verdict: SERR\n";
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

                    return false;
                }
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
