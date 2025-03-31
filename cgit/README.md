# CGit: A Lightweight Git Clone

CGit is a lightweight Git implementation written in C++ from scratch, using only the standard library (STL) and zlib for compression. 

## Features

CGit supports the following Git commands:

- `commit` - Record changes to the repository.
- `add` - Add file contents to the index.
- `ls-tree` - Pretty-print a tree object.
- `log` - Display the history of a given commit.
- `show-ref` - List all references.
- `hash-object` - Compute object ID and optionally create a blob from a file.
- `cat-file` - Provide content of repository objects.
- `tag` - List and create tags.
- `checkout` - Checkout a commit inside a directory.
- `rev-parse` - Parse revision (or other objects) identifiers.
- `ls-files` - List all staged files.
- `init` - Initialize a new, empty repository.
- `check-ignore` - Check path(s) against ignore rules.
- `status` - Show the working tree status.
- `rm` - Remove files from the working tree and the index.

_Packfiles supported out of the box. If a reference cannot be resolved as a loose object, it picks from packed objects_

## **Installation**  

To compile CGit, ensure you have a C++23-compatible compiler (e.g., GCC 13+) and zlib installed.  

### **Manual Compilation**  
Use the following command to compile CGit manually:  
```sh
g++ main.cpp src/* -o cgit -std=c++23 -lz -O2
```

### **Using CMake**  
Alternatively, you can use CMake to build and install CGit:  
```sh
mkdir build && cd build
cmake ..
cmake --build . --parallel
cmake --install .
```

## Usage

Run `cgit --help` to see available commands:

```sh
$ cgit --help
Usage: cgit [OPTIONS] {commit,add,ls-tree,log,show-ref,hash-object,cat-file,tag,checkout,rev-parse,ls-files,init,check-ignore,status,rm}

CGit: A lite C++ clone of Git

Subcommands:
 commit                 Record changes to the repository.
 add                    Add files contents to the index.
 ls-tree                Pretty-print a tree object.
 log                    Display history of a given commit.
 show-ref               List all references.
 hash-object            Compute object ID and optionally creates a blob from a file.
 cat-file               Provide content of repository objects.
 tag                    List and create tags.
 checkout               Checkout a commit inside of a directory.
 rev-parse              Parse revision (or other objects) identifiers
 ls-files               List all staged files.
 init                   Initialize a new, empty repository.
 check-ignore           Check path(s) against ignore rules.
 status                 Show the working tree status.
 rm                     Remove files from the working tree and the index.

Arguments:
 --help                 Display this help text and exit (implicit=1) (default=0)

```

Example usage:

```sh
$ cgit init  # Initialize a repository
$ cgit add file.txt  # Stage a file
$ cgit commit -m "Initial commit"  # Commit changes
$ cgit log  # View commit history
```

## Custom Header-Only Libraries

CGit is built with a collection of lightweight, header-only libraries of my own:

- `misc/iniparser.hpp` - Parses INI files, similar to Python's `configparser`.
- `cli/argparse.hpp` - Command-line argument parser.
- `misc/fnmatch.hpp` - Mimics Python's `fnmatch` for pattern matching.
- `misc/zhelper.hpp` - Wrapper around zlib for compression and decompression.
- `cryptography/hashlib.hpp` - SHA-1 implementation, similar to Python's `hashlib`.
- `misc/ordered_map.hpp` - Ordered map implementation preserving insertion order.

## Resources & Inspiration

- [Build Your Own Git - Coding Challenges](https://codingchallenges.fyi/challenges/challenge-git) - Coding challenge from John Crickett
- [Write Yourself a Git (WYAG)](https://wyag.thb.lt/) - A Python implementation of Git.
- [Unpacking Git Packfiles](https://codewords.recurse.com/issues/three/unpacking-git-packfiles) - Explanation of Git packfiles.
- [Git Internals part 2: packfiles](https://dev.to/calebsander/git-internals-part-2-packfiles-1jg8) - A Rust-based approach to packfiles.
- [Git Guts: Delta objects](https://awasu.com/weblog/git-guts/delta-objects/) - A Python implementation of Git packfiles.
