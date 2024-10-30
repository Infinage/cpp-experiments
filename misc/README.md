# Project Overview

This `misc` folder contains various experimental C++ projects and snippets that are currently uncategorized within the main repository. Once enough projects follow a similar theme or pattern, they may be moved into their own organized folders.

## minesweeper

A CLI interactive Minesweeper game. You can start the game by specifying the number of rows and columns.

### Usage

Run the program with the number of rows and columns as arguments:

```bash
./build/minesweeper.out <rows> <cols>
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
