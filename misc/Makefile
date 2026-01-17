# TODO: convert to cmakelists and also add target for mandelbrot
# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra

# Debug and Release flags
DEBUG_FLAGS = -ggdb
RELEASE_FLAGS = -O2

# Additional configurations
MINESWEEPER_CONFIGS = -L/home/kael/cpplib/lib -I/home/kael/cpplib/include -lftxui-component -lftxui-dom -lftxui-screen
CJUDGE_CONFIGS = -lcap

# Find source files
SOURCES = minesweeper.cpp barcode.cpp brainfuck.cpp cjudge.cpp

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

# Compile minesweeper.cpp with FTXUI dependencies
$(BUILD_DIR)/minesweeper.debug: minesweeper.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) $(MINESWEEPER_CONFIGS) -o $@

$(BUILD_DIR)/minesweeper.out: minesweeper.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) $(MINESWEEPER_CONFIGS) -o $@

# Compile cjudge.cpp with capabilities dependencies
$(BUILD_DIR)/cjudge.debug: cjudge.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) $(CJUDGE_CONFIGS) -o $@

$(BUILD_DIR)/cjudge.out: cjudge.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) $(CJUDGE_CONFIGS) -o $@

# General rule for other .cpp files without extra dependencies
$(BUILD_DIR)/%.debug: %.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) -o $@

$(BUILD_DIR)/%.out: %.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Clean up the project
clean:
	rm -f build/*

.PHONY: all build-debug build-release clean
