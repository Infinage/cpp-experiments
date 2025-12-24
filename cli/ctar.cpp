#include "../misc/tarfile.hpp"
#include "argparse.hpp"

#include <print>

int main(int argc, char **argv) try {
    argparse::ArgumentParser cli {"ctar"};
    cli.description("A lightweight tarball utility.");

    cli.addArgument("file", argparse::POSITIONAL)
        .help("Tar file path").required();

    cli.addArgument("list", argparse::NAMED)
        .alias("l").help("List the tar file contents")
        .implicitValue(true).defaultValue(false);

    cli.addArgument("extract", argparse::NAMED)
        .alias("x").help("Extract all tarfile to CWD")
        .implicitValue(true).defaultValue(false);

    cli.addArgument("add", argparse::NAMED)
        .alias("a").help("Add list of files to archive")
        .defaultValue(std::vector<std::string>{});

    cli.parseArgs(argc, argv);
    auto filePath = cli.get("file");

    auto    listFlag = cli.get<bool>("list");
    auto extractFlag = cli.get<bool>("extract");
    auto    addFiles = cli.get<std::vector<std::string>>("add");

    auto flags = {listFlag, extractFlag, !addFiles.empty()};
    if (std::ranges::count(flags, true) == 1)
        throw std::runtime_error{"Ctar Error: Can only pick one of options: "
            "'list', 'extract', 'add'"};

    if (listFlag) {
        for (auto &file: tar::TarFile(filePath).getMembers())
            std::println("{}", file.fullPath());
    }

    else if (extractFlag) {
        tar::TarFile(filePath).extractAll(".");
    }

    else if (!addFiles.empty()) {
        tar::TarFile tf {filePath, tar::FMode::WRITE};
        for (const auto &fpath: addFiles) tf.add(fpath);
    }
}

catch(std::exception &ex) { std::println("{}", ex.what()); }
