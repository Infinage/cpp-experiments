#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>

#include "argparse.hpp"
#include "../misc/iniparser.hpp"
#include "../misc/zhelper.hpp"
#include "../cryptography/hashlib.hpp"

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

class GitObject {
    public:
        const std::string fmt;
        GitObject(const std::string &fmt): fmt(fmt) {}
        virtual ~GitObject() = default;
        virtual void deserialize(const std::string&) = 0;
        virtual std::string serialize() const = 0;
};

class GitBlob: public GitObject {
    private:
        std::string data;

    public:
        GitBlob(const std::string &data): GitObject("blob") { deserialize(data); }
        void deserialize(const std::string &data) override { this->data = data; }
        std::string serialize() const override { return data; }
};

class GitCommit: public GitObject {
    private:
        stdx::ordered_map<std::string, std::string> data;

    public:
        GitCommit(const std::string &raw): GitObject("commit") { deserialize(raw); }

        void deserialize(const std::string &raw) override {
            enum states: short {START, KEY_DONE, MULTILINE_VAL, BODY_START};
            short state {START}; std::string acc {""}, key{""};
            for (const char &ch: raw) {
                if (state == BODY_START || (ch != ' ' && ch != '\n') || (ch == ' ' && state != START)) {
                    acc += ch;
                } else if (ch == ' ' && state == START) {
                    if (!acc.empty()) {
                        state = KEY_DONE; key = acc; acc.clear();
                    } else if (!data.empty()) {
                        state = MULTILINE_VAL;
                    } else {
                        throw std::runtime_error("Failed to deserialize commit - Multiline value without existing key.");
                    }
                } else if (ch == '\n' && state == START) {
                    state = BODY_START; key = "";
                } else if (ch == '\n' && state == KEY_DONE) {
                    data[key] = acc; acc.clear(); state = START;
                } else if (ch == '\n' && state == MULTILINE_VAL) {
                    data[key] += '\n' + acc;
                }
            }

            // We are adding all characters for the message body, need to remove '\n'
            if (!acc.empty() && acc.back() == '\n')
                acc.pop_back();

            // Add the body with empty string as header
            data[""] = acc;
        }

        std::string serialize() const override {
            std::ostringstream oss;
            for (const auto &[key, value]: data) {
                if (!key.empty()) {
                    oss << key << ' ';
                    for (const char &ch: value)
                        oss << (ch != '\n'? std::string(1, ch): "\n ");
                    oss << '\n';
                }
            }
            oss << '\n' << data.at("");
            return oss.str();
        }
};

class GitTree: public GitObject {
    public:
        GitTree(const std::string&): GitObject("tree") {}

        void deserialize(const std::string&) {

        }

        std::string serialize() const {

        }
};

class GitTag: public GitObject {
    public:
        GitTag(const std::string&): GitObject("tag") {}

        void deserialize(const std::string&) {

        }

        std::string serialize() const {

        }
};

class GitRepository {
    private:
        const fs::path workTree, gitDir;
        INI::Parser conf;  

