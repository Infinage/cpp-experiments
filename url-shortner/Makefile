# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++23 -I include

# Directories
SRC_DIR = src
BUILD_DIR = build
OUTPUT = $(BUILD_DIR)/app

# Source files
SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)

# Targets
all: release

# Debug build
debug: CXXFLAGS += -ggdb -O0
debug: $(OUTPUT).debug

$(OUTPUT).debug: $(SRC_FILES) main.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Release build
release: CXXFLAGS += -O2 -static
release: $(OUTPUT)

$(OUTPUT): $(SRC_FILES) main.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all debug release clean
