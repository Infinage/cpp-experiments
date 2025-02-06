#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * TODO:
 * 1. Reset ctrl C signal handler after execute
 * 2. Execute commands like "echo 'hello world'"
 */

namespace fs = std::filesystem;
static pid_t execid {-1};

class Shell {
private:
    std::vector<std::string> paths{"/bin"};
    std::unordered_map<std::string, std::string> commands;

private:
    /*
     * Check if current user running shell has exec permissions
     */
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

    /*
     * Execute the binary 
     */
    void execute(std::string &command) {
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

            // Execute command
            char **environ {};
            execle(commands.at(command).c_str(), command.c_str(), nullptr, environ);
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
                std::cerr << "Failed to execute: " << command << "\n";
        }
    }

public:
    Shell() { populateCommandsFromPath(); }

    void run() {
        while(1) {
            std::string line;
            std::cout << "csh> ";
            std::getline(std::cin, line);

            if (line == "exit" || !std::cin.good()) 
                break;
            else if (commands.find(line) == commands.end())
                std::cerr << line << ": command not in path.\n";
            else
                execute(line);
        }
    }
};

int main(int argc, char **argv) {
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
