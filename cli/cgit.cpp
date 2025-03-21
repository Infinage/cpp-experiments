#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "argparse.hpp"
#include "../misc/iniparser.hpp"

namespace fs = std::filesystem;

std::string readTextFile(const fs::path &path) {
    std::ifstream ifs{path};
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

void writeTextFile(const std::string &data, const fs::path &path) {
    std::ofstream ofs{path, std::ios::trunc};
    if (!ofs) throw std::runtime_error("Failed to open file for writing: " + path.string());
    ofs << data;
    if (!ofs) throw std::runtime_error("Failed to write to file: " + path.string());
}

// Explictly disallow second parameter as a string to prevent errors
void writeTextFile(const std::string&, const std::string&) = delete;

class GitRepository {
    private:
        const fs::path workTree, gitDir;
        INI::Parser conf;  

        template <typename ...Args>
        fs::path repoPath(const std::string &first, const Args &...rest) {
            fs::path result {gitDir / first};
            (result /= ... /= rest);
            return result;
        }

    public:
        GitRepository(const fs::path &path, bool force = false):
            workTree(path), gitDir(path / ".git")
        {
            if (!force) {
                if (!fs::is_directory(gitDir))
                    throw std::runtime_error("Not a Git Repository: " + gitDir.string());
                if (!fs::is_regular_file(gitDir / "config"))
                    throw std::runtime_error("Configuration file missing");

                // Read the config file
                conf.reads(readTextFile(gitDir / "config"));
                std::string repoVersion {"** MISSING **"};
                if (!conf.exists("core", "repositoryformatversion") || (repoVersion = conf["core"]["repositoryformatversion"]) != "0")
                    throw std::runtime_error("Unsupported repositoryformaversion: " + repoVersion);
            }
            
            else {
                if (!fs::exists(workTree)) 
                    fs::create_directories(workTree);
                else {
                    if (!fs::is_directory(workTree))
                        throw std::runtime_error(workTree.string() + " is not a directory");
                    else if (!fs::is_empty(workTree))
                        throw std::runtime_error(workTree.string() + " is not empty");
                }

                // Create the folders required
                fs::create_directories(gitDir / "branches");
                fs::create_directories(gitDir / "objects");
                fs::create_directories(gitDir / "refs" / "tags");
                fs::create_directories(gitDir / "refs" / "heads");

                // .git/description
                writeTextFile("Unnamed repository; edit this file 'description' to name the repository.\n", gitDir / "description");

                // .git/HEAD
                writeTextFile("ref: refs/heads/main\n", gitDir / "HEAD");

                // .git/config - default config
                conf["core"]["repositoryformatversion"] = "0";
                conf["core"]["filemode"] = "false";
                conf["core"]["bare"] = "false";
                writeTextFile(conf.dumps(), gitDir / "config");
            }
        }

        fs::path repoDir() const { return gitDir; }

        template<typename ...Args>
        fs::path repoDir(const Args &...rest, bool create = false) const {
            fs::path fpath {repoPath(rest...)};
            if (create) fs::create_directories(fpath);
            return fpath;
        }

        template<typename ...Args>
        fs::path repoFile(const Args &...rest, bool create = false) const {
            fs::path fpath {repoPath(rest...)};
            if (create) fs::create_directories(fpath.parent_path());
            return fpath;
        }

        static GitRepository findRepo(const fs::path &path = ".") {
            if (fs::exists(path / ".git"))
                return GitRepository(path);
            else if (!path.has_parent_path())
                throw std::runtime_error("No git directory");
            else
                return findRepo(path.parent_path());
        }
};

int main(int argc, char **argv) {
    argparse::ArgumentParser argparser{"git"};
    argparser.description("CGit: A lite C++ clone of Git");

    argparse::ArgumentParser initParser{"init"};
    initParser.addArgument(argparse::Argument("path")
            .defaultValue(std::string{"."})
            .help("Where to create the repository"));

    argparser.addSubcommand(initParser);
    argparser.parseArgs(argc, argv);

    if (initParser.ok()) {
        std::string path {initParser.get<std::string>("path")};
        GitRepository(path, true);
    }

    else {
        std::cout << argparser.getHelp() << '\n';
    }

    return 0;
}
