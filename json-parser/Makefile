# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -O2 -Wall -Wextra

# Source files 
JSON_SRC = json.hpp
VALIDATE_SRC = validate-json.cpp
CREATE_SRC = create-json.cpp
MANIPULATE_SRC = manipulate-json.cpp

# Build dir & executable paths
BUILD_DIR = build
VALIDATE_BIN = $(BUILD_DIR)/validate-json.out
CREATE_BIN = $(BUILD_DIR)/create-json.out
MANIPULATE_BIN = $(BUILD_DIR)/manipulate-json.out

# Target to build both binaries
all: $(VALIDATE_BIN) $(CREATE_BIN) $(MANIPULATE_BIN)

# Target to build validate-json
$(VALIDATE_BIN): $(VALIDATE_SRC) $(JSON_SRC)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(VALIDATE_SRC) -o $(VALIDATE_BIN)

# Target to build create-json
$(CREATE_BIN): $(CREATE_SRC) $(JSON_SRC)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(CREATE_SRC) -o $(CREATE_BIN)

# Target to build manipulate-json
$(MANIPULATE_BIN): $(MANIPULATE_SRC) $(JSON_SRC)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(MANIPULATE_SRC) -o $(MANIPULATE_BIN)

# Target to run tests using validate-json
test: $(VALIDATE_BIN)
	@echo "Running tests with validate-json..."
	./${BUILD_DIR}/validate-json.out test

# Clean build directory
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all run clean
