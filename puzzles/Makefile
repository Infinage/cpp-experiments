# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra

# Debug and Release flags
DEBUG_FLAGS = -ggdb
RELEASE_FLAGS = -O2

# Find source files
SOURCES = $(wildcard *.cpp)

# Output for both debug and release
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

# General rule for other .cpp files without FTXUI dependencies
$(BUILD_DIR)/%.debug: %.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) -o $@

$(BUILD_DIR)/%.out: %.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Clean up the project
clean:
	rm -f build/*

.PHONY: all build-debug build-release clean
