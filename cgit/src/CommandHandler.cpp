#include "../include/CommandHandler.hpp"
#include "../include/GitObjects.hpp"
#include "../include/GitRepository.hpp"
#include "../include/utils.hpp"

CommandHandler::CommandHandler(): argparser(initParser()) {}

void CommandHandler::handleArgs(int argc, char **argv) {
    // Parse the input args
    argparser.parseArgs(argc, argv);

    // References of the child parsers
    argparse::ArgumentParser &initParser        {argparser.getChildParser("init")};
    argparse::ArgumentParser &catFileParser     {argparser.getChildParser("cat-file")};
    argparse::ArgumentParser &hashObjectParser  {argparser.getChildParser("hash-object")};
    argparse::ArgumentParser &logParser         {argparser.getChildParser("log")};
    argparse::ArgumentParser &lsTreeParser      {argparser.getChildParser("ls-tree")};
    argparse::ArgumentParser &checkoutParser    {argparser.getChildParser("checkout")};
    argparse::ArgumentParser &showRefParser     {argparser.getChildParser("show-ref")};
    argparse::ArgumentParser &tagParser         {argparser.getChildParser("tag")};
    argparse::ArgumentParser &revPParser        {argparser.getChildParser("rev-parse")};
    argparse::ArgumentParser &lsFilesParser     {argparser.getChildParser("ls-files")};
    argparse::ArgumentParser &checkIgnoreParser {argparser.getChildParser("check-ignore")};
    argparse::ArgumentParser &statusParser      {argparser.getChildParser("status")};
    argparse::ArgumentParser &rmParser          {argparser.getChildParser("rm")};
    argparse::ArgumentParser &addParser         {argparser.getChildParser("add")};
    argparse::ArgumentParser &commitParser      {argparser.getChildParser("commit")};

    if (initParser.ok()) {
        std::string path {initParser.get("path")};
        GitRepository repo(path, true);
        std::cout << "Initialized empty Git repository in " << repo.repoDir() << '\n';
    }

    else if (catFileParser.ok()) {
        std::string objectHashPart {catFileParser.get("object")};
        GitRepository repo {GitRepository::findRepo()};
        std::string objectHash {repo.findObject(objectHashPart)};
        std::cout << repo.readObject(objectHash)->serialize() << '\n';
    }

    else if (hashObjectParser.ok()) {
        bool writeFile {catFileParser.get<bool>("write")};
        std::string fmt {catFileParser.get("type")}, path {catFileParser.get("path")};
        std::string data {readTextFile(path)};

        std::unique_ptr<GitObject> obj;
        if (fmt == "tag") 
            obj =    std::make_unique<GitTag>("", data);
        else if (fmt == "tree") 
            obj =   std::make_unique<GitTree>("", data);
        else if (fmt == "blob") 
            obj =   std::make_unique<GitBlob>("", data);
        else if (fmt == "commit") 
            obj = std::make_unique<GitCommit>("", data);
        else
            throw std::runtime_error("Unknown type " + fmt);

        std::cout << GitRepository::findRepo().writeObject(obj, writeFile) << '\n';
    }

    else if (logParser.ok()) {
        long maxCount {logParser.get<long>("max-count")};
        std::string objectHash {logParser.get("commit")};
        GitRepository repo {GitRepository::findRepo()}; 
        std::cout << repo.getLog(objectHash, maxCount);
        if (maxCount != 0) std::cout << '\n';
    }

    else if (lsTreeParser.ok()) {
        bool recurse {lsTreeParser.get<bool>("recursive")};
        std::string ref {lsTreeParser.get("tree")};
        std::cout << GitRepository::findRepo().lsTree(ref, recurse) << '\n';
    }

    else if (checkoutParser.ok()) {
        std::string ref {checkoutParser.get("commit")}, 
            path {checkoutParser.get("path")};
        GitRepository::findRepo().checkout(ref, path);
    }

    else if (showRefParser.ok()) {
        std::cout << GitRepository::findRepo().showAllRefs() << '\n';
    }

    else if (tagParser.ok()) {
        GitRepository repo {GitRepository::findRepo()};
        if (tagParser.exists("name")) {
            bool createTagObj {tagParser.get<bool>("create-tag-object")};
            std::string name {tagParser.get("name")}, 
                ref {tagParser.get("object")};
            repo.createTag(name, ref, createTagObj);
        } else {
            std::string result {repo.showAllTags()};
            std::cout << result;
            if (!result.empty()) std::cout << '\n';
        }
    }

    else if (revPParser.ok()) {
        std::string name {revPParser.get("name")}, 
            type {revPParser.get("type")};
        std::string result {GitRepository::findRepo().findObject(name, type, true)};
        std::cout << result;
        if (!result.empty()) std::cout << '\n';
    }

    else if (lsFilesParser.ok()) {
        bool verbose {lsFilesParser.get<bool>("verbose")};
        std::cout << GitRepository::findRepo().lsFiles(verbose) << '\n';
    }

    else if (checkIgnoreParser.ok()) {
        std::vector<std::string> paths {checkIgnoreParser.get<std::vector<std::string>>("path")};
        GitIgnore rules {GitRepository::findRepo().gitIgnore()};
        for (const std::string &path: paths)
            if (rules.check(path))
                std::cout << path << '\n';
    }

    else if (statusParser.ok()) {
        std::cout << GitRepository::findRepo().getStatus() << '\n';
    }

    else if (rmParser.ok()) {
        bool cached {rmParser.get<bool>("cached")};
        std::vector<std::string> paths {rmParser.get<std::vector<std::string>>("path")};
        GitRepository repo {GitRepository::findRepo()};
        repo.rm(repo.collectFiles(paths), !cached);
    }

    else if (addParser.ok()) {
        std::vector<std::string> paths {addParser.get<std::vector<std::string>>("path")};
        GitRepository repo {GitRepository::findRepo()};
        repo.add(repo.collectFiles(paths));
    }

    else if (commitParser.ok()) {
        GitRepository::findRepo().commit(commitParser.get("message"));
    }
    
    else {
        std::cout << argparser.getHelp() << '\n';
    }
}

