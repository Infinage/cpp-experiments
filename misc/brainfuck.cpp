#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*
  Implement Opcodes for:
  1. [-]: Clear cell
  2. [>]: Find first zeroed cell to right
  3. [<]: Find first zeroed cell to left
  4. [>+8<-]: Copy values over with an optional factor
 */

constexpr int MEMORY_SIZE {30000};

class BrainFuck {
    private:
        enum OPCODE { UPDATEVAL, SHIFTPTR, OUTPUT, INPUT, LSTART, LEND, CLEAR, SHIFTPTR_NONZERO };
        static std::unordered_set<char> BRAINFUCK_CHARS;
        std::array<std::uint8_t, MEMORY_SIZE> memory {0};
        std::size_t ptr {0};

        struct HashPair {
            std::size_t operator() (const std::pair<std::size_t, std::size_t> &p) const {
                std::hash<std::size_t> Hash;
                return Hash(p.first) ^ (Hash(p.second) << 1);
            }
        };

        // Validate and print an error message at point of error
        static bool validate(const std::string &code, bool showErr = true) {
            std::size_t lineNo {1};
            std::stack<std::pair<std::size_t, std::size_t>> loopStk;
            for (std::size_t i {0}; i < code.size(); i++) {
                if (code[i] == '\n') lineNo++;
                else if (code[i] == '[')
                    loopStk.push({lineNo, i + 1});
                else if (code[i] == ']') {
                    if (!loopStk.empty()) loopStk.pop();
                    else {
                        if (showErr) {
                            std::cerr << "Error: Unexpected closing bracket in line " 
                                      << lineNo << " char " << i + 1 << "\n";
                        }

                        return false;
                    }
                }
            }

            if (!loopStk.empty() && showErr) {
                std::cerr << "Error: Unclosed bracket in line " 
                          << loopStk.top().first << " char " << loopStk.top().second << "\n";
            }

            return loopStk.empty();
        }

        // Read file as raw text
        static std::string readRawFile(const std::string &fname) {
            std::ifstream ifs {fname};
            if (!ifs) { std::cerr << "Error: Unable to open file.\n"; std::exit(1); }
            std::ostringstream oss; 
            oss << ifs.rdbuf();
            return oss.str();
        }

        // Extract substr excluding comments, compress if applicable
        static std::string getLoopRepr(std::size_t start, std::size_t end, const std::string& code) {
            std::ostringstream oss;
            std::pair<char, std::size_t> curr{'\0', 0};
            for (std::size_t pos {start}; pos <= end; pos++) {
                char ch {code.at(pos)}; 
                if (BRAINFUCK_CHARS.find(ch) == BRAINFUCK_CHARS.end()) 
                    continue;
                else if (curr.first == ch && ch != '[' && ch != ']' && ch != '.' && ch != ',') 
                    curr.second++;
                else {
                    if (curr.first != '\0') oss << curr.first << (curr.second > 1? std::to_string(curr.second): "");
                    curr = {ch, 1};
                } 
            }

            oss << curr.first << (curr.second > 1? std::to_string(curr.second): "");
            return oss.str();
        }

