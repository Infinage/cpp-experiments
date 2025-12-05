#include "include/torrent_downloader.hpp"
#include "include/torrent_file.hpp"
#include "include/torrent_tracker.hpp"

#include "../cli/argparse.hpp"

#include <print>

int main(int argc, char **argv) try {

    argparse::ArgumentParser cli {"ctorrent"};

    cli.addArgument("torrent-file").alias("f").required().help("Path to the .torrent file.");

    cli.addArgument("download-directory").alias("d").defaultValue("downloads")
        .help("Where to store the downloaded files?");

    cli.addArgument("block-size", argparse::NAMED).alias("b").defaultValue(1 << 14)
        .validate<int>(argparse::validators::between(1, 1 << 16))
        .help("Block size in bytes for peer requests (max 16 KB per spec).");

    cli.addArgument("backlog", argparse::NAMED).alias("L").defaultValue(8)
        .validate<int>(argparse::validators::between(1, 32))
        .help("Number of concurrent block requests to keep pipelined per peer.");

    cli.addArgument("unchoke-attempts", argparse::NAMED).alias("u").defaultValue(3)
        .validate<int>(argparse::validators::between(1, 10))
        .help("Disconnect peer after this many unanswered unchoke attempts.");

    cli.addArgument("max-wait", argparse::NAMED).alias("w").defaultValue(5)
        .validate<int>(argparse::validators::between(1, 600))
        .help("Maximum seconds a peer may stay idle before being disconnected or reset.");

    cli.addArgument("timeout", argparse::NAMED).alias("t").defaultValue(10)
        .validate<int>(argparse::validators::between(1, 120))
        .help("Timeout (in seconds) for network operations.");

    cli.addArgument("max-queue", argparse::NAMED).alias("Q").defaultValue(5000)
        .validate<int>(argparse::validators::between(1000, 20000))
        .help("Max pending validated pieces before disk writer blocks the main thread.");

    cli.addArgument("verbose", argparse::NAMED).alias("v").defaultValue(false)
        .implicitValue(true).help("Enable verbose logging");

    cli.description("Command line torrent downloader");

    // Parse and extract the arguments
    cli.parseArgs(argc, argv);
    std::string torrentFilePath {cli.get("torrent-file")};
    std::string downloadDirectory {cli.get("download-directory")};
    auto blockSize {static_cast<std::uint16_t>(cli.get<int>("block-size"))};
    auto backlog {static_cast<std::uint8_t>(cli.get<int>("backlog"))};
    auto unchokeAttempts {static_cast<std::uint8_t>(cli.get<int>("unchoke-attempts"))};
    auto waitTime {static_cast<std::uint16_t>(cli.get<int>("max-wait"))};
    auto timeout {cli.get<int>("timeout")};

    // Actual torrent stuff
    Torrent::TorrentFile torrent{torrentFilePath};
    Torrent::TorrentTracker tTracker {torrent};
    Torrent::TorrentDownloader tDownloader {tTracker, downloadDirectory, blockSize, 
        backlog, unchokeAttempts, waitTime};
    tDownloader.download(timeout);
} 

// Catch exceptions & just print the message, ensures RAII cleanup can happen
catch (std::exception &ex) { std::println("{}", ex.what()); }
