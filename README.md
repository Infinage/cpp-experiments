# C++ Experiments

A growing collection of practical C++ projects — from system tools and network programming to games, design patterns, and real-world protocol implementations.

## Project Overview

| Project         | Description |
|----------------|-------------|
| `cgit`         | Lightweight Git reimplementation in C++ (STL + zlib), supporting commands like `commit`, `add`, `log`, `status`, `checkout`, etc. |
| `cli`          | A suite of custom Unix-like command-line tools (`cdiff`, `csh`, `cuniq`, `fc`, etc.) built using C++. |
| `cryptography` | Toy implementations of classic ciphers (Vigenère, ROT), hash functions, and a password archive cracker. |
| `design-patterns` | Code examples of all major GoF design patterns categorized into behavioral, creational, and structural groups. |
| `finance`      | A moving average crossover backtester for basic quantitative trading strategies. |
| `json-parser`  | A simple C++ JSON library for creating, manipulating, and validating JSON with test scaffolding. |
| `misc`         | Miscellaneous utilities like a Brainfuck interpreter, ASCII art generator, CSV/XML tools, and Mandelbrot visualizer. |
| `networking`   | Low-level socket programming examples, a tiny HTTP server, and my awesome `net.hpp` library with tests. |
| `pacman`       | A partial implementation of a Pacman clone using SFML for rendering sprites and animations. |
| `puzzles`      | Solvers for logic puzzles like N-Queens (with Dockerfile), Sudoku, Skyscrapers, and Wordsearch. |
| `redis-server` | Redis clone with single-threaded async processing, persistence (`dump.rdb`), and support for common commands (`GET`, `SET`, `LPUSH`, `KEYS`, etc.). |
| `torrent`      | BitTorrent client with custom bencode, tracker I/O, poll-based peer messaging, piece hashing, async disk writes, and resume support. |
| `url-shortner` | An in-memory, async C++ URL shortener deployed on AWS with a minimalist HTML/CSS/JS UI. |
| `webdriverxx`  | Header-only C++ Selenium WebDriver client with support for Chrome, Firefox, Edge and features like RAII sessions and detailed logging. |

---

## Header-Only Libraries

| Header                 | Description                                                                                                   |
| ---------------------- | ------------------------------------------------------------------------------------------------------------- |
| `cli/argparse.hpp`     | Lightweight single-header argument parser, inspired by Python’s argparse.                                     |
| `misc/CSVUtil.hpp`     | CSV reader/writer mimicking Python’s `csv` module.                                                            |
| `misc/zhelper.hpp`     | Simplified zlib wrapper for compression/decompression with file I/O support.                                  |
| `misc/bigint.hpp`      | Arbitrary-precision integer class using base-10,000 representation; supports overloaded arithmetic operators. |
| `misc/cache.hpp`       | LRU cache for any function using variadic templates; inspired by Python’s `functools.lru_cache`.              |
| `misc/png_reader.hpp`  | Minimal libpng wrapper for reading PNG files.                                                                 |
| `misc/ordered_map.hpp` | Insertion-ordered hash map using `std::list` and `unordered_map`, similar to Python’s `dict`.                 |
| `misc/threadPool.hpp`  | Custom thread pool supporting task queueing, worker control, and stateful execution.                          |
| `misc/fnmatch.hpp`     | Filename matching utility similar to Python’s `fnmatch`.                                                      |
| `misc/iniparser.hpp`   | INI config parser with an interface similar to Python’s `configparser`.                                       |
| `json-parser/json.hpp` | Standalone JSON parser with object manipulation and validation support. First project; may be messy.          |
| `networking/net.hpp`   | TCP/SSL sockets, serialization helpers, safer wrappers, and other goodies                                     |
| `misc/tarfile.hpp`     | POSIX ustar-compatible TAR reader/writer with support for files, directories, symlinks, and metadata.         |
| `misc/xml.hpp`         | Lightweight XML parser and serializer with DOM-style traversal and zero external dependencies.                |

---

### License

This collection of projects is licensed under the MIT License. See the LICENSE file for details.
