#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sstream>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * TODO:
 * 0. cd without any arguments or with a tilde switches to user home
 * 1. Clear screen
 * 2. Piping commands: "<", ">", "|"
 * 3. Running in background
 * 3. How do these commands work: clear, more
 * 4. What will csh do if executed csh binary? Nesting bash inside bash
 * 5. Shell history, arrow keys to navigate history
 * 6. Prompt with colors, current working directory. How sig_noop would handle it?
 */

namespace fs = std::filesystem;
static pid_t execid {-1};

class Shell {
private:
    std::vector<std::string> paths{"/bin"};
    std::string currDirectory, homeDirectory;
    std::unordered_map<std::string, std::string> commands;

private:

    /* NOOP Signal Handler, prints "csh> " */
    static void SIG_NOOP (int) { 
        std::cout << "\ncsh> "; 
        std::cout.flush(); 
    }

    /* Split an string into a vector of strings, delimited by whitespace */
    static std::vector<std::string> split(const std::string &str) {
        std::istringstream iss {str};  
        std::vector<std::string> splits;
        std::string piece;
        while (iss >> piece) 
            splits.push_back(piece);

        return splits;
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

    /* Execute the binary */
    void execute(std::vector<std::string> &cmds_) {
        int pipefd[2];
        if (pipe(pipefd) == -1) { std::cerr << "Unable to open a pipe.\n"; return; }

        execid = fork();  
        if (execid == -1) { std::cerr << "Unable to fork.\n"; }

        else if (execid == 0) {
            // Close read end
            close(pipefd[0]);

            // Redirect output stdin, stderr to write end
            bool redirectStatus {true};
            redirectStatus &= dup2(pipefd[1], STDOUT_FILENO) != -1;
            redirectStatus &= dup2(pipefd[1], STDERR_FILENO) != -1;
            close(pipefd[1]);
            if (!redirectStatus) { std::exit(1); };

            // Convert cmds to a vector of char*
            std::vector<char *> cmds;
            for (std::string& str: cmds_)
                cmds.push_back(str.data());
            cmds.push_back(nullptr);

            // Execute command
            char **environ {};
            chdir(currDirectory.c_str());
            execvpe(commands.at(cmds[0]).c_str(), cmds.data(), environ);
            std::exit(1);
        }

        else {
            // Install signal interupt
            std::signal(SIGINT, [](int) { kill(execid, SIGKILL); });

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
            if (WEXITSTATUS(status) != 0)
                std::cerr << "Failed to execute.\n";

            // Remove the signal handler
            std::signal(SIGINT, SIG_NOOP);
        }
    }

public:
    Shell(): currDirectory(fs::current_path()) 
    { 
        populateCommandsFromPath(); 
    }

    /* Start shell loop */
    void run() {
        // On SIGINT, do nothing
        std::signal(SIGINT, SIG_NOOP);

        while(1) {
            std::string line;
            std::cout << "csh> ";
            std::getline(std::cin, line);
            std::vector<std::string> cmds{split(line)};
            if (cmds.empty()) {
                if (std::cin.eof()) { std::cout << "\n"; break; }
            } else if (cmds[0] == "exit") {
                break;
            } else if (cmds[0] == "cd") {
                if (cmds.size() > 2) {
                    std::cerr << "cd: too many arguments.\n";
                } else if (cmds.size() == 1 || cmds[1] == "~") {
                    std::cerr << "cd: switching to home is currently unsupported.\n";
                } else {
                    if (fs::is_directory(cmds[1])) 
                        currDirectory = cmds[1];
                }
            } else if (commands.find(cmds[0]) == commands.end()) {
                std::cerr << "Command not in path: " << cmds[0] << "\n";
            } else {
                execute(cmds);
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
