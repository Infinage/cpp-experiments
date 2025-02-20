#include <functional>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace MLUtils {
    template <typename T>
    std::ostream &operator<< (std::ostream &oss, const std::vector<T> &vec) {
        oss << "[ ";
        for (const T &val: vec)
            oss << val << ' ';
        oss << ']';
        return oss;
    }

    template <typename T>
    std::vector<std::vector<T>> transpose(const std::vector<std::vector<T>> &matrix) {
        std::size_t rows {matrix.size()}, cols {matrix[0].size()};
        std::vector<std::vector<T>> result(cols, std::vector<T>(rows));
        for (std::size_t row {0}; row < rows; row++)
            for (std::size_t col {0}; col < cols; col++)
                result[col][row] = matrix[row][col];
        return result;
    }

    template <typename T> requires std::is_arithmetic_v<T>
    std::vector<std::vector<T>> dot(
        const std::vector<std::vector<T>> &mat1, 
        const std::vector<std::vector<T>> &mat2
    ) {
        std::size_t rows1 {mat1.size()}, cols1 {mat1[0].size()};
        std::size_t rows2 {mat2.size()}, cols2 {mat2[0].size()};
        if (cols1 != rows2) 
            throw std::runtime_error("The dimensions are not aligned, cannot do a product.");
        
    }

    template <typename T, typename Op> requires std::is_arithmetic_v<T>
    std::vector<std::vector<T>> elementwise_Op(
        const std::vector<std::vector<T>> &v1, 
        const std::vector<std::vector<T>> &v2,
        Op op
    ) {
        std::size_t rows {v1.size()}, cols {v1[0].size()};
        if (rows != v2.size() || cols != v2[0].size())
            throw std::runtime_error("Dimensions do not match.");

        std::vector<std::vector<T>> result(rows, std::vector<T>(cols));
        for (std::size_t row {0}; row < rows; row++)
            for (std::size_t col {0}; col < cols; col++)
                result[row][col] = op(v1[row][col], v2[row][col]);

        return result;
    }

    template <typename T1, typename T2, typename Op> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> broadcast_Op(const std::vector<std::vector<T1>> &vec, const T2 val, Op op) {
        std::size_t rows {vec.size()}, cols {vec[0].size()};
        std::vector<std::vector<T1>> result(rows, std::vector<T1>(cols));
        for (std::size_t row {0}; row < rows; row++)
            for (std::size_t col {0}; col < cols; col++)
                result[row][col] = op(vec[row][col], val);

        return result;
    }

    template<typename T, typename Op> requires std::is_arithmetic_v<T>
    void apply_Op(const std::vector<std::vector<T>> &vec, Op op) {
        for (const std::vector<T> &row: vec) 
            for (const T &val: row) op(val);
    }

    template <typename T> requires std::is_arithmetic_v<T>
    std::vector<std::vector<T>> operator-(const std::vector<std::vector<T>> &v1, const std::vector<std::vector<T>> &v2) {
        return elementwise_Op(v1, v2, std::minus<T>());
    }

    template <typename T> requires std::is_arithmetic_v<T>
    std::vector<std::vector<T>> operator+(const std::vector<std::vector<T>> &v1, const std::vector<std::vector<T>> &v2) {
        return elementwise_Op(v1, v2, std::plus<T>());
    }

    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> operator*(const std::vector<std::vector<T1>> &vec, const T2 val) {
        return broadcast_Op(vec, val, std::multiplies<T1>{});
    }

    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> operator/(const std::vector<std::vector<T1>> &vec, const T2 val) {
        if (val == 0) throw std::runtime_error("Cannot divide by zero.");
        return broadcast_Op(vec, val, std::divides<T1>{});
    }

    template <typename T> requires std::is_arithmetic_v<T>
    T sum(const std::vector<std::vector<T>> &vec) {
        T result {0.};
        apply_Op(vec, [&result](T val) { result += val; });
        return result;
    }

    template <typename T> requires std::is_arithmetic_v<T>
    T mean(const std::vector<std::vector<T>> &vec) {
        return sum(vec) / static_cast<T>(vec.size() * vec[0].size());
    }
}

int main() {
    using namespace MLUtils;
    std::vector<std::vector<double>> arr {{1, 2, 3, 4, 5, 6}};

    std::cout << (arr / 2)[0] << '\n';
    std::cout << sum(arr) << "\n";
    std::cout << mean(arr) << "\n";
}
