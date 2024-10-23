# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra

# Debug and Release flags
DEBUG_FLAGS = -ggdb
RELEASE_FLAGS = -O2

# Find all .cpp files
SOURCES = $(wildcard *.cpp)

# Create output file names for both .out (release) and .debug (debug)
DEBUG_OUTPUTS = $(patsubst %.cpp, build/%.debug, $(SOURCES))
RELEASE_OUTPUTS = $(patsubst %.cpp, build/%.out, $(SOURCES))

# Create the build directory if it doesn't exist
BUILD_DIR = build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Targets
all: build-debug build-release

# Build all .cpp files as debug
build-debug: $(BUILD_DIR) $(DEBUG_OUTPUTS)

# Build all .cpp files as release
build-release: $(BUILD_DIR) $(RELEASE_OUTPUTS)

# Compile each .cpp file into a debug executable
$(BUILD_DIR)/%.debug: %.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) -o $@

# Compile each .cpp file into a release executable
$(BUILD_DIR)/%.out: %.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Clean up the project
clean:
	rm -f build/*

.PHONY: all build-debug build-release clean
