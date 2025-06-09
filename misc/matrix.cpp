#include <functional>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <valarray>

namespace stdx {
    // Slicing Vector / Matrix
    struct Slice { 
        std::size_t start, end, step; 
        bool empty {false};

        Slice(): start(0), end(0), step(1), empty(true) {}
        Slice(const std::size_t start, const std::size_t end): start(start), end(end), step(1) {}
        Slice(const std::size_t start, const std::size_t end, const std::size_t step): 
            start(start), end(end), step(step) 
        { if (step == 0) throw std::runtime_error("Step cannot be 0"); }
    };

    template<typename T>
    class Vector: public std::valarray<T> {
        public:
            using std::valarray<T>::valarray;

            friend std::ostream &operator<<(std::ostream &os, const Vector<T> &vec) {
                std::size_t N {vec.size()};
                os << "[ ";
                for (std::size_t i {0}; i < N; i++)
                    os << vec[i] << ' ';
                os << ']';
                return os;
            }

            [[nodiscard]] inline Vector operator()(const Slice &slice) const {
                std::vector<T> temp;
                std::size_t end {slice.empty? this->size(): std::min(slice.end, this->size())};
                for (std::size_t idx{slice.start}; idx < end; idx += slice.step)
                    temp.push_back((*this)[idx]);
                return Vector{temp.data(), temp.size()};
            }
    };

    template<typename T>
    class Matrix {
        public:
            Matrix(const std::size_t rows, const std::size_t cols, const T init = T{}):
                rows(rows), cols(cols), data(init, rows * cols) 
            {}

            Matrix(std::initializer_list<std::initializer_list<T>> init):
                rows(init.size()), cols(init.begin()->size()), data(rows * cols) 
            {
                std::size_t i = 0;
                for (auto row: init) {
                    if (row.size() != cols)
                        throw std::invalid_argument("Inconsistent row size");
                    for (auto val: row)
                        data[i++] = val;
                }
            }

            [[nodiscard]] inline Matrix operator()(const Slice &rslice, const Slice &cslice) const {
                std::size_t rrows {0};
                std::size_t rend {rslice.empty? rows: std::min(rslice.end, rows)};
                std::size_t cend {cslice.empty? cols: std::min(cslice.end, cols)};
                std::vector<T> temp;
                for (std::size_t row {rslice.start}; row < rend; row += rslice.step) {
                    ++rrows;
                    for (std::size_t col {cslice.start}; col < cend; col += cslice.step) {
                        temp.push_back((*this)(row, col));
                    }
                }
                return Matrix{rrows, temp.size() / rrows, {temp.data(), temp.size()}};
            }

            // Index 1D array with 2D indices
            inline T &operator()(const std::size_t &row, const std::size_t &col) {
                if (!validateIdx(row, col)) 
                    throw std::runtime_error("Indices out of bounds.");
                return data[row * cols + col];
            }

            // Indexing (const variant)
            inline const 
            T &operator()(const std::size_t &row, const std::size_t &col) const {
                return const_cast<T&>(data[row * cols + col]);
            }

            // Get the Row
            [[nodiscard]] Vector<T> row(const std::size_t rowIdx) const {
                if (rowIdx >= rows) throw std::runtime_error("Row out of range");
                std::vector<T> temp;
                for (std::size_t colIdx {0}; colIdx < cols; ++colIdx) 
                    temp.push_back((*this)(rowIdx, colIdx));
                return Vector<T>{temp.data(), temp.size()};
            }

            // Get the Col
            [[nodiscard]] Vector<T> col(const std::size_t colIdx) const {
                if (colIdx >= cols) throw std::runtime_error("Col out of range");
                std::vector<T> temp;
                for (std::size_t rowIdx {0}; rowIdx < rows; ++rowIdx) 
                    temp.push_back((*this)(rowIdx, colIdx));
                return Vector<T>{temp.data(), temp.size()};
            }

            // Transpose matrix
            [[nodiscard]] Matrix transpose() const {
                Matrix result{cols, rows};
                for (std::size_t i {0}; i < rows; i++)
                    for (std::size_t j {0}; j < cols; j++)
                        result(j, i) = (*this)(i, j);
                return result;
            }

            // Multiply two matrices
            [[nodiscard]] static Matrix dot(const Matrix &mat1, const Matrix &mat2) {
                std::size_t rows {mat1.rows}, cols {mat2.cols}, inner {mat2.rows};
                if (mat1.cols != inner) 
                    throw std::runtime_error("The dimensions are not aligned, cannot do a product.");

                Matrix result {mat1.rows, mat2.cols};
                for (std::size_t i {0}; i < rows; i++) {
                    for (std::size_t j {0}; j < cols; j++) {
                        for (std::size_t k {0}; k < inner; k++) {
                            result(i, j) += mat1(i, k) * mat2(k, j);
                        }
                    }
                }

                return result;
            }

