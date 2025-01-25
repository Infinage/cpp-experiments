# Project Overview

This contains a collection of C++ solutions to various algorithmic puzzles and challenges.

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

## Skyscrapers

A CLI puzzle game generator and solver based on the popular Skyscrapers logic puzzle. The program can generate new puzzles, verify puzzle solutions, and solve given puzzles.

### Usage

- Generate a Skyscrapers puzzle: Run the program with the desired grid size as an argument.
```
echo 5 | ./skyscrapers generate
```

- Solve a Skyscrapers puzzle: Provide a text file containing the puzzle definition.
```
./skyscrapers solve < puzzle.txt
```

### Input file format

- The corners represent the puzzle size.
- Clues are positioned around the edges of the grid (top, right, bottom, left), with 0 indicating no clue.
- 0 inside the grid denotes empty cells to be filled.

#### Sample puzzle

```
8 0 0 0 3 3 4 0 3 8
0 0 0 0 0 0 0 0 0 3
3 0 0 3 0 0 0 0 0 0
4 0 0 0 0 0 3 0 5 0
0 3 0 0 0 6 0 0 0 5
2 0 0 0 0 0 0 0 1 4
0 4 5 0 0 0 0 0 0 0
3 0 0 0 0 5 0 4 0 0
0 0 0 0 0 0 0 0 3 0
8 2 0 1 0 4 0 4 3 8
```

Puzzle size: The corners (8 at the top-left and bottom-right) indicate the grid is 8x8.
Clues: The first and last rows/columns around the grid specify how many skyscrapers are visible from that direction.
Grid content: 0 represents blank cells to be filled with skyscraper heights.

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
