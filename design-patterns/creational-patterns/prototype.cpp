#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

class DocumentPrototype {
    public:
        virtual std::shared_ptr<DocumentPrototype> clone() const = 0;
        virtual ~DocumentPrototype() = default;
};

class TextDocument: public DocumentPrototype {
    std::string content {""};

    public:
        TextDocument(std::string content): content(content) {}

        // Implement a clone method inside class to copy all fields including private ones
        std::shared_ptr<DocumentPrototype> clone() const override {
            return std::make_shared<TextDocument>(content);
        }

        void display() {
            std::cout << "=======================================\n"
                      << "File Type   : " << "Text Document" << "\n"
                      << "File Content: " << content         << "\n"
                      << "=======================================\n";
        }
};

class ImageDocument: public DocumentPrototype {
    int rows, cols;
    std::vector<std::vector<int>> dataMatrix; 

    public:
        ImageDocument(int rows, int cols, const std::vector<std::vector<int>> &matrix):
            rows(rows), cols(cols), dataMatrix(matrix) {}

        // Implement a clone method inside class to copy all fields including private ones
        std::shared_ptr<DocumentPrototype> clone() const override {
            std::size_t rows_ {static_cast<std::size_t>(rows)}, cols_ {static_cast<std::size_t>(cols)};
            std::vector<std::vector<int>> dataMatrixCopy(rows_, std::vector<int>(cols_));
            for (std::size_t i{0}; i < rows_; i++)
                for (std::size_t j{0}; j < cols_; j++)
                    dataMatrixCopy[i][j] = dataMatrix[i][j];
            return std::make_shared<ImageDocument>(rows, cols, dataMatrixCopy);
        }

        void display() {
            std::ostringstream oss;
            for (std::size_t i{0}; i < static_cast<std::size_t>(rows); i++) {
                for (std::size_t j{0}; j < static_cast<std::size_t>(cols); j++)
                    oss << dataMatrix[i][j] << " ";
                oss << (i == static_cast<std::size_t>(rows) - 1? "": "\n");
            }

            std::cout << "========================================\n"
                      << "File Type    : " << "Image Document" << "\n"
                      << "File Content :\n" << oss.str()        << "\n"
                      << "========================================\n";
        }
};

int main() {
    // A simple text document -----------------
    std::shared_ptr<TextDocument> doc1 {std::make_shared<TextDocument>("Some dummy text here")};
    std::shared_ptr<TextDocument> doc1Copy{std::static_pointer_cast<TextDocument>(doc1->clone())};
    doc1->display();
    doc1Copy->display();

    std::cout << "\n";

    // A more complex image document -----------------
    std::vector<std::vector<int>> matrix {{{1, 0, 2, 2, 1, 0, 1, 2}, {0, 1, 3, 2, 1, 0, 0, 0}}};
    std::shared_ptr<ImageDocument> doc2 {std::make_shared<ImageDocument>(2, 8, matrix)};
    std::shared_ptr<ImageDocument> doc2Copy{std::static_pointer_cast<ImageDocument>(doc2->clone())};
    doc2->display();
    doc2Copy->display();

    return 0;
}
