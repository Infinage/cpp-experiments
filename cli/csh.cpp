#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <termios.h>

#include <fstream>
#include <queue>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*
 * Supported functionalities:
 * 1. Shell commands: cd, exit, history
 * 2. Binaries: auto picks all executables from "/bin"
 * 3. Simple pipes, without redirections
 * 4. History is auto picked from '~/.csh_history'
 * 5. Home is auto picked from env variable, strings starting with '~' are auto substituted
 * 6. Arrow keys for navigating history
 * 7. Interupt binary, EOF with ctrl + D
 *
 *  Unsupported functionalities: 
 *  1. Support binary from current directory
 *  2. Support &&, ||; Redirections like >&2.
 *  3. Running in background; support fg, bg
 *  4. Support operators like: $() or ``
 *  5. Executing scripts
 *  6. Tab completions
 */

namespace fs = std::filesystem;
constexpr const char* SHELL_PROMPT {"csh> "};

class History {
private:
    const unsigned int MAX_HISTORY {500};
    std::deque<std::string> history;
    const fs::path historyPath;

public:
    ~History() { writeHistory(); }
    History(const fs::path &homeDir): historyPath(homeDir / ".csh_history") {
        populateHistory();
    }

    std::string operator[] (const std::size_t &idx) const {
        return idx >= history.size()? history.back(): history[idx];
    }

    std::size_t size() { return history.size(); }
    std::size_t capacity() { return MAX_HISTORY; }

    void add(const std::string &command) {
        history.push_back(command);
        if (history.size() > MAX_HISTORY) 
            history.pop_front();
    }

    /* Populate history from `.csh_history` on object instantiation */
    void populateHistory() {
        history.clear();
        std::ifstream ifs {historyPath};
        std::string command;
        while (std::getline(ifs, command))
            add(command);
    }

    /* Write history to `.csh_history` */
    void writeHistory() {
        std::ofstream ofs {historyPath};
        for (const std::string &command: history)
            ofs << command << "\n";
    }
};

class UnbufferedIO {
private:
    struct termios orig_term;
    History &history;

    void enableRawMode() {
        if (tcgetattr(STDIN_FILENO, &orig_term) == -1) {
            std::cerr << "Failed to retrieve terminal settings.\n";
            std::exit(1);
        }

        struct termios new_term {orig_term};
        new_term.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | ISIG));
        new_term.c_cc[VMIN] = 1;
        new_term.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) == -1) {
            std::cerr << "Failed to modify terminal settings.\n";
            std::exit(1);
        }
    }

    void disableRawMode() {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &orig_term) == -1) {
            std::cerr << "Failed to revert terminal settings.\n";
            std::exit(1);
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


public:
    UnbufferedIO(History &history): history(history) {}
    std::string readLine() {
        enableRawMode();
        std::size_t maxIdx {history.size()}, histIdx {history.size()};
        std::string curr, buffer; char ch; std::size_t bufferIdx {0};
        while (read(0, &ch, 1) > 0) {
            if (ch == 4 || ch == '\n') {
                std::cout << '\n';
                if (checkLinePending(buffer)) {
                    std::cerr << "Sorry, multi line commands are not supported.\n" << std::flush;
                    buffer.clear();
                }
                if (ch == 4 && buffer.empty()) buffer = "exit";
                break;
            } else if (ch == 3) {
                std::cout << "^C\n" << SHELL_PROMPT << std::flush; 
                buffer.clear(); bufferIdx = 0;
            } else if (ch == '\x1b') {
                    char seq[3];
                    if (read(STDIN_FILENO, &seq[0], 1) != 1 || read(STDIN_FILENO, &seq[1], 1) != 1) 
                        continue;

                    // Up arrow
                    if (seq[0] == '[' && seq[1] == 'A') {
                        if (histIdx == 0) std::cout << '\a' << std::flush;
                        else {
                            --histIdx;
                            buffer = history[histIdx];
                            bufferIdx = buffer.size();
                            std::cout << "\r\x1b[2K" << SHELL_PROMPT << buffer << std::flush;
                        }
                    } 

                    // Down arrow
                    else if (seq[0] == '[' && seq[1] == 'B') {
                        if (histIdx == maxIdx) std::cout << '\a' << std::flush;
                        else {
                            ++histIdx;
                            buffer = histIdx == maxIdx? curr: history[histIdx];
                            bufferIdx = buffer.size();
                            std::cout << "\r\x1b[2K" << SHELL_PROMPT << buffer << std::flush;
                        }
                    } 

                    // Right arrow
                    else if (seq[0] == '[' && seq[1] == 'C') {
                        if (bufferIdx >= buffer.size()) std::cout << '\a' << std::flush;
                        else {
                            bufferIdx++;
                            std::cout << "\033[C" << std::flush;
                        }
                    } 

                    // Left arrow
                    else if (seq[0] == '[' && seq[1] == 'D') {
                        if (bufferIdx == 0) std::cout << '\a' << std::flush;
                        else {
                            bufferIdx--;
                            std::cout << "\033[D" << std::flush;
                        }
                    }

                    // Del key
                    else if (seq[0] == '[' && seq[1] == '3' && read(STDIN_FILENO, &seq[2], 1) == 1 && seq[2] == '~') {
                        if (bufferIdx >= buffer.size()) std::cout << '\a' << std::flush;
                        else {
                            buffer.erase(bufferIdx, 1);
                            std::cout << " \b" << buffer.substr(bufferIdx) + ' '
                                      << std::string(buffer.size() - bufferIdx + 1, '\b') 
                                      << std::flush;
                        }
                    }
            }  else if (ch == 127) {
                if (bufferIdx == 0) std::cout << '\a' << std::flush;
                else {
                    bufferIdx--;
                    buffer.erase(bufferIdx, 1);
                    std::cout << "\b \b" << buffer.substr(bufferIdx) << ' ' 
                              << std::string(buffer.size() - bufferIdx + 1, '\b') 
                              << std::flush;
                }
            } else if (std::isprint(ch)) {
                buffer.insert(buffer.begin() + static_cast<long>(bufferIdx++), ch);
                curr = buffer;
                std::cout << ch << buffer.substr(bufferIdx) 
                          << std::string(buffer.size() - bufferIdx, '\b') 
                          << std::flush;
            }
        }
        disableRawMode();
        return buffer;
    }
};

