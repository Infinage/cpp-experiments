# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -lsfml-graphics -lsfml-window -lsfml-system

# Directories
SRC_DIR = src
BIN_DIR = bin

# Files
MAIN_SRC = main.cpp
PROD_OBJS = $(wildcard $(SRC_DIR)/*.cpp)

# Targets
PROD_EXEC = $(BIN_DIR)/pacman
PROD_EXEC_DEBUG = $(BIN_DIR)/pacman_debug

# Build types
DEBUG_FLAGS = -g -ggdb
RELEASE_FLAGS = -O2

# Rules
all: release

release: $(PROD_EXEC)

debug: $(PROD_EXEC_DEBUG)

$(BIN_DIR):
	mkdir -p $@

# Releas target
$(PROD_EXEC): $(MAIN_SRC) $(PROD_OBJS) | $(BIN_DIR)
	$(CXX) $^ $(CXXFLAGS) $(RELEASE_FLAGS) -o $@

# Debug target
$(PROD_EXEC_DEBUG): $(MAIN_SRC) $(PROD_OBJS) | $(BIN_DIR)
	$(CXX) $^ $(CXXFLAGS) $(DEBUG_FLAGS) -o $@

clean:
	rm -rf $(BIN_DIR)

.PHONY: all release clean
