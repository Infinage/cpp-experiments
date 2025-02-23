# Project Overview

This `misc` folder contains various experimental C++ projects and snippets that are currently uncategorized within the main repository. Once enough projects follow a similar theme or pattern, they may be moved into their own organized folders.

## Minesweeper

A CLI interactive Minesweeper game. You can start the game by specifying the number of rows and columns.

#### Usage

Run the program with the number of rows and columns as arguments:

```bash
./build/minesweeper.out <rows> <cols>
```

## Barcode Generator

A CLI-based Code 128 Barcode Generator that encodes messages into Code 128 barcodes and saves the output as a PBM file.

#### Usage

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

## Brainfuck CLI Tool

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

## CJudge

CJudge is a sandboxed code execution tool designed to run and evaluate binaries against predefined inputs and expected outputs, enforcing strict runtime and resource limits.  

#### **Usage**  
```bash
cjudge [--memory=256] [--time=0.5] [--nprocs=1] <binary_command> <questions_file> <answers_file>
```
- `--memory`: Maximum allowed memory in MB (default: 256MB).  
- `--time`: Execution time limit in seconds (default: 0.5s).  
- `--nprocs`: Number of allowed processes (default: 1). Increase if your program forks.  

#### **Features**  
- Sandboxed execution using Linux namespaces.  
- Prevents runaway processes with strict time and memory constraints.  
- Supports executing arbitrary binaries securely.  
- Runs as **non-root** to enforce resource limits.

## CSVSplit CLI Tool  

A fast and efficient tool for splitting, hashing, and merging CSV files.  

### **Usage**  
```bash
csvsplit <mode> <options> <file>  
```

### **Modes**  
- `rows <count> <file>`  
  Split CSV into chunks of at most `<count>` rows each.  
- `size <size> <file>`  
  Split CSV into chunks of approximately `<size>` MB.  
- `hash <colIdx> <buckets> <file>`  
  Hash column `<colIdx>` and distribute into `<buckets>` files.  
- `group <colIdx> <groupSize> <file>`  
  Assign unique values of `<colIdx>` into groups of `<groupSize>`.  
  If `<groupSize>` is 1, creates one file per unique value.  
- `revert <sync|async> <file1> <file2> ...`  
  Merge multiple CSV files back into one.  
  - `sync` maintains order.  
  - `async` merges in parallel without order guarantees.  
- `stat <file1> <file2> ...`  
  Asynchronously compute statistics for CSV files:  
  ```
  Lines    Rows    Columns    Size    Filename
  ```

### **Output Directory**  
- The output directory is set using the `CSVOUT` environment variable.  
- If `CSVOUT` is not set, outputs are created in the current working directory.  

### **Notes**  
- Assumes the CSV has a header, which is preserved in splits and ignored when merging.  
- `rows` and `size` are the most efficient modes.  
- `hash` is slightly less efficient but effective for large datasets.  
- `group` is the least efficient and not recommended for very large CSVs.  

## **Linear Regression - C++ Implementation**  

A simple C++ implementation of linear regression.

### **Features**  
- Supports **batch gradient descent** for training.  
- Handles **both single-variable and multi-variable regression**.  
- Provides **mean squared error (MSE) evaluation**.  
- Optimized **matrix operations** for speed.
- Python integration using `pybind11`.  

### **Usage**  
Compile as a shared library:  
```bash
g++ linear-regression.cpp -shared -o CPPLearn.so -fPIC -std=c++23 -O2 $(python3-config --includes --ldflags)
```
Use in Python (after compiling):  
```python
import CPPLearn
model = CPPLearn.LinearRegression()
model.fit(X, y, epochs=1000, learning_rate=0.01)
predictions = model.predict(X_test)
```

### **Notes**  
- Assumes input data is stored as **row-major order matrices**.  
- Requires `pybind11` for Python bindings.  
- Uses **constant learning rate**, no adaptive optimizers.  

## ThreadPool Helper

A thread pool implementation that allows enqueuing tasks to be executed by a pool of worker threads. This is a reusable utility for any project requiring multithreading.

## Build Instructions

To build the project, use the provided `Makefile`. It will create both debug and release versions of the executables.

#### Targets

- **build-debug**: Compiles the source files into debug executables located in the `build` directory.
- **build-release**: Compiles the source files into release executables located in the `build` directory.
- **clean**: Removes the compiled executables from the `build` directory.

#### Commands

You can use the following commands in your terminal:

```bash
make build-debug     # For debug builds
make build-release   # For release builds
make clean           # To clean up build files
```
