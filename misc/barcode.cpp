/*
 * Code 128 Implementation in CPP
 */

#include <fstream>
#include <string>
#include <vector>

class Barcode {
    public:
        static void print(std::vector<bool> &codes, std::string &fname, std::size_t width = 10, std::size_t height = 80) {
            // Write as binary
            std::ofstream ofs {fname, std::ios::binary};

            // Specify format and structure
            ofs << "P4\n" << (codes.size() * width) << " " << height << "\n";

            // Read the vector of bools and write to file
            for (std::size_t i {0}; i < height; i++) {
                // Repeat line 'height' no of times
                unsigned int byte {0};
                int bitCount {0};
                for (bool code: codes) {
                    for (std::size_t itr {0}; itr < width; itr++) {
                        byte = (byte << 1) | (code? 1: 0);
                        if (++bitCount == 8) {
                            ofs.put(static_cast<char>(byte));
                            bitCount = 0;
                            byte = 0;
                        }
                    }
                }

                // Any pending bits?
                if (bitCount > 0) {
                    byte <<= (8 - bitCount);
                    ofs.put(static_cast<char>(byte));
                }
            }
        }
};

int main() {
    std::vector<bool> codes {{1,0,0,1,1,1,0,1,1,0,0,1,1,1,0,0,1,1,1,1,1,0,0,1,0,1,0,0,1,1,0,0,0,0,1,1,1,0}};
    std::string fname {"sample.pbm"};
    Barcode::print(codes, fname);
}