argparse::ArgumentParser CommandHandler::initParser() {
    // Init the parser with description
    argparse::ArgumentParser argparser{"cgit"};
    argparser.description("CGit: A lite C++ clone of Git");

    // init command
    argparse::ArgumentParser &initParser{argparser.addSubcommand("init")};
    initParser.description("Initialize a new, empty repository.");
    initParser.addArgument("path", argparse::POSITIONAL)
        .defaultValue(".").help("Where to create the repository.");

    // cat-file command
    argparse::ArgumentParser &catFileParser{argparser.addSubcommand("cat-file")};
    catFileParser.description("Provide content of repository objects.");
    catFileParser.addArgument("object", argparse::POSITIONAL)
        .required().help("The object to display.");

    // hash-object command
    argparse::ArgumentParser &hashObjectParser{argparser.addSubcommand("hash-object")};
    hashObjectParser.description("Compute object ID and optionally creates a blob from a file.");
    hashObjectParser.addArgument("type").alias("t").help("Specify the type.").defaultValue("blob");
    hashObjectParser.addArgument("path").required().help("Read object from <path>.");
    hashObjectParser.addArgument("write", argparse::NAMED).alias("w")
        .help("Actually write the object into the database.")
        .implicitValue(true).defaultValue(false);

    // log command
    argparse::ArgumentParser &logParser{argparser.addSubcommand("log")};
    logParser.description("Display history of a given commit.")
        .epilog("Equivalent to `git log --pretty=raw`");
    logParser.addArgument("commit").defaultValue("HEAD").help("Commit to start at.");
    logParser.addArgument("max-count").scan<long>().defaultValue(-1l).alias("n").help("Limit the number of commits displayed.");

    // ls-tree command
    argparse::ArgumentParser &lsTreeParser{argparser.addSubcommand("ls-tree")};
    lsTreeParser.description("Pretty-print a tree object.");
    lsTreeParser.addArgument("tree", argparse::POSITIONAL).help("A tree-ish object.").required();
    lsTreeParser.addArgument("recursive", argparse::NAMED).alias("r").defaultValue(false)
        .implicitValue(true).help("Recurse into subtrees.");

    // checkout commnad
    argparse::ArgumentParser &checkoutParser{argparser.addSubcommand("checkout")};
    checkoutParser.description("Checkout a commit inside of a directory.");
    checkoutParser.addArgument("commit", argparse::POSITIONAL)
        .help("The commit or tree to checkout.").required();
    checkoutParser.addArgument("path", argparse::POSITIONAL)
        .help("The EMPTY directory to checkout on.").required();

    // show-ref command
    argparse::ArgumentParser &showRefParser{argparser.addSubcommand("show-ref")};
    showRefParser.description("List all references.");

    // tag command
    argparse::ArgumentParser &tagParser{argparser.addSubcommand("tag")};
    tagParser.description("List and create tags.");
    tagParser.addArgument("create-tag-object", argparse::NAMED).alias("a")
        .help("Whether to create a tag object.").defaultValue(false).implicitValue(true);
    tagParser.addArgument("name").help("The new tag's name.");
    tagParser.addArgument("object").help("The object the new tag will point to").defaultValue("HEAD");

    // rev-parse command
    argparse::ArgumentParser &revPParser{argparser.addSubcommand("rev-parse")};
    revPParser.description("Parse revision (or other objects) identifiers");
    revPParser.addArgument("name", argparse::POSITIONAL).help("The name to parse.").required();
    revPParser.addArgument("type", argparse::NAMED).alias("t").defaultValue("")
        .help("Specify the expected type - ['blob', 'commit', 'tag', 'tree']");

    // ls-files command
    argparse::ArgumentParser &lsFilesParser{argparser.addSubcommand("ls-files")};
    lsFilesParser.description("List all staged files.");
    lsFilesParser.addArgument("verbose", argparse::NAMED).alias("v")
        .defaultValue(false).implicitValue(true).help("Show everything.");

    // check-ignore command
    argparse::ArgumentParser &checkIgnoreParser{argparser.addSubcommand("check-ignore")};
    checkIgnoreParser.description("Check path(s) against ignore rules.");
    checkIgnoreParser.addArgument("path", argparse::POSITIONAL).required()
        .scan<std::vector<std::string>>().help("Paths to check.");

    // status command
    argparse::ArgumentParser &statusParser{argparser.addSubcommand("status")};
    statusParser.description("Show the working tree status.");

    // rm command
    argparse::ArgumentParser &rmParser{argparser.addSubcommand("rm")};
    rmParser.description("Remove files from the working tree and the index.");
    rmParser.addArgument("cached", argparse::NAMED).defaultValue(false).implicitValue(true)
        .help("Unstage and remove paths only from the index.");
    rmParser.addArgument("path", argparse::POSITIONAL).required().help("Files to remove.")
        .scan<std::vector<std::string>>();

    // add command
    argparse::ArgumentParser &addParser{argparser.addSubcommand("add")};
    addParser.description("Add files contents to the index.");
    addParser.addArgument("path", argparse::POSITIONAL).required().help("Files to add.")
        .scan<std::vector<std::string>>();

    // commit command
    argparse::ArgumentParser &commitParser{argparser.addSubcommand("commit")};
    commitParser.description("Record changes to the repository.");
    commitParser.addArgument("message", argparse::NAMED).required().alias("m")
        .help("Message to associate with this commit.");

    return argparser;
}