            // Print util
            friend std::ostream &operator<<(std::ostream &os, const Matrix &mat) {
                for (std::size_t i {0}; i < mat.rows; i++) {
                    os << "[ ";
                    for (std::size_t j {0}; j < mat.cols; j++)
                        os << mat(i, j) << ' ';
                    os << (i < mat.rows - 1? "]\n": "]");
                }
                return os;
            }

            // Matrix, Matrix ops
            friend Matrix operator+(const Matrix &mat1, const Matrix &mat2) { return binaryOp(mat1, mat2, std::plus<>()); }
            friend Matrix operator-(const Matrix &mat1, const Matrix &mat2) { return binaryOp(mat1, mat2, std::minus<>()); }
            friend Matrix operator*(const Matrix &mat1, const Matrix &mat2) { return binaryOp(mat1, mat2, std::multiplies<>()); }
            friend Matrix operator/(const Matrix &mat1, const Matrix &mat2) { return binaryOp(mat1, mat2, std::divides<>()); }

            // Matrix, Vector ops
            friend Matrix operator+(const Matrix &mat, const Vector<T> &vec) { return binaryOp(mat, vec, std::plus<>()); }
            friend Matrix operator-(const Matrix &mat, const Vector<T> &vec) { return binaryOp(mat, vec, std::minus<>()); }
            friend Matrix operator*(const Matrix &mat, const Vector<T> &vec) { return binaryOp(mat, vec, std::multiplies<>()); }
            friend Matrix operator/(const Matrix &mat, const Vector<T> &vec) { return binaryOp(mat, vec, std::divides<>()); }
            friend Matrix operator+(const Vector<T> &vec, const Matrix &mat) { return binaryOp(mat, vec, std::plus<>()); }
            friend Matrix operator*(const Vector<T> &vec, const Matrix &mat) { return binaryOp(mat, vec, std::multiplies<>()); }

            // Matrix, Scalar ops
            friend Matrix operator+(const Matrix &mat, const T &val) { return Matrix{mat.rows, mat.cols, mat.data + val}; }
            friend Matrix operator-(const Matrix &mat, const T &val) { return Matrix{mat.rows, mat.cols, mat.data - val}; }
            friend Matrix operator*(const Matrix &mat, const T &val) { return Matrix{mat.rows, mat.cols, mat.data * val}; }
            friend Matrix operator/(const Matrix &mat, const T &val) { return Matrix{mat.rows, mat.cols, mat.data / val}; }
            friend Matrix operator+(const T &val, const Matrix &mat) { return Matrix{mat.rows, mat.cols, mat.data + val}; }
            friend Matrix operator*(const T &val, const Matrix &mat) { return Matrix{mat.rows, mat.cols, mat.data * val}; }

        private:
            std::size_t rows, cols;
            Vector<T> data;

            Matrix(const std::size_t rows, const std::size_t cols, const Vector<T> &data):
                rows(rows), cols(cols), data(data) {}

            template<typename Op>
            static Matrix binaryOp(const Matrix &mat1, const Matrix &mat2, const Op &op) {
                if (mat1.rows != mat2.rows || mat1.cols != mat2.cols)
                    throw std::runtime_error("Dimensions do not match.");
                Vector<T> data(mat1.rows * mat1.cols);
                for (std::size_t idx{0}; idx < data.size(); ++idx)
                    data[idx] = op(mat1.data[idx], mat2.data[idx]);
                return Matrix {mat1.rows, mat1.cols, data};
            }

            template<typename Op>
            static Matrix binaryOp(const Matrix &mat, const Vector<T> &vec, const Op &op) {
                if (vec.size() != mat.cols) throw std::runtime_error("Dimension mismatch, cannot broadcast.");
                Matrix result {mat.rows, mat.cols};
                for (std::size_t row {0}; row < mat.rows; row++)
                    for (std::size_t col {0}; col < mat.cols; col++)
                        result(row, col) = op(mat(row, col), vec[col]);
                return result;
            }

            inline bool 
            validateIdx(const std::size_t &row, 
                        const std::size_t &col) 
            const noexcept
            { return row < rows && col < cols; }
    };
}

int main() {
    auto mat1 {stdx::Matrix<int>{{1, 2}, {4, 5}, {6, 7}}};
    auto mat2 {stdx::Matrix<int>{{1, 2}, {3, 4}}};
    std::cout << stdx::Matrix<int>::dot(mat1, mat2) << '\n';

    auto mat3 {stdx::Matrix<double>{2, 2}};
    std::cout << (mat3 + stdx::Vector<double>({1., 2.})) << '\n';

    auto va {stdx::Vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9}};
    std::cout << va({}) << '\n';

    //std::cout << mat1({0, 1}, {0, 2}) << '\n';
    std::cout << mat1.col(1) << '\n';

    return 0;
}
