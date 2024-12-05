/*
 * Code 128 Implementation in CPP
 *
 * For a list of nonprintable ascii chars, refer: https://en.cppreference.com/w/cpp/language/ascii
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

class Barcode {
    private:
        constexpr static std::size_t INF {std::numeric_limits<int>::max()};

        const std::array<std::string, 107> patterns {{
            "11011001100", "11001101100", "11001100110", "10010011000", "10010001100",
            "10001001100", "10011001000", "10011000100", "10001100100", "11001001000",
            "11001000100", "11000100100", "10110011100", "10011011100", "10011001110",
            "10111001100", "10011101100", "10011100110", "11001110010", "11001011100",
            "11001001110", "11011100100", "11001110100", "11101101110", "11101001100",
            "11100101100", "11100100110", "11101100100", "11100110100", "11100110010",
            "11011011000", "11011000110", "11000110110", "10100011000", "10001011000",
            "10001000110", "10110001000", "10001101000", "10001100010", "11010001000",
            "11000101000", "11000100010", "10110111000", "10110001110", "10001101110",
            "10111011000", "10111000110", "10001110110", "11101110110", "11010001110",
            "11000101110", "11011101000", "11011100010", "11011101110", "11101011000",
            "11101000110", "11100010110", "11101101000", "11101100010", "11100011010",
            "11101111010", "11001000010", "11110001010", "10100110000", "10100001100",
            "10010110000", "10010000110", "10000101100", "10000100110", "10110010000",
            "10110000100", "10011010000", "10011000010", "10000110100", "10000110010",
            "11000010010", "11001010000", "11110111010", "11000010100", "10001111010",
            "10100111100", "10010111100", "10010011110", "10111100100", "10011110100",
            "10011110010", "11110100100", "11110010100", "11110010010", "11011011110",
            "11011110110", "11110110110", "10101111000", "10100011110", "10001011110",
            "10111101000", "10111100010", "11110101000", "11110100010", "10111011110",
            "10111101110", "11101011110", "11110101110",

            // START_Xs, STOP
            "11010000100", "11010010000", "11010011100", "1100011101011",
        }};

        const std::array<std::string, 103> Code128AChars {{
            " ", "!", "\"", "#", "$", "%", "&", "\'", "(", ")", "*", "+", ",", "-", ".", "/",
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?", "@",
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q",
            "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_", "\x00", "\x01",
            "\x02", "\x03", "\x04", "\x05", "\x06", "\x07", "\x08", "\x09", "\x0A", "\x0B", "\x0C", "\x0D", "\x0E",
            "\x0F", "\x10", "\x11", "\x12", "\x13", "\x14", "\x15", "\x16", "\x17", "\x18", "\x19", "\x1A",
            "\x1B", "\x1C", "\x1D", "\x1E", "\x1F", "FNC 3", "FNC 2", "Shift B", "Code C", "Code B",
            "FNC 4", "FNC 1"
        }};

        const std::array<std::string, 103> Code128BChars {{
            " ", "!", "\"", "#", "$", "%", "&", "\'", "(", ")", "*", "+", ",", "-", ".", "/",
            "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?", "@",
            "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q",
            "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_", "`", "a", "b",
            "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s",
            "t", "u", "v", "w", "x", "y", "z", "{", "|", "}", "~", "\x7F", "FNC 3", "FNC 2", "Shift A",
            "Code C", "FNC 4", "Code A", "FNC 1"
        }};

        const std::array<std::string, 103> Code128CChars {{
            "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12",
            "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25",
            "26", "27", "28", "29", "30", "31", "32", "33", "34", "35", "36", "37", "38",
            "39", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "50", "51",
            "52", "53", "54", "55", "56", "57", "58", "59", "60", "61", "62", "63", "64",
            "65", "66", "67", "68", "69", "70", "71", "72", "73", "74", "75", "76", "77",
            "78", "79", "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "90",
            "91", "92", "93", "94", "95", "96", "97", "98", "99", "Code B", "Code A", "FNC 1"
        }};

        // Indices containing to positions on 'patterns'
        const std::size_t START_A {103};
        const std::size_t START_B {104};
        const std::size_t START_C {105};
        const std::size_t STOP    {106};

        // For neatness, these would be initialized via a loop in the constructor
        std::unordered_map<std::string, std::size_t> CODE128A, CODE128B, CODE128C;

    public:

        Barcode() {
            for (std::size_t i{0}; i < Code128AChars.size(); i++) {
                CODE128A[Code128AChars[i]] = i;
                CODE128B[Code128BChars[i]] = i;
                CODE128C[Code128CChars[i]] = i;
            }
        }

        // Encode a message into a vector of bits
        std::vector<bool> encode(std::string &message) {

            // Compute DP grid for the message
            // DP[idx] denotes min 'keys' required to construct message[idx:]
            std::vector<std::vector<std::size_t>> dp(message.size(), std::vector<std::size_t>(5, INF));
            dp.push_back({0, 0, 0, 0, 0});
            for (std::size_t idx {message.size()}; idx-- > 0;) {
                std::string chStr{message[idx]};

                // If chStr is found in Code A set
                if (CODE128A.find(chStr) != CODE128A.end()) {
                    // Temporarily shift encoding for curr char only
                    dp[idx][4] = std::min(dp[idx][4], 2 + dp[idx + 1][1]);

                    // Change encoding - except when it is already 'A'
                    dp[idx][0] = std::min(dp[idx][0], 1 + dp[idx + 1][0]);
                    dp[idx][1] = std::min({dp[idx][1], 2 + dp[idx + 1][0], 2 + dp[idx + 1][4]});
                    dp[idx][2] = std::min(dp[idx][2], 2 + dp[idx + 1][0]);
                }

                // If chStr is found in Code B set
                if (CODE128B.find(chStr) != CODE128B.end()) {
                    // Temporarily shift encoding for curr char only
                    dp[idx][3] = std::min(dp[idx][3], 2 + dp[idx + 1][0]);

                    // Change encoding - except when it is already 'B'
                    dp[idx][0] = std::min({dp[idx][0], 2 + dp[idx + 1][1], 2 + dp[idx + 1][3]});
                    dp[idx][1] = std::min(dp[idx][1], 1 + dp[idx + 1][1]);
                    dp[idx][2] = std::min(dp[idx][2], 2 + dp[idx + 1][1]);
                }

                // Curr char is digit, next char is also digit (Code C)
                if (std::isdigit(chStr[0]) && idx < message.size() - 1 && std::isdigit(message[idx + 1])) {
                    // Change encoding - except when it is already 'C'
                    dp[idx][0] = std::min(dp[idx][0], 2 + dp[idx + 2][2]);
                    dp[idx][1] = std::min(dp[idx][1], 2 + dp[idx + 2][2]);
                    dp[idx][2] = std::min(dp[idx][2], 1 + dp[idx + 2][2]);
                }
            }

            // Helper: Given a bit string, insert the bits into a boolean vector
            std::function<void(const std::string&, std::vector<bool>&)> insertBits {[](const std::string &bits, std::vector<bool> &result) {
                for (const char ch: bits)
                    result.push_back(ch == '1');
            }};

            // ******************* Build the boolean vector from DP grid ******************* //

            // Initially a value outside of range
            std::ptrdiff_t enc {3}; 
            std::size_t idx {0}, checkSum {0}, checkPos {1}; 
            std::vector<bool> result;
            while (idx < message.size()) {
                std::string chStr{message[idx]};
                std::vector<std::size_t>::iterator it{std::min_element(dp[idx].begin(), dp[idx].end())};

                // If multiple min exists, can we pick the current encoding?
                std::ptrdiff_t pos {idx > 0 && *it == dp[idx][static_cast<std::size_t>(enc)]? enc: it - dp[idx].begin()};

                if (*it == INF) {
                    std::cerr << "Error: Unsupported character encountered in the message.\n";
                    std::exit(1);
                }

                else {
                    // Shift or Change character
                    std::size_t changeOrShiftOrd;

                    // We dont need to shift or change encoding if it already matches
                    if(enc != pos) {
                        // START_X chars
                        if (idx == 0)
                            changeOrShiftOrd = pos == 0? START_A: pos == 1? START_B: START_C;

                        // Temporary shifts
                        else if (pos == 3 || pos == 4)
                            changeOrShiftOrd = pos == 3? CODE128A["Shift B"]: CODE128B["Shift A"];

                        // Code A encoding
                        else if (enc == 0)
                            changeOrShiftOrd = pos == 1? CODE128A["Code B"]: CODE128A["Code C"];

                        // Code B encoding
                        else if (enc == 1)
                            changeOrShiftOrd = pos == 0? CODE128B["Code A"]: CODE128B["Code C"];

                        // Code C encoding
                        else
                            changeOrShiftOrd = pos == 0? CODE128C["Code A"]: CODE128B["Code B"];

                        // Start_X is always 1, rest of the chars can work with incrementally updated weights
                        checkSum += idx == 0? changeOrShiftOrd: checkPos++ * changeOrShiftOrd;
                        insertBits(patterns[changeOrShiftOrd], result);
                    }

                    // Insert actual token
                    std::size_t tokenOrd{pos == 0 || pos == 3? CODE128A[chStr]: pos == 1 || pos == 4? CODE128B[chStr]: CODE128C[chStr + message[idx + 1]]};
                    checkSum += checkPos++ * tokenOrd;
                    insertBits(patterns[tokenOrd], result);
                }

                // Change encoding if they are not shift ones
                if (pos != 3 && pos != 4) enc = pos;

                // Skip a char if Code C
                if (pos == 2) idx++;

                idx++;
            }

            // ******************* Checksum, STOP ******************* //

            insertBits(patterns[checkSum % 103], result);
            insertBits(patterns[STOP], result);

            return result;
        }

        // Write the boolean vector into a PBM file
        void print(std::vector<bool> &codes, std::string &fname, std::size_t width = 5, std::size_t height = 150, std::size_t quiet = 10) {
            // Write as binary
            std::ofstream ofs {fname, std::ios::binary};

            // Specify format and structure
            ofs << "P4\n" << ((quiet + codes.size() + quiet) * width) << " " << height << "\n";

            // Helper to add 'zones' into the output image
            std::function<void(bool, unsigned int&, int&)> addZone {[&quiet, &codes, &width, &ofs](bool isQuiet, unsigned int &byte, int &bitCount){
                std::size_t N {isQuiet? quiet: codes.size()};
                for (std::size_t i{0}; i < N; i++) {
                    for (std::size_t itr {0}; itr < width; itr++) {
                        byte = (byte << 1) | (!isQuiet && codes[i] ? 1: 0);
                        if (++bitCount == 8) {
                            ofs.put(static_cast<char>(byte));
                            bitCount = 0;
                            byte = 0;
                        }
                    }
                }
            }};

            // Read the vector of bools and write to file
            for (std::size_t i {0}; i < height; i++) {
                // Repeat line 'height' no of times
                unsigned int byte {0};
                int bitCount {0};

                addZone(true,  byte, bitCount);  // Quiet Zone at start
                addZone(false, byte, bitCount);  // Actual Code at middle
                addZone(true,  byte, bitCount);  // Quiet Zone at end

                // Any pending bits?
                if (bitCount > 0) {
                    byte <<= (8 - bitCount);
                    ofs.put(static_cast<char>(byte));
                }
            }
        }

};

// ********************* SAMPLE FUNCTION RUN ********************* //

int main(int argc, char **argv) {
    if (argc != 3)
        std::cout << "Code 128 Barcode Generator.\nUsage: ./barcode <inputFile> <outputFile>\n";

    else {
        // Extract params
        std::ifstream ifs {argv[1]};
        std::string ofname {argv[2]};

        if (!ifs) {
            std::cerr << "Error: Invalid input file provided.\n";
            std::exit(1);
        }

        // Read the message
        std::string message {""}, buffer;
        while (std::getline(ifs, buffer)) {
            if (!message.empty())
                message += "\n";
            message += buffer;
        }

        if (message.size() > 128) {
            std::cerr << "Error: Input message is too long.\n";
            std::exit(1);
        }

        // Generate the barcode
        Barcode bg;
        std::vector<bool> codes {bg.encode(message)};
        bg.print(codes, ofname);
    }
}
