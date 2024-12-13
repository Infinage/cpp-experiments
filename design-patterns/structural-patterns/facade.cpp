#include <iostream>
#include <string>
#include <vector>

// ------------------ Abstracting away complex logic ------------------ //

class ImageReader {
    public:
        std::vector<std::vector<int>> read(const std::string &fpath, const std::string &format) {
            std::cout << "Reading input      : " << fpath   << "\n";
            std::cout << "Input file format  : " << format << "\n";
            return {};
        }
};

class ImageWriter {
    public:
        bool convert(std::vector<std::vector<int>> &image, const std::string &format, const std::string &fpath) {
            std::cout << "Writing output     : " << fpath   << "\n";
            std::cout << "Output file format : " << format << "\n";
            std::cout << "Conversion status  : successful\n";
            return true;
        }
};

class ImageConvertor {
    private:
        ImageReader reader;
        ImageWriter writer;

    public:
        bool convert(const std::string &&ifpath, const std::string &&oformat) {
            std::size_t dotPos {ifpath.rfind('.')};
            if (dotPos == std::string::npos) {
                std::cout << "Not a valid input file.\n";
                return false;
            } else {
                const std::string iformat {ifpath.substr(dotPos + 1)}, ofpath {ifpath.substr(0, dotPos) + '.' + oformat};
                std::vector<std::vector<int>> imageMatrix {reader.read(ifpath, iformat)};
                return writer.convert(imageMatrix, oformat, ofpath);
            }
        }
};

// ------------------ Sample Client end logic ------------------ //

int main() {
    ImageConvertor convertor;
    convertor.convert("sample.png", "jpg");
    return 0;
}
