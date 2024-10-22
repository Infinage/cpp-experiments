# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++23 -ggdb -Wall -Wextra

# Find all .cpp files
SOURCES = $(wildcard *.cpp)

# Create output file names by replacing .cpp with .out
OUTPUTS = $(SOURCES:.cpp=.out)

# Targets
all: $(OUTPUTS)

%.out: %.cpp
	$(CXX) $<  $(CXXFLAGS) -o "build/$@" $(EXTRA-CONFIGS)
	@echo "Executing $@.."
	@./build/$@

# Clean up the project
clean:
	rm -f build/*

.PHONY: all run clean
