# Project Overview

This `cli` folder contains command-line utilities that offer functionality similar to common Unix tools but with custom implementations. Currently, it includes two projects: `fc` and `cdiff`.

## fc

A command-line utility similar to `wc`, designed to count files by their extensions given a directory path. It lists the counts of all varying extensions in a neat tabular format.

### Usage

Run the program with the directory path as an argument:

```bash
./build/fc.out <directory_path>
```

## cdiff

A simplified, custom implementation of the 'diff' command-line tool. It supports three modes:

- **Default Mode**: Highlights differences between files line-by-line. This mode applies if no specific mode is provided as arguments.
- **Context Mode**: Displays lines around the differing sections, similar to 'diff -c'.
- **Unified Mode**: Shows differing lines with a few surrounding lines, similar to 'diff -u'.

### Usage

Run the program by specifying the comparison mode and paths to two files:

```bash
./build/cdiff.out [-u | -c] <file1> <file2>
```

- Use '-u' for unified mode.
- Use '-c' for context mode.

## Build Instructions

To build the project, use the provided `Makefile`. It will create both debug and release versions of the executables.

### Targets

- **build-debug**: Compiles the source files into debug executables located in the `build` directory.
- **build-release**: Compiles the source files into release executables located in the `build` directory.
- **clean**: Removes the compiled executables from the `build` directory.

### Commands

You can use the following commands in your terminal:

```bash
make build-debug     # For debug builds
make build-release   # For release builds
make clean           # To clean up build files
```
