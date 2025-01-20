#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>
#include <vector>

constexpr int MEMORY_SIZE {30000};

class BrainFuck {
    private:
        enum OPCODE { UPDATEVAL, SHIFTPTR, OUTPUT, INPUT, LSTART, LEND };
        std::array<std::uint8_t, MEMORY_SIZE> memory {0};
        std::size_t ptr {0};

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
                            std::cerr << "Syntax error. Unexpected closing bracket in line " 
                                      << lineNo << " char " << i + 1 << "\n";
                        }

                        return false;
                    }
                }
            }

            if (!loopStk.empty() && showErr) {
                std::cerr << "Syntax error. Unclosed bracket in line " 
                          << loopStk.top().first << " char " << loopStk.top().second << "\n";
            }

            return loopStk.empty();
        }

        // Execute raw string instruction - assumes code is already validated
        void executeRaw(const std::string &code) {
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
                        if (memory[ptr] == 0) loopStk.pop();
                        else pos = loopStk.top();
                        break;
                }

                // Go to the next instruction
                pos++;
            }
        }

        void executeByteCode(std::string &fname) {
            std::ifstream ifs {fname, std::ios::binary};
            if (!ifs) { std::cerr << "Unable to open file for reading bytecode.\n"; std::exit(1); }
            std::vector<std::pair<OPCODE, long>> instructions;
            while (ifs) {
                OPCODE opcode; long val;
                ifs.read(reinterpret_cast<char*>(&opcode), sizeof(OPCODE));
                ifs.read(reinterpret_cast<char*>(&val), sizeof(long));

                // Execute instruction
            }
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

        static void compile2ByteCode(const std::string &fname) {
            std::ifstream ifs{fname};
            if (!ifs) { std::cerr << "No such file or directory.\n"; std::exit(1); }

            // Read the file as a raw string
            std::ostringstream oss; 
            oss << ifs.rdbuf();
            std::string code {oss.str()};

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
                    instructions.push_back({currOpCode, 0l});
                } else if (ch == '[') {
                    stk.push(instructions.size());
                    instructions.push_back({OPCODE::LSTART, -1});
                } else if (ch == ']') {
                    std::size_t loopStartPos {stk.top()}; stk.pop();
                    instructions[loopStartPos].second = static_cast<long>(instructions.size());
                    instructions.push_back({OPCODE::LEND, static_cast<long>(loopStartPos)});
                }
            }

            // Save the instruction set
            std::ofstream ofs{fname + ".bfc", std::ios::binary};
            if (!ofs) { std::cerr << "Unable to open file for writing bytecode.\n"; std::exit(1); }
            for (const std::pair<OPCODE, long> &instruction: instructions) {
                ofs.write(reinterpret_cast<const char*>(&instruction.first), sizeof(OPCODE));
                ofs.write(reinterpret_cast<const char*>(&instruction.second), sizeof(long));
            }
        }

        void executeFile(const char* fname_) {
            std::string fname {fname_};
            // Not a brainfuck compiled bytecode, compile it
            if (!fname.ends_with(".bfc")) {
                compile2ByteCode(fname);
                fname += ".bfc";
            }

            // Execute the compiled code
            executeByteCode(fname);
        }
};

int main(int argc, char **argv) {
    BrainFuck bf;
    if (argc >= 3)
        std::cout << "Usage: brainfuck [<script.bf>]\n";
    else if (argc == 1)
        bf.shell();        
    else if (argc == 2)
        bf.executeFile(argv[1]);

    return 0;
}
