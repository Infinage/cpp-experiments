# Project Overview

This `misc` folder contains various experimental C++ projects and snippets that are currently uncategorized within the main repository. Once enough projects follow a similar theme or pattern, they may be moved into their own organized folders.

## minesweeper

A CLI interactive Minesweeper game. You can start the game by specifying the number of rows and columns.

### Usage

Run the program with the number of rows and columns as arguments:

```bash
./build/minesweeper.out <rows> <cols>
```

## Barcode Generator

A CLI-based Code 128 Barcode Generator that encodes messages into Code 128 barcodes and saves the output as a PBM file.

### Usage

Encode a message from a text file into a barcode:

- Provide an input file containing the message.
- Specify the output file name for the generated barcode.

```
# Generate a barcode from a text file containing a message
./barcode <inputFile> <outputFile>

# Sample Usage
./barcode message.txt output.pbm

# Optionally convert to png if imagemagik is available
./convert output.pbm output.png
```

### Brainfuck CLI Tool

A versatile Brainfuck interpreter, compiler, and transpiler.

#### Usage
```bash
brainfuck                  # Interactive Brainfuck shell
brainfuck <file>           # Compile .bf file to bytecode (.bfc)
brainfuck <option> <file>  # Perform an operation based on the option
```

#### Options
- `-p, --profile`   Execute raw .bf code without compiling.
- `-t, --transpile` Transpile .bfc bytecode to C source (.bfc.c).
- `-e, --exec`      Execute compiled .bfc bytecode.
- `-h, --help`      Display help information.

## ThreadPool Helper

A thread pool implementation that allows enqueuing tasks to be executed by a pool of worker threads. This is a reusable utility for any project requiring multithreading.

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
