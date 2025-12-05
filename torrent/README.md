# CTorrent (C++ BitTorrent ~Client~ Downloader)

A BitTorrent client implemented from scratch in modern C++. No external dependencies are used for the torrent protocol itself — all networking, SHA1 hashing, JSON handling, bencoding, message parsing, and peer communication are custom built.

---

## ✨ Features

### **Torrent Metadata Parsing**

* Full bencode decoder/encoder
* Extracts info dictionary, file structure, piece hashes
* Computes `info_hash` using a self written SHA1 implementation
* Supports both single-file and multi-file torrents

### **Tracker Communication**

* Implements both **UDP** and **TCP** tracker protocols
* Custom `net.hpp` implementation for sockets, DNS, URL parsing, etc

### **Peer Wire Protocol**

* Non-blocking sockets with a custom poll based event loop
* Handshake, keep-alive, choke/unchoke, bitfield, have, request, piece, cancel, port
* Custom message framing, parsing, and state machine

### **Piece / Block Management**

* Tracks block requests per peer
* Handles timeouts, resets, and partial states
* Verifies piece hash before writing

### **Asynchronous Disk Writer**

* Background thread consuming a "bounded" queue
* Writes blocks into a temporary file, then fans out into final files
* Supports resume via saved piece level state (partial blocks discarded)
* Synchronous API exists but discouraged

### **Zero Third‑Party Dependencies**

Only OpenSSL is used (for HTTPS trackers). Everything else is handcrafted:

* `json.hpp` → JSON support
* `hashlib.hpp` → SHA1 implementation
* `net.hpp` → sockets, DNS, URL and many more
* `argparse.hpp` → CLI argment parsing

---

## ⚙️ Architecture Overview

The main thread drives **everything**: network I/O, peer state machines, scheduling, timeouts, and validated piece dispatch.

Networking uses **non‑blocking sockets** and a custom `poll()` loop that steps all peers and issues requests as soon as they're eligible.

The asynchronous disk writer runs its _own dedicated thread_, receiving only fully validated pieces. Its design is intentionally fire‑and‑forget — if writing a validated piece fails, that piece is considered permanently lost with _no safety net_.

Once every piece is downloaded and verified, the main thread signals the disk writer to finish. The writer then splits the large temporary blob into final files according to the torrent’s `files` metadata, and exits cleanly.

---

## 📂 Directory Structure

```
.
├── include/           # Public headers
├── src/               # Implementations
├── CMakeLists.txt
├── main.cpp           # CLI interface
└── README.md
```

Key components inside `include/`:

* **torrent_file.hpp** – Parse metadata & info_hash
* **torrent_tracker.hpp** – Communicate with trackers
* **peer_context.hpp** – Per-peer connection state
* **piece_manager.hpp** – Piece/block scheduling
* **protocol.hpp** – Build/parse wire protocol messages
* **disk_writer.hpp** – Async file writer
* **torrent_downloader.hpp** – Main orchestration layer

---

## 🚀 Build & Run

### **Requirements**

* C++23 compiler
* Linux (uses raw sockets + poll)
* OpenSSL (HTTPS tracker support)

### **Build**

```
cmake -DCMAKE_BUILD_TYPE=Release -B build . 
cmake --build build -j4
```

### **Run**

```
./build/ctorrent <path-to-file.torrent> <download-dir>
```

---

## 🔧 How It Works (High Level)

### 1. **Load .torrent File**

```
TorrentFile tf{"ubuntu.torrent"};
```

Parses bencode → extracts metadata → computes info_hash → loads piece hashes.

### 2. **Initalize torrent tracker to fetch peers**

```
TorrentTracker tt{tf};
```

### 3. **Start Downloading**

```
TorrentDownloader dl{tt, "./downloads"};
dl.download();
```

Manages all peers concurrently using poll().

### 4. **State Save & Resume**

When you hit Ctrl+C or an exception occurs, `TorrentDownloader`'s destructor collects the completed pieces (`haves` map) and writes them to a `.ctorrent` state file. On the next run, the CLI checks for this file and the temporary download buffer. If present, it restores progress and resumes the download from where it was stopped.

---

## 📌 Roadmap

* [ ] Add docker snapshot to freeze the environment (my libraries evolve fast :)
* [ ] Cleaner logs, implement logging with verbose/terse modes
* [ ] Add retry logic with incremental/exponential backoff for tracker failures
* [ ] Implement retries with incremental backoff for tracker URLs
* [ ] Periodically refresh the peer list when all current peers drop
* [ ] Implement rarest-first and other smarter scheduling algorithms
* [ ] Add seeding/upload mode to move toward full BitTorrent spec compliance

---

## Inspiration

1. [How to make your own bittorrent client](https://blog.jse.li/posts/torrent/)
2. [Building a BitTorrent client from the ground up in GO](https://blog.jse.li/posts/torrent/)

---

## 📜 License

Licensed under MIT — do whatever you want with it, just keep the license notice.  
See the root LICENSE file of the repository for full text.

---