        // Execute raw string instruction - assumes code is already validated
        void executeRaw(const std::string &code, const std::string logFName = "") {
            // Profiling loops with a counter
            std::unordered_map<std::pair<std::size_t, std::size_t>, std::size_t, HashPair> loopCounter;
            bool logOutput {!logFName.empty()};

            std::size_t pos {0};
            std::stack<std::size_t> loopStk;
            while (pos < code.size()) {
                char ch  {code.at(pos)};
                switch (ch) {
                    case '+':
                        memory[ptr] = memory[ptr] == 255? 0: memory[ptr] + 1;
                        break;
                    case '-':
                        memory[ptr] = memory[ptr] == 0? 255: memory[ptr] - 1;
                        break;
                    case '>':
                        ptr = ptr == MEMORY_SIZE - 1? 0: ptr + 1;
                        break;
                    case '<':
                        ptr = ptr == 0? MEMORY_SIZE - 1: ptr - 1;
                        break;
                    case '.':
                        std::putchar(memory[ptr]);
                        break;
                    case ',':
                        memory[ptr] = static_cast<std::uint8_t>(std::getchar());
                        break;
                    case '[':
                        if (memory[ptr] != 0) loopStk.push(pos);
                        else {
                            int openBrackets {1};
                            while (openBrackets > 0) {
                                ++pos;
                                if (code.at(pos) == ']') openBrackets--;
                                else if (code.at(pos) == '[') openBrackets++;
                            }
                        }
                        break;
                    case ']':
                        if (logOutput) loopCounter[{loopStk.top(), pos}]++;
                        if (memory[ptr] == 0) loopStk.pop();
                        else pos = loopStk.top();
                        break;
                }

                // Go to the next instruction
                pos++;
            }

            // Output loop profiling output
            if (logOutput) {
                // Get the loops string repr into a map
                std::unordered_map<std::string, std::size_t> loopCounterStrRep;
                for (const std::pair<const std::pair<std::size_t, std::size_t>, std::size_t> &kv: loopCounter)
                    loopCounterStrRep[getLoopRepr(kv.first.first, kv.first.second, code)] += kv.second;

                // Write to a vector for sorting
                std::vector<std::pair<std::string, std::size_t>> loopCounterVec;
                for (const std::pair<const std::string, std::size_t> &kv: loopCounterStrRep)
                    loopCounterVec.push_back({kv.first, kv.second});
                
                // Sort the vector
                std::sort(loopCounterVec.begin(), loopCounterVec.end(), []
                    (std::pair<std::string, std::size_t> &v1, std::pair<std::string, std::size_t> &v2) {
                        return v1.second > v2.second;
                    }
                );

                // Write the profile output
                std::ofstream log {logFName};
                if (!log) { std::cerr << "Error: Unable to open file for logging.\n"; std::exit(1); }
                log << '\n' + std::string('-', 20) + '\n';
                for (std::pair<std::string, std::size_t> &v: loopCounterVec)
                    log << v.first << "," << v.second << "\n";
            }
        }

        // Execute bytecode (binary file, extension is not explicity checked)
        void executeByteCode(const std::string &fname) {
            std::ifstream ifs {fname, std::ios::binary};
            if (!ifs) { std::cerr << "Error: Unable to open file for reading bytecode.\n"; std::exit(1); }
            std::vector<std::pair<OPCODE, long>> instructions;
            OPCODE opcode; long val;
            while (ifs.read(reinterpret_cast<char*>(&opcode), sizeof(OPCODE)) 
                && ifs.read(reinterpret_cast<char*>(   &val), sizeof(  long))) {
                instructions.push_back({opcode, val});
            }

            // Execute bytecode, profile loops if logFName is set
            for (std::size_t pos {0}; pos < instructions.size(); pos++) {
                std::tie(opcode, val) = instructions[pos];
                switch (opcode) {
                    case OPCODE::UPDATEVAL:
                        memory[ptr] = static_cast<std::uint8_t>(((val + memory[ptr]) % 255 + 255) % 255);
                        break;
                    case OPCODE::SHIFTPTR:
                        ptr = static_cast<std::size_t>(((static_cast<long>(ptr) + val) % MEMORY_SIZE + MEMORY_SIZE) % MEMORY_SIZE);
                        break;
                    case OPCODE::INPUT:
                        memory[ptr] = static_cast<std::uint8_t>(std::getchar());
                        break;
                    case OPCODE::OUTPUT:
                        std::putchar(memory[ptr]);
                        break;
                    case OPCODE::LSTART:
                        pos = memory[ptr] == 0? static_cast<std::size_t>(val): pos;
                        break;
                    case OPCODE::LEND:
                        pos = memory[ptr] == 0? pos: static_cast<std::size_t>(val);
                        break;
                    case OPCODE::CLEAR:
                        memory[ptr]  = 0;
                        break;
                    case OPCODE::SHIFTPTR_NONZERO:
                        while(memory[ptr] != 0) 
                            ptr = static_cast<std::size_t>(((static_cast<long>(ptr) + val) % MEMORY_SIZE + MEMORY_SIZE) % MEMORY_SIZE);
                        break;
                }
            }
        }

