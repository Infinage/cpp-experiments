# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -O2 -Wall -Wextra

# Source files 
JSON_SRC = json.hpp
VALIDATE_SRC = validate-json.cpp
EXAMPLE_SRC = example-document.cpp

# Build dir & executable paths
BUILD_DIR = build
VALIDATE_BIN = $(BUILD_DIR)/validate-json.out
EXAMPLE_BIN = $(BUILD_DIR)/example-document.out

# Target to build both binaries
all: $(VALIDATE_BIN) $(EXAMPLE_BIN)

# Target to build validate-json
$(VALIDATE_BIN): $(VALIDATE_SRC) $(JSON_SRC)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(VALIDATE_SRC) -o $(VALIDATE_BIN)

# Target to build example-document
$(EXAMPLE_BIN): $(EXAMPLE_SRC) $(JSON_SRC)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(EXAMPLE_SRC) -o $(EXAMPLE_BIN)

# Target to run tests using validate-json
test: $(VALIDATE_BIN)
	@echo "Running tests with validate-json..."
	./${BUILD_DIR}/validate-json.out test

# Clean build directory
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
