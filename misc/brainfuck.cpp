#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stack>

constexpr int MEMORY_SIZE {30000};

class BrainFuck {
    private:
        std::array<std::uint8_t, MEMORY_SIZE> memory {0};
        std::stack<std::size_t> loopStk;
        std::size_t ptr {0};

        // Validate and print an error message at point of error
        static bool validate(const std::string &code, bool showErr = true) {
            std::size_t lineNo {1};
            std::stack<std::pair<std::size_t, std::size_t>> stk;
            for (std::size_t i {0}; i < code.size(); i++) {
                if (code[i] == '\n') lineNo++;
                else if (code[i] == '[')
                    stk.push({lineNo, i + 1});
                else if (code[i] == ']') {
                    if (!stk.empty()) stk.pop();
                    else {
                        if (showErr) {
                            std::cerr << "Syntax error. Unexpected closing bracket in line " 
                                      << lineNo << " char " << i + 1 << "\n";
                        }

                        return false;
                    }
                }
            }

            if (!stk.empty() && showErr) {
                std::cerr << "Syntax error. Unclosed bracket in line " 
                          << stk.top().first << " char " << stk.top().second << "\n";
            }

            return stk.empty();
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
                    evaluate(line);
                }
            }
        }

        void executeFile(const char* fname) {
            std::ifstream ifs{fname};
            if (!ifs) {
                std::cerr << "No such file or directory.\n";
                std::exit(1);
            }

            // Read the file to a string
            std::ostringstream oss; 
            oss << ifs.rdbuf();
            std::string code {oss.str()};

            // Execute if valid
            if (validate(code)) evaluate(code);
        }

        void evaluate(const std::string &code) {
            std::size_t pos {0};
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
