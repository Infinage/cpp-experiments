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
	$(CXX) $<  $(CXXFLAGS) -o $@ $(EXTRA-CONFIGS)
	@echo "Executing $@.."
	@./$@

minesweeper.out: EXTRA-CONFIGS = -L/home/kael/cpplib/lib -I/home/kael/cpplib/include -lftxui-component -lftxui-dom  -lftxui-screen

# Clean up the project
clean:
	rm -f $(OUTPUTS)

.PHONY: all run clean
