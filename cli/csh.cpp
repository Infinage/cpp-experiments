#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>

#include <sstream>
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
static pid_t execid {-1};

class Shell {
private:
    std::vector<std::string> paths{"/bin"};
    std::string currDirectory, homeDirectory;
    std::unordered_map<std::string, std::string> commands;
    std::unordered_set<std::string> shellCommands{"cd", "exit"};

private:
    /* NOOP Signal Handler, prints "csh> " */
    static void SIG_CNOOP (int) { 
        std::cout << "\ncsh> "; 
        std::cout.flush(); 
    }

    /* SIGINT Handler, kill the executing process */
    static void SIG_CKILL (int) {
        std::cout << "\n";
        kill(execid, SIGKILL);
    }

    /* Split an string into a vector of strings, delimited by whitespace */
    std::vector<std::string> split(const std::string &str) const {
        std::istringstream iss {str};  
        std::vector<std::string> splits;
        std::string piece;
        while (iss >> piece) {
            if (piece.at(0) == '~')
                piece = homeDirectory + piece.substr(1);
            splits.push_back(piece);
        }

        return splits;
    }

    /* Handle exit shell command */
    static bool handleExit() {
        std::cout << "exit\n";
        std::exit(0);
    }

    /* Handle cd shell command */
    bool handleChangeDirectory(std::vector<std::string> &cmds) {
        if (cmds.size() > 2) {
            std::cerr << "cd: too many arguments.\n";
            return false;
        } else if (cmds.size() == 1) {
            currDirectory = homeDirectory;
            return true;
        } else {
            if (!fs::is_directory(cmds[1])) {
                std::cerr << "cd: " << cmds[1] << ": No such directory.\n";
                return false;
            } else {
                currDirectory = cmds[1];
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

    bool handleShellCommand(std::vector<std::string> &cmds) {
        if (cmds[0] == "cd") 
            return handleChangeDirectory(cmds);
        else if (cmds[0] == "exit") 
            return handleExit();
        else 
            return false;
    }

    /* Execute the binary */
    bool execute(std::vector<std::string> &cmds_) {
        if (commands.find(cmds_[0]) == commands.end() && shellCommands.find(cmds_[0]) == shellCommands.end()) {
            std::cerr << "Command not in path: " << cmds_[0] << "\n";
            return false;
        } else if (shellCommands.find(cmds_[0]) != shellCommands.end()) {
            return handleShellCommand(cmds_);
        } else {
            int pipefd[2];
            if (pipe(pipefd) == -1) { std::cerr << "Unable to open a pipe.\n"; return false; }

            execid = fork();  
            if (execid == -1) { std::cerr << "Unable to fork.\n"; return false; }

            else if (execid == 0) {
                // Close read end
                close(pipefd[0]);

                // Redirect output stdin, stderr to write end
                bool redirectStatus {true};
                redirectStatus &= dup2(pipefd[1], STDERR_FILENO) != -1;
                redirectStatus &= dup2(pipefd[1], STDOUT_FILENO) != -1;
                close(pipefd[1]);
                if (!redirectStatus) { std::cerr << "Unable to pipe.\n"; return false; };

                // Convert cmds to a vector of char*
                std::vector<char *> cmds;
                for (std::string& str: cmds_)
                    cmds.push_back(str.data());
                cmds.push_back(nullptr);

                // Execute command
                char *environ[] { (char *)"TERM=xterm", nullptr };
                chdir(currDirectory.c_str());
                execvpe(commands.at(cmds[0]).c_str(), cmds.data(), environ);
                return false;
            }

            else {
                // Close write end
                close(pipefd[1]);

                // Read from pipe
                char buffer[1024];
                ssize_t bytesRead;
                while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[bytesRead] = '\0';
                    std::cout << buffer; 
                }

                // Close read end
                close(pipefd[0]);

                // Wait for child process completion
                int status;
                waitpid(execid, &status, 0);
                return WEXITSTATUS(status) == 0;
            }
        }
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
            std::string line;
            std::cout << "csh> ";
            std::getline(std::cin, line);
            std::vector<std::string> cmds{split(line)};
            if (cmds.empty()) {
                if (std::cin.eof()) { std::cout << "\n"; break; }
            } else {
                // Install signal interupt and remove post execution
                std::signal(SIGINT, SIG_CKILL);
                execute(cmds);
                std::signal(SIGINT, SIG_CNOOP);
            }
        }
    }
};

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
