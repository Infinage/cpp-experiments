# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra

# Debug and Release flags
DEBUG_FLAGS = -ggdb
RELEASE_FLAGS = -O2

# Find source files
SOURCES = fc.cpp cdiff.cpp spellcheck.cpp cxxd.cpp

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

# Compile fc
$(BUILD_DIR)/fc.debug: fc.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) -o $@
$(BUILD_DIR)/fc.out: fc.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Compile spellcheck
$(BUILD_DIR)/spellcheck.debug: spellcheck.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) -o $@
$(BUILD_DIR)/spellcheck.out: spellcheck.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Compile cdiff
$(BUILD_DIR)/cdiff.debug: cdiff.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) -o $@
$(BUILD_DIR)/cdiff.out: cdiff.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Compile cxxd
$(BUILD_DIR)/cxxd.debug: cxxd.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(DEBUG_FLAGS) -o $@
$(BUILD_DIR)/cxxd.out: cxxd.cpp | $(BUILD_DIR)
	$(CXX) $< $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Clean up the project
clean:
	rm -f build/*

.PHONY: all build-debug build-release clean