class Shell {
private:
    static const std::unordered_set<char> WHITESPACES;
    static std::queue<pid_t> execIds;

    const std::unordered_set<std::string> shellCommands{"cd", "exit", "history", "help"};
    std::vector<std::string> paths{"/bin"};
    fs::path currDirectory, homeDirectory;
    std::unordered_map<std::string, std::string> commands;
    History history;
    UnbufferedIO uio;

private:
    /* NOOP Signal Handler, prints "csh> " */
    static void SIG_CNOOP (int) { 
        std::cout << "\n" << SHELL_PROMPT; 
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
                if (piece.size() == 1 && piece.at(0) == '~') result.back().push_back(homeDirectory);
                else if (piece.size() >= 2 && piece.substr(0, 2) == "~/") result.back().push_back(homeDirectory / piece.substr(2));
                else result.back().push_back(piece);
            }
        }

        return result;
    }

    /* Handle exit shell command */
    bool handleExit() {
        history.writeHistory();
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
            fs::path targetPath {changePath.is_absolute()? changePath: currDirectory / changePath};
            if (!fs::is_directory(targetPath)) {
                std::cerr << "cd: " << targetPath << ": No such directory.\n";
                return false;
            } else {
                currDirectory = targetPath.string();
                return true;
            }
        }
    }

    /* Handle history shell command */
    bool handleHistory() {
        for (std::size_t i {0}; i < history.size(); i++)
            std::cout << "  " << i << "  " << history[i] << "\n";
        return history.size();
    }

    /* Handle help shell command */
    bool handleHelp(bool topicsRequested) {
        if (topicsRequested) std::cout << "Help topics not supported: Please use `man` instead.\n";
        else std::cout << "\n"
           "   ██████╗███████╗██╗  ██╗" << "\n"
           "  ██╔════╝██╔════╝██║  ██║" << "\n"
           "  ██║     ███████╗███████║" << "\n"
           "  ██║     ╚════██║██╔══██║" << "\n"
           "  ╚██████╗███████║██║  ██║" << "\n"
           "   ╚═════╝╚══════╝╚═╝  ╚═╝" << "\n"
            << "\nCSH Lite, version 1.0.1-release\n";
        return true;
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
    std::string getHomeDirectory() {
        const char* enVal {std::getenv("HOME")};
        if (enVal != NULL) return enVal;
        else {
            struct passwd *pwd {getpwuid(getuid())}; 
            if (pwd) return pwd->pw_dir;
            else {
                std::cerr << "Error setting home directory.\n";
                std::exit(1);
            }
        }
    }

    bool handleShellCommand(const std::vector<std::string> &cmds) {
        if      (cmds[0] ==      "cd") return handleChangeDirectory(cmds);
        else if (cmds[0] ==    "exit") return handleExit();
        else if (cmds[0] == "history") return handleHistory();
        else if (cmds[0] ==    "help") return handleHelp(cmds.size() > 1);
        else                           return false;
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
        fs::current_path(currDirectory);
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
                char buffer[1024]; ssize_t bytesRead;
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
        char buffer[1024]; ssize_t bytesRead;

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
    Shell(): 
        currDirectory(fs::current_path()), 
        homeDirectory(getHomeDirectory()), 
        history(History(homeDirectory)),
        uio(history)
    { 
        populateCommandsFromPath(); 
    }

    /* Start shell loop */
    void run() {
        while(1) {
            std::cout << SHELL_PROMPT << std::flush;
            std::string line {uio.readLine()};

            // Split into tokens
            std::vector<std::vector<std::string>> cmdsList{parseSplits(split(line))};
            if (cmdsList.empty()) {
                if (std::cin.eof()) { std::cout << "\n"; break; }
            } else {
                // Add the command to history
                history.add(line);

                // Install signal interupt and remove post execution
                std::signal(SIGINT, SIG_CKILL);
                if (cmdsList.size() == 1) execute(cmdsList[0]);
                else executePipe(cmdsList);
                std::signal(SIGINT, SIG_DFL);
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
    } else { 
        Shell sh;
        sh.run();
        return 0;
    }
}