        // Compiles the raw code to bytecode - ".bfc" is appended to filename
        static void compile2ByteCode(const std::string &fname) {
            // Read as raw file
            std::string code {readRawFile(fname)};

            // Validate the code and exit if not valid
            if (!validate(code)) std::exit(1);

            // Process and write the instructions to bytecode
            std::stack<std::size_t> stk;
            std::vector<std::pair<OPCODE, long>> instructions;
            for (const char ch: code) {
                if (ch == '+' || ch == '-' || ch == '>' || ch == '<') {
                    long   shiftVal {ch == '+' || ch == '>'? 1l: -1l};
                    OPCODE currOpCode {ch == '+' || ch == '-'? OPCODE::UPDATEVAL: OPCODE::SHIFTPTR};
                    if (!instructions.empty() && instructions.back().first == currOpCode)
                        instructions.back().second += shiftVal;
                    else
                        instructions.push_back({currOpCode, shiftVal});
                } else if (ch == '.' || ch == ',') {
                    OPCODE currOpCode {ch == '.'? OPCODE::OUTPUT: OPCODE::INPUT};
                    instructions.push_back({currOpCode, 1l});
                } else if (ch == '[') {
                    stk.push(instructions.size());
                    instructions.push_back({OPCODE::LSTART, -1});
                } else if (ch == ']') {
                    std::size_t loopStartPos {stk.top()}, loopEndPos {instructions.size()}; stk.pop();
                    instructions[loopStartPos].second = static_cast<long>(loopEndPos);
                    instructions.push_back({OPCODE::LEND, static_cast<long>(loopStartPos)});

                    // If loop contains only UPDATEVAL, it is a clear
                    if (loopEndPos - loopStartPos == 2 && instructions[loopStartPos + 1].first == OPCODE::UPDATEVAL) {
                        instructions.pop_back(); instructions.pop_back(); instructions.pop_back();
                        instructions.push_back({OPCODE::CLEAR, 1l});
                    }

                    // If loop contains only SHIFTPTR, it is shift until nonzero
                    else if (loopEndPos - loopStartPos == 2 && instructions[loopStartPos + 1].first == OPCODE::SHIFTPTR) {
                        instructions.pop_back(); instructions.pop_back(); instructions.pop_back();
                        instructions.push_back({OPCODE::SHIFTPTR_NONZERO, instructions[loopStartPos + 1].second});
                    }
                }
            }

            // Save the instruction set
            std::ofstream ofs{fname + ".bfc", std::ios::binary};
            if (!ofs) { std::cerr << "Error: Unable to open file for writing bytecode.\n"; std::exit(1); }
            for (const std::pair<OPCODE, long> &instruction: instructions) {
                ofs.write(reinterpret_cast<const char*>(&instruction.first), sizeof(OPCODE));
                ofs.write(reinterpret_cast<const char*>(&instruction.second), sizeof(long));
            }
        }

        void _logByteCodeInstructions(std::vector<std::pair<OPCODE, long>> &instructions, const std::string &fname) {
            std::ofstream ofs {fname};
            if (!ofs) { std::cerr << "Error: Unable to open file for writing.\n"; std::exit(1); }
            for (std::pair<OPCODE, long> &instruction: instructions)
                ofs << instruction.first << "," << instruction.second << "\n";
        }

    public:
        void shell() {
            std::cout << "Brainfuck Interpreter. Hit Ctrl+C to exit.\n";
            std::string line;
            while (1) {
                std::cout << "BF> ";
                if (!std::getline(std::cin, line)) { 
                    std::cout << "\n"; break; 
                } else if (validate(line, true)) {
                    executeRaw(line);
                }
            }
        }

        void executeFile(std::string &fname, bool profileFlag) {
            // Fail if file is missing
            if (!std::filesystem::exists(fname) || !std::filesystem::is_regular_file(fname)) {
                std::cerr << "Error: No such file: " << fname << "\n";
                std::exit(1);
            }

            // Already compiled
            else if (fname.ends_with(".bfc")) {
                if (profileFlag) { std::cerr << "Error: Can't profile a compiled binary.\n"; std::exit(1); }
                executeByteCode(fname);
            }

            // Execute Raw if not specified
            else if (profileFlag) {
                std::string code {readRawFile(fname)};
                if (validate(code)) executeRaw(code, fname + ".log");
            }

            // Compile and execute
            else {
                compile2ByteCode(fname);
                executeByteCode(fname + ".bfc");
            }
        }
};

// Init static variables
std::unordered_set<char> BrainFuck::BRAINFUCK_CHARS {'+', '-', '>', '<', ',', '.', '[', ']'};

bool hasOpt(const std::string &arg, std::vector<std::string> &args) {
    return std::find(args.begin(), args.end(), arg) != args.end();
}

int main(int argc, char **argv) {
    BrainFuck bf;

    std::vector<std::string> args {argv, argv + argc};
    if (hasOpt("-h", args) || hasOpt("--help", args))
        std::cout << "Usage: brainfuck [OPTIONS] [<script.bf>]\n";
    else if (args.size() == 1)
        bf.shell();        
    else {
        bool profileFlag {hasOpt("-p", args) || hasOpt("--profile", args)};
        bf.executeFile(args.back(), profileFlag);
    }

    return 0;
}
