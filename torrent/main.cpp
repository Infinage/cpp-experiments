#include "include/torrent_downloader.hpp"
#include "include/torrent_file.hpp"
#include "include/torrent_tracker.hpp"

#include "../cli/argparse.hpp"
#include "../misc/logger.hpp"

int main(int argc, char **argv) try {
    // Define the command line parser with supported arguments
    argparse::ArgumentParser cli {"ctorrent"};
    cli.addArgument("torrent-file").alias("f").required().help("Path to the .torrent file.");

    cli.addArgument("download-directory").alias("d").defaultValue("downloads")
        .help("Where to store the downloaded files?");

    cli.addArgument("block-size", argparse::NAMED).alias("b").defaultValue(1 << 14)
        .validate<int>(argparse::validators::between(1, 1 << 16))
        .help("Block size in bytes for peer requests (max 16 KB per spec)");

    cli.addArgument("backlog", argparse::NAMED).alias("L").defaultValue(8)
        .validate<int>(argparse::validators::between(1, 32))
        .help("Number of concurrent block requests to keep pipelined per peer");

    cli.addArgument("unchoke-attempts", argparse::NAMED).alias("u").defaultValue(3)
        .validate<int>(argparse::validators::between(1, 10))
        .help("Disconnect peer after this many unanswered unchoke attempts");

    cli.addArgument("recon-attempts", argparse::NAMED).alias("r").defaultValue(3)
        .validate<int>(argparse::validators::between(0, 10))
        .help("Maximum no of attempts we will try to reconnect a dropped peer");

    cli.addArgument("max-iwait", argparse::NAMED).alias("i").defaultValue(5)
        .validate<int>(argparse::validators::between(1, 600))
        .help("Maximum seconds a peer may stay idle before being disconnected or reset");

    cli.addArgument("min-rwait", argparse::NAMED).alias("w").defaultValue(30)
        .validate<int>(argparse::validators::between(1, 6000))
        .help("Minimum seconds we will wait before attempting to reconnect to a disconnected peer");

    cli.addArgument("timeout", argparse::NAMED).alias("t").defaultValue(10)
        .validate<int>(argparse::validators::between(1, 120))
        .help("Timeout (in seconds) for trackers and general socket operations");

    cli.addArgument("verbose", argparse::NAMED).alias("v").defaultValue<short>(3)
        .implicitValue<short>(4).validate<short>(argparse::validators::between<short>(1, 5))
        .help("Controls logging verbosity (1=ERROR, 2=WARN, 3=INFO, 4=DEBUG, 5=TRACE)");

    cli.description("A minimal BitTorrent client written in C++");
    cli.epilog("Most options are tuned to sane defaults. Adjust them only if you know what you’re optimizing.");

    // Parse and extract the arguments
    cli.parseArgs(argc, argv);
    std::string torrentFilePath {cli.get("torrent-file")};
    std::string downloadDirectory {cli.get("download-directory")};
    auto blockSize {static_cast<std::uint16_t>(cli.get<int>("block-size"))};
    auto backlog {static_cast<std::uint8_t>(cli.get<int>("backlog"))};
    auto unchokeAttempts {static_cast<std::uint8_t>(cli.get<int>("unchoke-attempts"))};
    auto reconAttempts {static_cast<std::uint8_t>(cli.get<int>("recon-attempts"))};
    auto reqWaitTime {static_cast<std::uint16_t>(cli.get<int>("max-iwait"))};
    auto reconWaitTime {static_cast<std::uint16_t>(cli.get<int>("min-rwait"))};
    auto timeout {cli.get<int>("timeout")};
    auto verbose {cli.get<short>("verbose")};

    // Verbosity of logger set by the verbosity cli arg (validated b/w 1-5)
    Logging::Dynamic::setLogLevel(static_cast<Logging::Level>(verbose));

    // Actual torrent stuff
    Torrent::TorrentFile torrent{torrentFilePath};
    Torrent::TorrentTracker tTracker {torrent};
    Torrent::TorrentDownloader tDownloader {tTracker, downloadDirectory, blockSize, 
        backlog, unchokeAttempts, reconAttempts, reqWaitTime, reconWaitTime};
    tDownloader.download(timeout);
} 

// Catch exceptions & just print the message, ensures RAII cleanup can happen
catch (std::exception &ex) { Logging::Dynamic::Error("{}", ex.what()); }
