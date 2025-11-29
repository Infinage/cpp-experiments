// https://allenkim67.github.io/programming/2016/05/04/how-to-make-your-own-bittorrent-client.html

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
        Torrent::TorrentDownloader tDownloader {torrent, argc == 3? argv[2]: "."};
        tDownloader.download(peers);
    }
}
