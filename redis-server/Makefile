# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++23 -I include

# Directories
SRC_DIR = src
TEST_DIR = test
BIN_DIR = bin

# Files
MAIN_SRC = main.cpp
TEST_SRCS = $(wildcard $(TEST_DIR)/*.cpp)
PROD_OBJS = $(wildcard $(SRC_DIR)/*.cpp)

# Targets
PROD_EXEC = $(BIN_DIR)/server
PROD_EXEC_DEBUG = $(BIN_DIR)/server_debug
TEST_EXEC = $(patsubst $(TEST_DIR)/%.cpp, $(BIN_DIR)/%, $(TEST_SRCS))

# Build types
DEBUG_FLAGS = -g -ggdb -pg
RELEASE_FLAGS = -O2

# Rules
all: release

release: $(PROD_EXEC)

debug: $(PROD_EXEC_DEBUG)

test: $(TEST_EXEC)

$(BIN_DIR):
	mkdir -p $@

# Releas target
$(PROD_EXEC): $(MAIN_SRC) $(PROD_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(RELEASE_FLAGS) $^ -o $@

# Debug target
$(PROD_EXEC_DEBUG): $(MAIN_SRC) $(PROD_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $^ -o $@

# Test target
$(BIN_DIR)/%: $(TEST_DIR)/%.cpp $(PROD_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $^ -o $@
	@echo "Running $@..."
	@./$@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all release test clean