        fs::path repoPath(const std::initializer_list<std::string_view> &parts) const {
            fs::path result {gitDir};
            for (const std::string_view &part: parts)
                result /= part;
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

        fs::path repoDir(const std::initializer_list<std::string_view> &parts, bool create = false) const {
            fs::path fpath {repoPath(parts)};
            if (create) fs::create_directories(fpath);
            return fpath;
        }

        fs::path repoFile(const std::initializer_list<std::string_view> &parts, bool create = false) const {
            fs::path fpath {repoPath(parts)};
            if (create) fs::create_directories(fpath.parent_path());
            return fpath;
        }

        static GitRepository findRepo(const fs::path &path_ = ".") {
            fs::path path {fs::absolute(path_)};
            if (fs::exists(path / ".git"))
                return GitRepository(path);
            else if (!path.has_parent_path())
                throw std::runtime_error("No git directory");
            else
                return findRepo(path.parent_path());
        }

        std::string writeObject(const std::unique_ptr<GitObject> &obj, bool write = false) const {
            // Serialize the object data
            std::string serialized {obj->serialize()};

            // Add header and compute hash
            serialized = obj->fmt + ' ' + std::to_string(serialized.size()) + '\x00' + serialized;
            std::string objectHash {hashutil::sha1(serialized)};

            // Write the object to disk
            if (write) {
                fs::path path {repoFile({"objects", objectHash.substr(0, 2), objectHash.substr(2)}, true)};
                zhelper::zwrite(serialized, path);
            }

            return objectHash;
        }

        std::string findObject(const std::string &name, const std::string &fmt = "", bool follow = true) const {
            return name;
        }

        std::unique_ptr<GitObject> readObject(const std::string &objectHashPart) const {
            std::string objectHash {findObject(objectHashPart)};
            fs::path path {repoFile({"objects", objectHash.substr(0, 2), objectHash.substr(2)})};
            std::string raw {zhelper::zread(path)};

            // Format: "<FMT> <SIZE>\x00<DATA...>"
            std::size_t fmtEndPos {raw.find(' ')};
            std::string fmt {raw.substr(0, fmtEndPos)};
            std::size_t sizeEndPos {raw.find('\x00', fmtEndPos)};
            std::size_t size {std::stoull(raw.substr(fmtEndPos + 1, sizeEndPos - fmtEndPos))};

            if (size != raw.size() - sizeEndPos - 1)
                throw std::runtime_error("Malformed object " + objectHash + ": bad length");

            std::string data {raw.substr(sizeEndPos + 1)};
            if (fmt == "tag") 
                return    std::make_unique<GitTag>(data);
            else if (fmt == "tree") 
                return   std::make_unique<GitTree>(data);
            else if (fmt == "blob") 
                return   std::make_unique<GitBlob>(data);
            else if (fmt == "commit") 
                return std::make_unique<GitCommit>(data);
            else
                throw std::runtime_error("Unknown type " + fmt + " for object " + objectHash);
        }
};

int main(int argc, char **argv) {
    argparse::ArgumentParser argparser{"git"};
    argparser.description("CGit: A lite C++ clone of Git");

    argparse::ArgumentParser initParser{"init"};
    initParser.addArgument(argparse::Argument("path").defaultValue(".")
            .help("Where to create the repository"));

    argparse::ArgumentParser catFileParser{"cat-file"};
    catFileParser.addArgument(argparse::Argument("object")
            .required().help("The object to display"));

    argparse::ArgumentParser hashObjectParser{"hash-object"};
    hashObjectParser.addArgument(argparse::Argument("type").alias("t")
            .help("Specify the type").defaultValue("blob"));
    hashObjectParser.addArgument(argparse::Argument("write").alias("w")
            .help("Actually write the object into the database")
            .implicitValue(true).defaultValue(false));
    hashObjectParser.addArgument(argparse::Argument("path")
            .required().help("Read object from <path>"));

    argparser.addSubcommand(initParser);
    argparser.addSubcommand(catFileParser);
    argparser.addSubcommand(hashObjectParser);
    argparser.parseArgs(argc, argv);

    if (initParser.ok()) {
        std::string path {initParser.get<std::string>("path")};
        GitRepository(path, true);
    }

    else if (catFileParser.ok()) {
        std::string objectHash {catFileParser.get<std::string>("object")};
        std::cout << GitRepository::findRepo().readObject(objectHash)->serialize() << '\n';
    }

    else if (hashObjectParser.ok()) {
        bool writeFile {catFileParser.get<bool>("write")};
        std::string fmt {catFileParser.get<std::string>("type")}, path {catFileParser.get<std::string>("path")};
        std::string data {readTextFile(path)};

        std::unique_ptr<GitObject> obj;
        if (fmt == "tag") 
            obj =    std::make_unique<GitTag>(data);
        else if (fmt == "tree") 
            obj =   std::make_unique<GitTree>(data);
        else if (fmt == "blob") 
            obj =   std::make_unique<GitBlob>(data);
        else if (fmt == "commit") 
            obj = std::make_unique<GitCommit>(data);
        else
            throw std::runtime_error("Unknown type " + fmt + "!");

        std::cout << GitRepository::findRepo().writeObject(obj, writeFile) << '\n';
    }

    else {
        std::cout << argparser.getHelp() << '\n';
    }

    return 0;
}
