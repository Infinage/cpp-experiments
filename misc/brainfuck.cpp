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
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/*
  Implement Opcodes for:
  1. [-]: Clear cell
  2. [>]: Find first zeroed cell to right
  3. [<]: Find first zeroed cell to left
  4. [>+8<-]: Copy values over with an optional factor, optionally support copying over to multiple cells
 */

constexpr std::size_t MEMORY_SIZE {30000};

class BrainFuck {
    private:
        static std::unordered_set<char> BRAINFUCK_CHARS;
        enum OPCODE { 
            UPDATEVAL, SHIFTPTR, OUTPUT, INPUT, LSTART, LEND, 
            CLEAR, SHIFTPTR_ZERO, UPDATE_BY_CURR
        };

        using INSTRUCTION_T = std::tuple<OPCODE, long, long>;

        std::array<std::uint8_t, MEMORY_SIZE> memory {0};
        std::size_t ptr {0};

        struct HashPair {
            std::size_t operator() (const std::pair<std::size_t, std::size_t> &p) const {
                std::hash<std::size_t> Hash;
                return Hash(p.first) ^ (Hash(p.second) << 1);
            }
        };

        template <typename T>
        static T mod(T val, T modulus) {
            return (val % modulus + modulus) % modulus;
        }

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
        void executeRaw(const std::string &code, const std::string &logFName = "") {
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
                    loopCounterVec.emplace_back(kv.first, kv.second);
                
                // Sort the vector
                std::sort(loopCounterVec.begin(), loopCounterVec.end(), []
                    (std::pair<std::string, std::size_t> &v1, std::pair<std::string, std::size_t> &v2) {
                        return v1.second > v2.second;
                    }
                );

                // Write the profile output
                std::ofstream log {logFName};
                if (!log) { std::cerr << "Error: Unable to open file for logging.\n"; std::exit(1); }
                for (std::pair<std::string, std::size_t> &v: loopCounterVec)
                    log << v.first << "," << v.second << "\n";
            }
        }

        // Execute bytecode (binary file, extension is not explicity checked)
        void executeByteCode(const std::string &fname) {
            std::ifstream ifs {fname, std::ios::binary};
            if (!ifs) { std::cerr << "Error: Unable to open file for reading bytecode.\n"; std::exit(1); }
            std::vector<INSTRUCTION_T> instructions;
            OPCODE opcode; long val1, val2;
            while (ifs.read(reinterpret_cast<char*>(&opcode), sizeof(OPCODE)) 
                && ifs.read(reinterpret_cast<char*>(  &val1), sizeof(  long))
                && ifs.read(reinterpret_cast<char*>(  &val2), sizeof(  long))) {
                instructions.emplace_back(opcode, val1, val2);
            }

            // Execute bytecode, profile loops if logFName is set
            for (std::size_t pos {0}; pos < instructions.size(); pos++) {
                std::tie(opcode, val1, val2) = instructions[pos];
                switch (opcode) {

                    case OPCODE::UPDATEVAL:
                        memory[ptr] = static_cast<std::uint8_t>(mod(static_cast<long>(memory[ptr]) + val1, 255l));
                        break;

                    case OPCODE::SHIFTPTR:
                        ptr = static_cast<std::size_t>(mod(static_cast<long>(ptr) + val1, static_cast<long>(MEMORY_SIZE)));
                        break;

                    case OPCODE::INPUT:
                        memory[ptr] = static_cast<std::uint8_t>(std::getchar());
                        break;

                    case OPCODE::OUTPUT:
                        std::putchar(memory[ptr]);
                        break;

                    case OPCODE::LSTART:
                        pos = memory[ptr] == 0? static_cast<std::size_t>(val1): pos;
                        break;

                    case OPCODE::LEND:
                        pos = memory[ptr] == 0? pos: static_cast<std::size_t>(val1);
                        break;

                    case OPCODE::CLEAR:
                        memory[ptr]  = 0;
                        break;

                    case OPCODE::SHIFTPTR_ZERO:
                        while(memory[ptr] != 0) 
                            ptr = static_cast<std::size_t>(mod(static_cast<long>(ptr) + val1, static_cast<long>(MEMORY_SIZE)));
                        break;

                    case OPCODE::UPDATE_BY_CURR: {
                        // Where the destination position is
                        std::size_t copyTo {static_cast<std::size_t>(mod(static_cast<long>(ptr) + val1, static_cast<long>(MEMORY_SIZE)))};

                        // Updated val to write
                        long updatedVal {static_cast<long>(memory[copyTo])};
                        updatedVal += val2 * static_cast<long>(memory[ptr]);
                        updatedVal = mod(updatedVal, 255l);

                        // Update val at destination
                        memory[copyTo] = static_cast<std::uint8_t>(updatedVal);
                    } break;

                }
            }
        }

        /*
         * - Check if starting and ending points are the same
         * - Loop is non nested without any compound op codes
         * - We decrement startPos memory by 1 
         * - For eg: [<3+2<1+>4-]
         */
        static bool validateSimpleDistributionLoop(
            std::size_t start, std::size_t end, 
            std::vector<INSTRUCTION_T> &instructions,
            std::unordered_map<long, long> &distributeTo
        ) {
            // Keep track of curr relative pos
            long shift {0};
            for (std::size_t pos {start + 1}; pos < end; pos++) {
                OPCODE opcode; long val1, val2; 
                std::tie(opcode, val1, val2) = instructions[pos];

                // Move curr pointer
                if (opcode == OPCODE::SHIFTPTR) shift += val1;

                // Update value by a factor
                else if (opcode == OPCODE::UPDATEVAL) distributeTo[shift] += val1;

                // Disallow any other opcode
                else return false;
            }

            // Check if we are back to where we start and 
            // we are doing a simple decrement
            bool status {shift == 0 && distributeTo[0] == -1};
            return status;
        }

        // Compiles the raw code to bytecode - ".bfc" is appended to filename
        static void compile2ByteCode(const std::string &fname) {
            // Read as raw file
            std::string code {readRawFile(fname)};

            // Validate the code and exit if not valid
            if (!validate(code)) std::exit(1);

            // Process and write the instructions to bytecode
            std::stack<std::size_t> stk;
            std::vector<INSTRUCTION_T> instructions;
            for (const char ch: code) {
                if (ch == '+' || ch == '-' || ch == '>' || ch == '<') {
                    long   shiftVal {ch == '+' || ch == '>'? 1l: -1l};
                    OPCODE currOpCode {ch == '+' || ch == '-'? OPCODE::UPDATEVAL: OPCODE::SHIFTPTR};
                    if (!instructions.empty() && std::get<0>(instructions.back()) == currOpCode)
                        std::get<1>(instructions.back()) += shiftVal;
                    else
                        instructions.emplace_back(currOpCode, shiftVal, 0l);
                } else if (ch == '.' || ch == ',') {
                    OPCODE currOpCode {ch == '.'? OPCODE::OUTPUT: OPCODE::INPUT};
                    instructions.emplace_back(currOpCode, 1l, 0l);
                } else if (ch == '[') {
                    stk.push(instructions.size());
                    instructions.emplace_back(OPCODE::LSTART, -1, 0l);
                } else if (ch == ']') {
                    std::size_t loopStartPos {stk.top()}, loopEndPos {instructions.size()}; stk.pop();
                    std::get<1>(instructions[loopStartPos]) = static_cast<long>(loopEndPos);
                    instructions.emplace_back(OPCODE::LEND, static_cast<long>(loopStartPos), 0l);

                    // When we are processing simple distribution 
                    // loops, we would make use of this guy
                    std::unordered_map<long, long> distributeTo;

                    // If loop contains only UPDATEVAL, it is a clear
                    if (loopEndPos - loopStartPos == 2 && std::get<0>(instructions[loopStartPos + 1]) == OPCODE::UPDATEVAL) {
                        instructions.pop_back(); instructions.pop_back(); instructions.pop_back();
                        instructions.emplace_back(OPCODE::CLEAR, 1l, 0l);
                    }

                    // If loop contains only SHIFTPTR, it is shift until nonzero
                    else if (loopEndPos - loopStartPos == 2 && std::get<0>(instructions[loopStartPos + 1]) == OPCODE::SHIFTPTR) {
                        instructions.pop_back(); instructions.pop_back(); instructions.pop_back();
                        instructions.emplace_back(OPCODE::SHIFTPTR_ZERO, std::get<1>(instructions[loopStartPos + 1]), 0l);
                    }

                    // For eg: [<3+2<1+>4-]
                    else if (validateSimpleDistributionLoop(loopStartPos, loopEndPos, instructions, distributeTo)) {
                        // Remove all instructions inside the loop
                        while (instructions.size() > loopStartPos) 
                            instructions.pop_back();

                        // Insert INCR_BY_CURR, DECR_BY_CURR opcodes
                        for (const std::pair<const long, long> &kv: distributeTo) {
                            long shift, factor;
                            std::tie(shift, factor) = kv;
                            if (factor != 0 && shift != 0)
                                instructions.emplace_back(OPCODE::UPDATE_BY_CURR, shift, factor);
                        }

                        // Clear curr cell
                        instructions.emplace_back(OPCODE::CLEAR, 1l, 0l);
                    }

                }
            }

            // Save the instruction set
            std::ofstream ofs{fname + ".bfc", std::ios::binary};
            if (!ofs) { std::cerr << "Error: Unable to open file for writing bytecode.\n"; std::exit(1); }
            for (const INSTRUCTION_T &instruction: instructions) {
                ofs.write(reinterpret_cast<const char*>(&std::get<0>(instruction)), sizeof(OPCODE));
                ofs.write(reinterpret_cast<const char*>(&std::get<1>(instruction)), sizeof(long));
                ofs.write(reinterpret_cast<const char*>(&std::get<2>(instruction)), sizeof(long));
            }
        }

        void _logByteCodeInstructions(std::vector<INSTRUCTION_T> &instructions, const std::string &fname) {
            std::ofstream ofs {fname};
            if (!ofs) { std::cerr << "Error: Unable to open file for writing.\n"; std::exit(1); }
            for (INSTRUCTION_T &instruction: instructions)
                ofs << std::get<0>(instruction) << "," << std::get<1>(instruction) << "\n";
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
                if (profileFlag) { std::cerr << "Error: Can't profile a Bytecode.\n"; std::exit(1); }
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
