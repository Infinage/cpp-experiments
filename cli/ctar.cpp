#include "../misc/tarfile.hpp"
#include "argparse.hpp"

#include <print>

int main(int argc, char **argv) try {
    argparse::ArgumentParser cli {"ctar"};
    cli.description(
        "A lightweight tarball utility.\n\n"
        "Examples:\n"
        "  ctar -l -- in.tar\n"
        "  ctar -x -- in.tar\n"
        "  ctar -a folder,file.txt:docs/renamed.txt -- out.tar"
    );

    cli.addArgument("file", argparse::POSITIONAL)
        .help("Tar file path").required();

    cli.addArgument("list", argparse::NAMED)
        .alias("l").help("List the tar file contents")
        .implicitValue(true).defaultValue(false);

    cli.addArgument("extract", argparse::NAMED)
        .alias("x").help("Extract all tarfile to CWD")
        .implicitValue(true).defaultValue(false);

    cli.addArgument("add", argparse::NAMED)
        .alias("a").help("Add files to archive. Syntax: SRC[:ARCNAME]")
        .defaultValue(std::vector<std::string>{});

    cli.parseArgs(argc, argv);
    auto filePath = cli.get("file");

    auto    listFlag = cli.get<bool>("list");
    auto extractFlag = cli.get<bool>("extract");
    auto    addFiles = cli.get<std::vector<std::string>>("add");

    auto flags = {listFlag, extractFlag, !addFiles.empty()};
    if (std::ranges::count(flags, true) != 1)
        throw std::runtime_error{"Ctar Error: Must pick (only) one of: "
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

        for (const auto &arg: addFiles) {
            std::string srcStr = arg, arcname;

            if (auto pos = arg.find(':'); pos != std::string::npos) {
                srcStr  = arg.substr(0, pos), arcname = arg.substr(pos + 1);

                if (arcname.empty())
                    throw std::runtime_error{"Ctar Error: empty arcname in '" + arg + "'"};

                if (!arcname.empty() && arcname.front() == '/')
                    throw std::runtime_error{"Ctar Error: arcname must be relative: '" + arcname + "'"};
            }

            tf.add(srcStr, arcname);
        }
    }
}

catch(std::exception &ex) { std::println("{}", ex.what()); }
