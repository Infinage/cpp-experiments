#include <queue>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>

#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*
 * TODO:
 *  0. Propagate SIGINT to all child processes
 *  2. Piping commands: "<", ">", "|"
 *  3. Support &&, ||
 *  4. cd somewhere inside - echo hello && cd ~
 *  5. How does redirection like >&2 work? For ex: "find / -maxdepth 2 | wc -l" will output to stderr
 *  6. Does it support reading from stdin like "--"
 *  7. Running in background; support fg, bg
 *  8. Support operators like: $() or ``
 *  9. How do these commands work: more, vim
 * 10. What will csh do if executed csh binary? Nesting bash inside bash
 * 11. Shell history, arrow keys to navigate history
 * 12. Prompt with colors, current working directory. How sig_noop would handle it?
 */

namespace fs = std::filesystem;

class Shell {
private:
    static const std::unordered_set<char> WHITESPACES;
    static std::queue<pid_t> execIds;
    const std::unordered_set<std::string> shellCommands{"cd", "exit"};
    std::vector<std::string> paths{"/bin"};
    std::string currDirectory, homeDirectory;
    std::unordered_map<std::string, std::string> commands;

private:
    /* NOOP Signal Handler, prints "csh> " */
    static void SIG_CNOOP (int) { 
        std::cout << "\ncsh> "; 
        std::cout.flush(); 
    }

    /* SIGINT Handler, kill the executing process */
    static void SIG_CKILL (int) {
        std::cout << "\n";
        while (!execIds.empty()) {
            kill(execIds.front(), SIGKILL);
            execIds.pop();
        }
    }

    /* Check if line is complete */
    static bool checkLinePending(const std::string &line) {
        char insideString {0}, prevCh {0};
        for (const char &ch: line) {
            if (prevCh != '\\' && (ch == '\'' || ch == '"' || ch == '`')) {
                if (insideString == 0) insideString = ch;
                else if (insideString == ch) insideString = 0;
            }
            prevCh = ch;
        }

        // Is it stil pending?
        return insideString != 0 || prevCh == '\\';
    }

    /* Split an string into a vector of strings, delimited by whitespace */
    static std::vector<std::string> split(const std::string &str) {
        std::vector<std::string> splits;
        char insideString {0}, prevCh {0};
        std::string acc; 
        for (const char &ch: str) {
            bool isWhiteSpace {WHITESPACES.find(ch) != WHITESPACES.end()};
            if (prevCh != '\\' && (ch == '\'' || ch == '"' || ch == '`')) {
                if (!insideString) insideString = ch;
                else if (insideString == ch) insideString = 0;
                else acc += ch;
            } else if (insideString || !isWhiteSpace) {
                if (!insideString && prevCh == '\\') 
                    acc.pop_back();
                acc += ch;
            } else if (!acc.empty()) {
                splits.push_back(acc);
                acc.clear();
            }
            prevCh = ch;
        }

        // Insert last word
        if (!acc.empty())
            splits.push_back(acc);

        return splits;
    }

    /* Parse splits into edible commands for downstream processing */
    std::vector<std::vector<std::string>> parseSplits(const std::vector<std::string> &splits) const {
        std::vector<std::vector<std::string>> result{};
        for (std::size_t i {0}; i < splits.size(); i++) {
            const std::string &piece {splits[i]};
            if (piece == "|") {
                if (!result.empty() && !result.back().empty()) 
                    result.push_back({});
                else {
                    std::cerr << "Invalid command input passed.\n";
                    return {};
                }
            } else if (piece == "&&" || piece == "||" || (i > 0 && splits[i - 1].back() == ';')) {
                std::cerr << "Execution of multiple commands is currently not supported.\n";
                return {};
            } else {
                if (result.empty()) result.push_back({});
                result.back().push_back(piece.at(0) == '~'? homeDirectory + piece.substr(1): piece);
            }
        }

        return result;
    }

    /* Handle exit shell command */
    static bool handleExit() {
        std::cout << "exit\n";
        std::exit(0);
    }

    /* Handle cd shell command */
    bool handleChangeDirectory(const std::vector<std::string> &cmds) {
        if (cmds.size() > 2) {
            std::cerr << "cd: too many arguments.\n";
            return false;
        } else if (cmds.size() == 1) {
            currDirectory = homeDirectory;
            return true;
        } else {
            fs::path changePath {fs::path(cmds[1])};
            fs::path targetPath {changePath.is_absolute()? changePath: fs::path(currDirectory) / changePath};
            if (!fs::is_directory(targetPath)) {
                std::cerr << "cd: " << targetPath << ": No such directory.\n";
                return false;
            } else {
                currDirectory = targetPath.string();
                return true;
            }
        }
    }

