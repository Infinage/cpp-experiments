# Project Overview

This `misc` folder contains various experimental C++ projects and snippets that are currently uncategorized within the main repository. Once enough projects follow a similar theme or pattern, they may be moved into their own organized folders.

## minesweeper

A CLI interactive Minesweeper game. You can start the game by specifying the number of rows and columns.

### Usage

Run the program with the number of rows and columns as arguments:

```bash
./build/minesweeper.out <rows> <cols>
```

## Sudoku

A CLI Sudoku generator and solver. The program can either generate a new Sudoku puzzle or solve an existing one from a file.

### Usage

1. Generate a new Sudoku puzzle: Run without a filename argument to generate a new puzzle.
2. Solve a Sudoku puzzle: Provide a filename containing the puzzle grid to solve it.

```
# Generate a new Sudoku puzzle and save it to `test.txt`
./build/sudoku.out | tee test.txt

# Solve the Sudoku puzzle stored in `test.txt`
./build/sudoku.out test.txt
```

If a second argument (filename) is provided, the program reads the puzzle from that file, solves it if there is a unique solution, and prints the solution to the console. If the puzzle has multiple solutions or is invalid, an error message is shown.

### Input File Format

The file should contain digits `1-9` and `.` characters to represent blank spaces. All other characters are ignored.

## WordSearch

A CLI Wordsearch puzzle generator and solver. The program can generate a word search grid from a provided list of words or solve an existing word search grid by searching for the words.

### Usage

1. Generate a word search puzzle: Provide a list of words as the argument to generate a word search puzzle.
2. Solve a word search puzzle: Provide a word list and a grid to search for the words in the grid.

```
# Generate a wordsearch puzzle with a given word list
./build/wordsearch.out <wordlist>

# Solve an existing wordsearch puzzle with a word list and grid
./build/wordsearch.out <wordlist> <grid>
```

### Input File Format

- Word list: A newline-separated list of words to include in the puzzle.
- Grid: A grid where the word search puzzle is represented as a string of characters (optionally space seperated), with each row on a new line.

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
