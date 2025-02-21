#include <functional>
#include <iostream>
#include <ostream>
#include <random>
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
        std::size_t rows {mat1.size()}, cols {mat2[0].size()}, inner {mat2.size()};

        if (mat1[0].size() != inner) 
            throw std::runtime_error("The dimensions are not aligned, cannot do a product.");

        std::vector<std::vector<T>> result(rows, std::vector<T>(cols));
        for (std::size_t i {0}; i < rows; i++)
            for (std::size_t k {0}; k < inner; k++)
                for (std::size_t j {0}; j < cols; j++)
                    result[i][j] += mat1[i][k] * mat2[k][j];

        return result;
    }

    std::vector<std::vector<double>> randn(const std::size_t rows, const std::size_t cols, std::size_t seed) {
        std::mt19937 random_gen {seed};
        std::uniform_real_distribution<double> dist{-1, 1};
        std::vector<std::vector<double>> result(rows, std::vector<double>(cols));
        for (std::size_t row {0}; row < rows; row++)
            for (std::size_t col {0}; col < cols; col++)
                result[row][col] = dist(random_gen);
        return result;
    }


    /* ------------ Aggregate Ops - Matrix ------------ */
    template<typename T, typename Op> requires std::is_arithmetic_v<T>
    void apply_Op(const std::vector<std::vector<T>> &mat, Op op) {
        for (const std::vector<T> &row: mat) 
            for (const T &val: row) op(val);
    }
    template <typename T> requires std::is_arithmetic_v<T>
    T sum(const std::vector<std::vector<T>> &mat) {
        T result {0.};
        apply_Op(mat, [&result](T val) { result += val; });
        return result;
    }
    template <typename T> requires std::is_arithmetic_v<T>
    T mean(const std::vector<std::vector<T>> &mat) {
        return sum(mat) / static_cast<T>(mat.size() * mat[0].size());
    }
    /* ------------ Aggregate Ops - Matrix ------------ */


    /* ------------ Aggregate Ops - Vec ------------ */
    template<typename T, typename Op> requires std::is_arithmetic_v<T>
    void apply_Op(const std::vector<T> &vec, Op op) {
        for (const T &val: vec) op(val);
    }
    template <typename T> requires std::is_arithmetic_v<T>
    T sum(const std::vector<T> &vec) {
        T result {0.};
        apply_Op(vec, [&result](T val) { result += val; });
        return result;
    }
    template <typename T> requires std::is_arithmetic_v<T>
    T mean(const std::vector<T> &vec) {
        return sum(vec) / static_cast<T>(vec.size());
    }
    /* ------------ Aggregate Ops - Vec ------------ */


    /* ------------ Element wise operations - matrix1, matrix2 ------------ */
    template <typename T, typename Op> requires std::is_arithmetic_v<T>
    std::vector<std::vector<T>> elementwise_Op(
        const std::vector<std::vector<T>> &m1, 
        const std::vector<std::vector<T>> &m2,
        Op op
    ) {
        std::size_t rows {m1.size()}, cols {m1[0].size()};
        if (rows != m2.size() || cols != m2[0].size())
            throw std::runtime_error("Dimensions do not match.");

        std::vector<std::vector<T>> result(rows, std::vector<T>(cols));
        for (std::size_t row {0}; row < rows; row++)
            for (std::size_t col {0}; col < cols; col++)
                result[row][col] = op(m1[row][col], m2[row][col]);

        return result;
    }
    template <typename T> requires std::is_arithmetic_v<T>
    std::vector<std::vector<T>> operator-(const std::vector<std::vector<T>> &m1, const std::vector<std::vector<T>> &m2) {
        return elementwise_Op(m1, m2, std::minus<T>());
    }
    template <typename T> requires std::is_arithmetic_v<T>
    std::vector<std::vector<T>> operator+(const std::vector<std::vector<T>> &m1, const std::vector<std::vector<T>> &m2) {
        return elementwise_Op(m1, m2, std::plus<T>());
    }
    /* ------------ Element wise operations - matrix1, matrix2 ------------ */


    /* ------------ Element wise operations - vector1, vector2 ------------ */
    template <typename T, typename Op> requires std::is_arithmetic_v<T>
    std::vector<T> elementwise_Op(
        const std::vector<T> &v1, 
        const std::vector<T> &v2,
        Op op
    ) {
        std::size_t N {v1.size()};
        if (N != v2.size())
            throw std::runtime_error("Dimensions do not match.");

        std::vector<T> result(N);
        for (std::size_t i {0}; i < N; i++)
                result[i] = op(v1[i], v2[i]);

        return result;
    }
    template <typename T> requires std::is_arithmetic_v<T>
    std::vector<T> operator-(const std::vector<T> &v1, const std::vector<T> &v2) {
        return elementwise_Op(v1, v2, std::minus<T>());
    }
    template <typename T> requires std::is_arithmetic_v<T>
    std::vector<T> operator+(const std::vector<T> &v1, const std::vector<T> &v2) {
        return elementwise_Op(v1, v2, std::plus<T>());
    }
    /* ------------ Element wise operations - vector1, vector2 ------------ */


    /* ------------ Broadcast operations - matrix, vector ------------ */
    template <typename T1, typename T2, typename Op> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> broadcast_Op(const std::vector<std::vector<T1>> &mat, const std::vector<T2> vec, Op op) {
        std::size_t rows {mat.size()}, cols {mat[0].size()};
        if (rows != vec.size()) throw std::runtime_error("Dimension mismatch, cannot broadcast.");
        std::vector<std::vector<T1>> result(rows, std::vector<T1>(cols));
        for (std::size_t row {0}; row < rows; row++)
            for (std::size_t col {0}; col < cols; col++)
                result[row][col] = op(mat[row][col], vec[col]);

        return result;
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> operator+(const std::vector<std::vector<T1>> &mat, const std::vector<T2> vec) {
        return broadcast_Op(mat, vec, std::plus<T1>{});
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> operator-(const std::vector<std::vector<T1>> &mat, const std::vector<T2> vec) {
        return broadcast_Op(mat, vec, std::minus<T1>{});
    }
    /* ------------ Broadcast operations - matrix, vector ------------ */


    /* ------------ Broadcast operations - matrix, constant ------------ */
    template <typename T1, typename T2, typename Op> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> broadcast_Op(const std::vector<std::vector<T1>> &mat, const T2 val, Op op) {
        std::size_t rows {mat.size()}, cols {mat[0].size()};
        std::vector<std::vector<T1>> result(rows, std::vector<T1>(cols));
        for (std::size_t row {0}; row < rows; row++)
            for (std::size_t col {0}; col < cols; col++)
                result[row][col] = op(mat[row][col], val);

        return result;
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> operator+(const std::vector<std::vector<T1>> &mat, const T2 val) {
        return broadcast_Op(mat, val, std::plus<T1>{});
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> operator-(const std::vector<std::vector<T1>> &mat, const T2 val) {
        return broadcast_Op(mat, val, std::minus<T1>{});
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> operator*(const std::vector<std::vector<T1>> &mat, const T2 val) {
        return broadcast_Op(mat, val, std::multiplies<T1>{});
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<std::vector<T1>> operator/(const std::vector<std::vector<T1>> &mat, const T2 val) {
        if (val == 0) throw std::runtime_error("Cannot divide by zero.");
        return broadcast_Op(mat, val, std::divides<T1>{});
    }
    /* ------------ Broadcast operations - matrix, constant ------------ */


    /* ------------ Broadcast operations - vector, constant ------------ */
    template <typename T1, typename T2, typename Op> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<T1> broadcast_Op(const std::vector<T1> &vec, const T2 val, Op op) {
        std::size_t N {vec.size()};
        std::vector<T1> result(N);
        for (std::size_t i {0}; i < N; i++)
            result[i] = op(vec[i], val);

        return result;
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<T1> operator+(const std::vector<T1> &vec, const T2 val) {
        return broadcast_Op(vec, val, std::plus<T1>{});
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<T1> operator-(const std::vector<T1> &vec, const T2 val) {
        return broadcast_Op(vec, val, std::minus<T1>{});
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<T1> operator*(const std::vector<T1> &vec, const T2 val) {
        return broadcast_Op(vec, val, std::multiplies<T1>{});
    }
    template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
    std::vector<T1> operator/(const std::vector<T1> &vec, const T2 val) {
        if (val == 0) throw std::runtime_error("Cannot divide by zero.");
        return broadcast_Op(vec, val, std::divides<T1>{});
    }
    /* ------------ Broadcast operations - vector, constant ------------ */

    class LinearRegression {
        std::size_t iterations;
        double learningRate;
        std::size_t seed;
        std::vector<std::vector<double>> weights;
        std::vector<double> bias;
        bool weightsInitialized {false};

        public:
            LinearRegression(std::size_t iters, double lr, std::size_t seed = 42): 
                iterations(iters), learningRate(lr), seed(seed)
            {}

            LinearRegression& fit(std::vector<std::vector<double>> &X, std::vector<std::vector<double>> &y) {
                std::size_t nSamples {X.size()}, nFeats {X[0].size()}, nTargets {y[0].size()};
                if (nSamples != y.size()) throw std::runtime_error("No. of samples do not match.");
                weights = randn(nFeats, nTargets, seed);
                bias = std::vector<double>(nTargets, 0);
                weightsInitialized = true;

                for (std::size_t iter {0}; iter < iterations; iter++) {
                    std::vector<std::vector<double>> yPreds {predict(X)};
                    std::vector<std::vector<double>> dW {dot(transpose(X), y - yPreds) * -2 / nSamples};

                    std::vector<double> dB(y[0].size(), 0.0);
					for (const auto &row : (y - yPreds))
						for (std::size_t j = 0; j < dB.size(); j++)
							dB[j] += row[j];
					dB = dB * -2.0 / static_cast<double>(nSamples);

                    weights = weights - (dW * learningRate);
                    bias = bias - (dB * learningRate);
                }

                return *this;
            }

            std::vector<std::vector<double>> predict(const std::vector<std::vector<double>> &X) {
                if (!weightsInitialized) throw std::runtime_error("Model not fit yet.");
                return dot(X, weights) + bias;
            }
    };
}

int main() {
    using namespace MLUtils;
    std::vector<std::vector<double>> mat1 {{1,2}, {3,4}}, mat2 {{5,6,7}, {8,9,10}};
    std::vector<std::vector<double>> result {dot(mat1, mat2)};
    for (const std::vector<double> &row: result)
        std::cout << row << '\n';
}