    /* Check if current user running shell has exec permissions */
    bool checkExecPerm(const fs::directory_entry &dir) const {
        fs::perms filePerms {fs::status(dir).permissions()};
        unsigned int uid {getuid()}, gid {getgid()};
        struct stat fileStat; stat(dir.path().c_str(), &fileStat);

        if (fileStat.st_uid == uid && (filePerms & fs::perms::owner_exec) != fs::perms::none)
            return true;
        else if (fileStat.st_gid == gid && (filePerms & fs::perms::group_exec) != fs::perms::none)
            return true;
        else if ((filePerms & fs::perms::others_exec) != fs::perms::none)
            return true;
        else 
            return false;
    }

    /* 
     * Nonrecursively picks binaries found in paths adds them to commands
     * Binaries with same name that are encountered later will replace old ones
     */
    void populateCommandsFromPath() {
        for (const std::string &path: paths) {
            if (fs::is_directory(path)) {
                for (const fs::directory_entry &dir: fs::directory_iterator(path)) {
                    if (dir.is_regular_file() && checkExecPerm(dir))
                        commands[dir.path().filename()] = dir.path();
                }
            } else {
                std::cerr << path << ": is not a valid directory. Skipping..\n";
            }
        }
    }

    /* Get the home directory of the user */
    void setHomeDirectory() {
        const char* enVal {std::getenv("HOME")};
        if (enVal != NULL) homeDirectory = enVal;
        else {
            struct passwd *pwd {getpwuid(getuid())}; 
            if (pwd) homeDirectory = pwd->pw_dir;
            else {
                std::cerr << "Error setting home directory.\n";
                std::exit(1);
            }
        }
    }

    bool handleShellCommand(const std::vector<std::string> &cmds) {
        if (cmds[0] == "cd") 
            return handleChangeDirectory(cmds);
        else if (cmds[0] == "exit") 
            return handleExit();
        else 
            return false;
    }

    void exec(const std::vector<std::string> &cmds_) {
        // Convert cmds to a vector of char*
        std::vector<std::string> cmdsTmp{cmds_};
        std::vector<char *> cmds;
        for (std::string &str: cmdsTmp)
            cmds.push_back(str.data());
        cmds.push_back(nullptr);

        // Execute command
        char *environ[] { (char *)"TERM=xterm", nullptr };
        chdir(currDirectory.c_str());
        execvpe(commands.at(cmds[0]).c_str(), cmds.data(), environ);
        std::exit(1);
    }

