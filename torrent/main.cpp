#include "include/torrent_downloader.hpp"
#include "include/torrent_file.hpp"
#include "include/torrent_tracker.hpp"

#include <print>

int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) 
        std::println("Usage: ctorrent <torrent-file> [<download-directory>]");
    else {
        Torrent::TorrentFile torrent{argv[1]};
        Torrent::TorrentTracker tTracker {torrent};
        auto peers {tTracker.getPeers()};
        try {
            Torrent::TorrentDownloader tDownloader {torrent, argc == 3? argv[2]: "downloads"};
            tDownloader.download(peers);
        } catch (std::exception &ex) { std::println("{}", ex.what()); }
    }
}
