# C++ Cipher and Archive Cracker Project

## Overview

This project contains three programs:

1. `crack-archive.cpp`: A zip file password cracker using a dictionary attack with multi-threading.
2. `rot-cipher.cpp`: A ROT cipher implementation for encryption and decryption of text files.
3. `vignere-cipher.cpp`: A Vigenère cipher for both encryption and decryption using a user-provided key.

## Build Instructions

### Targets

- `make all`: Builds both debug and optimized release versions of all `.cpp` files.
- `make build-debug`: Builds all `.cpp` files as debug executables (with `-ggdb`).
- `make build-release`: Builds all `.cpp` files as optimized release executables (with `-O2`).
- `make clean`: Cleans all build outputs.
