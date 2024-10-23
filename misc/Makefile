# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra

# Debug and Release flags
DEBUG_FLAGS = -ggdb
RELEASE_FLAGS = -O2

# Additional configuration for minesweeper
MINESWEEPER_CONFIGS = -L/home/kael/cpplib/lib -I/home/kael/cpplib/include -lftxui-component -lftxui-dom -lftxui-screen

# Find source files
SOURCES = fc.cpp minesweeper.cpp

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

# Compile fc.cpp
$(BUILD_DIR)/fc.debug: fc.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) -o $@

$(BUILD_DIR)/fc.out: fc.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Compile minesweeper.cpp with FTXUI dependencies
$(BUILD_DIR)/minesweeper.debug: minesweeper.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) $(MINESWEEPER_CONFIGS) -o $@

$(BUILD_DIR)/minesweeper.out: minesweeper.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) $(MINESWEEPER_CONFIGS) -o $@

# Clean up the project
clean:
	rm -f build/*

.PHONY: all build-debug build-release clean