    /* Execute a single instruction (without pipe) */
    bool execute(const std::vector<std::string> &cmds_) {
        // Command not found
        if (commands.find(cmds_[0]) == commands.end() && shellCommands.find(cmds_[0]) == shellCommands.end()) {
            std::cerr << "Command not in path: " << cmds_[0] << "\n";
            return false;
        }

        // Shell builtin command
        else if (shellCommands.find(cmds_[0]) != shellCommands.end()) {
            return handleShellCommand(cmds_);
        } 

        // Actual command to binary that may or may not require piping
        else {
            int execForkPipe[2];
            if (pipe(execForkPipe) == -1) { std::cerr << "Unable to open a pipe.\n"; return false; }

            pid_t execid = fork();
            if (execid == -1) { std::cerr << "Unable to fork.\n"; return false; }

            else if (execid == 0) {
                // Duplicate exec pipe to the main process
                bool redirectStatus {true};
                redirectStatus &= dup2(execForkPipe[1], STDERR_FILENO) != -1;
                redirectStatus &= dup2(execForkPipe[1], STDOUT_FILENO) != -1;
                close(execForkPipe[0]); close(execForkPipe[1]); 
                if (!redirectStatus) { std::cerr << "Unable to pipe to main process.\n"; std::exit(1); };

                // Execute the command
                exec(cmds_);
                return false;
            }

            else {
                // Push to static list of pids to kill on SIGINT
                execIds.push(execid);

                // Close write end from execvp fork process
                close(execForkPipe[1]);

                // Read from pipe
                char buffer[1024];
                ssize_t bytesRead;
                while ((bytesRead = read(execForkPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[bytesRead] = '\0';
                    std::cout << buffer; 
                }

                // Close read end
                close(execForkPipe[0]);

                // Wait for child process completion
                int status;
                waitpid(execid, &status, 0);
                execIds.pop();
                return WEXITSTATUS(status) == 0;
            }
        }
    }

    /* Execute a list of commands, piping inputs / outputs */
    bool executePipe(const std::vector<std::vector<std::string>> &cmds_) {
        // Child process to log error into errPipe, we keep track of prevPipe as well for next command
        int errPipe[2], prevOutPipe[2];
        if (pipe(errPipe) == -1) { std::cerr << "Unable to open a pipe.\n"; return false; }
        for (std::size_t i {0}; i < cmds_.size(); i++) {
            const std::vector<std::string> &cmds {cmds_[i]};
            int currOutPipe[2];
            if (pipe(currOutPipe) == -1) { std::cerr << "Unable to open a pipe.\n"; return false; }
            pid_t pid {fork()};
            if (pid == 0) {
                bool redirectStatus {true};
                if (i > 0) {
                    redirectStatus &= dup2(prevOutPipe[0], STDIN_FILENO) != -1;
                    close(prevOutPipe[0]); close(prevOutPipe[1]); 
                }
                close(errPipe[0]); close(currOutPipe[0]);
                redirectStatus &= dup2(errPipe[1], STDERR_FILENO) != -1;
                redirectStatus &= dup2(currOutPipe[1], STDOUT_FILENO) != -1;
                close(errPipe[1]); close(currOutPipe[1]);
                if (!redirectStatus) { std::cerr << "Unable to pipe to main process.\n"; std::exit(1); };

                // Command not found
                if (commands.find(cmds[0]) == commands.end() && shellCommands.find(cmds[0]) == shellCommands.end()) {
                    std::cerr << "Command not in path: " << cmds[0] << "\n";
                    std::exit(1);
                }

                // Shell builtin command
                else if (shellCommands.find(cmds[0]) != shellCommands.end()) {
                    bool status {handleShellCommand(cmds)};
                    std::exit(status? 0: 1);
                } 

                // Fork and execute
                else {
                    exec(cmds);
                    std::exit(1);
                }

            } else {
                execIds.push(pid);
                if (i > 0) { close(prevOutPipe[0]); close(prevOutPipe[1]); }
                std::copy(std::begin(currOutPipe), std::end(currOutPipe), std::begin(prevOutPipe));
            }
        }

        // Temp variables to read from STDOUT / STDERR
        char buffer[1024];
        ssize_t bytesRead;

        // Read from err pipe
        close(errPipe[1]);
        while ((bytesRead = read(errPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            std::cerr << buffer; 
        }
        close(errPipe[0]);

        // Read from out pipe
        close(prevOutPipe[1]);
        while ((bytesRead = read(prevOutPipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            std::cout << buffer; 
        }
        close(prevOutPipe[0]);

        // Wait for all the subshells to complete 
        bool status {true};
        while (!execIds.empty()) {
            int pidStatus;
            waitpid(execIds.front(), &pidStatus, 0);
            status &= WIFEXITED(pidStatus) && WEXITSTATUS(pidStatus) == 0;
            execIds.pop();
        }

        return status;
    }

public:
    Shell(): currDirectory(fs::current_path()) 
    { 
        populateCommandsFromPath(); 
        setHomeDirectory();
    }

    /* Start shell loop */
    void run() {
        // On SIGINT, do nothing
        std::signal(SIGINT, SIG_CNOOP);
        while(1) {
            std::string acc;
            std::cout << "csh> ";

            // Read until the input complete
            do {
                std::string line;
                std::getline(std::cin, line);
                acc += line;
            } while (!std::cin.eof() && checkLinePending(acc));

            // Split into tokens
            std::vector<std::vector<std::string>> cmdsList{parseSplits(split(acc))};
            if (cmdsList.empty()) {
                if (std::cin.eof()) { std::cout << "\n"; break; }
            } else {
                // Install signal interupt and remove post execution
                std::signal(SIGINT, SIG_CKILL);
                if (cmdsList.size() == 1) execute(cmdsList[0]);
                else executePipe(cmdsList);
                std::signal(SIGINT, SIG_CNOOP);
            }        
        }
    }
};

// Define static variables
const std::unordered_set<char> Shell::WHITESPACES {' ', '\t', '\v', '\f', '\r', '\n'};
std::queue<pid_t> Shell::execIds {};

int main(int argc, char**) {
    if (argc != 1) {
        std::cerr << "Sorry, execution of scripts is currently unsupported.\n";
        return 1;
    } 

    else {
        Shell sh;
        sh.run();
        return 0;
    }
}
