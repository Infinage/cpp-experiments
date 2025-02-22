#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <ostream>
#include <random>
#include <stdexcept>
#include <type_traits>
#include <vector>

// g++ linear-regression.cpp -shared -o CPPLearn.so -fPIC -std=c++23 -I/usr/include/python3.12 -lpython3.12
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
namespace py = pybind11;

namespace CPPLearn {

    namespace Core {
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


        /* ------------ Reduction Ops - Matrix ------------ */
        template <typename T, typename Op> requires std::is_arithmetic_v<T>
        std::vector<T> reduce_Op(const std::vector<std::vector<T>> &mat, const T initVal, Op op, short axis = 1) {
            std::size_t rows {mat.size()}, cols {mat[0].size()};
            std::vector<T> result(axis == 0? cols: rows, initVal);
            for (std::size_t row {0}; row < rows; row++) {
                for (std::size_t col {0}; col < cols; col++) {
                    if (axis == 0) 
                        result[col] = op(result[col], mat[row][col]);
                    else 
                        result[row] = op(result[row], mat[row][col]);
                }
            }
            return result;
        }
        template <typename T> requires std::is_arithmetic_v<T>
        std::vector<T> sum(const std::vector<std::vector<T>> &mat, short axis) {
            return reduce_Op(mat, static_cast<T>(0), std::plus<>{}, axis);
        }
        template <typename T> requires std::is_arithmetic_v<T>
        std::vector<T> mean(const std::vector<std::vector<T>> &mat, short axis) {
            return sum(mat, axis) / (axis == 0 ? mat.size() : mat[0].size());
        }
        /* ------------ Reduction Ops - Matrix ------------ */


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


        // Element wise operations - matrix1, matrix2
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

        // Element wise operations - vector1, vector2
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

        // Broadcast operations - matrix, vector
        template <typename T1, typename T2, typename Op> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> broadcast_Op(const std::vector<std::vector<T1>> &mat, const std::vector<T2> vec, Op op) {
            std::size_t rows {mat.size()}, cols {mat[0].size()};
            if (vec.size() != cols) throw std::runtime_error("Dimension mismatch, cannot broadcast.");
            std::vector<std::vector<T1>> result(rows, std::vector<T1>(cols));
            for (std::size_t row {0}; row < rows; row++)
                for (std::size_t col {0}; col < cols; col++)
                    result[row][col] = op(mat[row][col], vec[col]);

            return result;
        }

        // Broadcast operations - matrix, constant
        template <typename T1, typename T2, typename Op> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> broadcast_Op(const std::vector<std::vector<T1>> &mat, const T2 val, Op op) {
            std::size_t rows {mat.size()}, cols {mat[0].size()};
            std::vector<std::vector<T1>> result(rows, std::vector<T1>(cols));
            for (std::size_t row {0}; row < rows; row++)
                for (std::size_t col {0}; col < cols; col++)
                    result[row][col] = op(mat[row][col], val);

            return result;
        }

        // Broadcast operations - vector, constant
        template <typename T1, typename T2, typename Op> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<T1> broadcast_Op(const std::vector<T1> &vec, const T2 val, Op op) {
            std::size_t N {vec.size()};
            std::vector<T1> result(N);
            for (std::size_t i {0}; i < N; i++)
                result[i] = op(vec[i], val);

            return result;
        }


        /* ------------ Core Utils ------------ */
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
        /* ------------ Core Utils ------------ */
    }

    namespace Overloads {

        /* ------------ Print utils ------------ */
        template <typename T>
        std::ostream &operator<<(std::ostream &oss, const std::vector<T> &vec) {
            oss << "[ ";
            for (const T &val: vec)
                oss << val << ' ';
            oss << ']';
            return oss;
        }

        template <typename T>
        std::ostream &operator<<(std::ostream &oss, const std::vector<std::vector<T>> &vec) {
            std::size_t N {vec.size()};
            oss << "[ ";
            for (std::size_t i {0}; i < N; i++) {
                oss << vec[i];
                oss << (i < N - 1? "\n": " ]");
            }
            return oss;
        }
        /* ------------ Print utils ------------ */


        /* ------------ Mat, Mat operator overrides ------------ */
        template <typename T> requires std::is_arithmetic_v<T>
        std::vector<std::vector<T>> operator-(const std::vector<std::vector<T>> &m1, const std::vector<std::vector<T>> &m2) {
            return Core::elementwise_Op(m1, m2, std::minus<>());
        }
        template <typename T> requires std::is_arithmetic_v<T>
        std::vector<std::vector<T>> operator+(const std::vector<std::vector<T>> &m1, const std::vector<std::vector<T>> &m2) {
            return Core::elementwise_Op(m1, m2, std::plus<>());
        }
        /* ------------ Mat, Mat operator overrides ------------ */


        /* ------------ Vec, Vec operator overrides ------------ */
        template <typename T> requires std::is_arithmetic_v<T>
        std::vector<T> operator-(const std::vector<T> &v1, const std::vector<T> &v2) {
            return Core::elementwise_Op(v1, v2, std::minus<>());
        }
        template <typename T> requires std::is_arithmetic_v<T>
        std::vector<T> operator+(const std::vector<T> &v1, const std::vector<T> &v2) {
            return Core::elementwise_Op(v1, v2, std::plus<>());
        }
        /* ------------ Vec, Vec operator overrides ------------ */
        

        /* ------------ Mat, Vec operator overrides ------------ */
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> operator+(const std::vector<std::vector<T1>> &mat, const std::vector<T2> vec) {
            return Core::broadcast_Op(mat, vec, std::plus<>{});
        }
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> operator-(const std::vector<std::vector<T1>> &mat, const std::vector<T2> vec) {
            return Core::broadcast_Op(mat, vec, std::minus<>{});
        }
        /* ------------ Mat, Vec operator overrides ------------ */


        /* ------------ Mat, Const operator overrides ------------ */
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> operator+(const std::vector<std::vector<T1>> &mat, const T2 val) {
            return Core::broadcast_Op(mat, val, std::plus<>{});
        }
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> operator-(const std::vector<std::vector<T1>> &mat, const T2 val) {
            return Core::broadcast_Op(mat, val, std::minus<>{});
        }
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> operator*(const std::vector<std::vector<T1>> &mat, const T2 val) {
            return Core::broadcast_Op(mat, val, std::multiplies<>{});
        }
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> operator/(const std::vector<std::vector<T1>> &mat, const T2 val) {
            if (val == 0) throw std::runtime_error("Cannot divide by zero.");
            return Core::broadcast_Op(mat, val, std::divides<>{});
        }
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<std::vector<T1>> operator^(const std::vector<std::vector<T1>> &mat, const T2 val) {
            return Core::broadcast_Op(mat, val, [](const T1 element, const T2 val) { return std::pow(element, val); });
        }
        /* ------------ Mat, Const operator overrides ------------ */


        /* ------------ Vec, Const operator overrides ------------ */
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<T1> operator+(const std::vector<T1> &vec, const T2 val) {
            return Core::broadcast_Op(vec, val, std::plus<>{});
        }
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<T1> operator-(const std::vector<T1> &vec, const T2 val) {
            return Core::broadcast_Op(vec, val, std::minus<>{});
        }
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<T1> operator*(const std::vector<T1> &vec, const T2 val) {
            return Core::broadcast_Op(vec, val, std::multiplies<>{});
        }
        template <typename T1, typename T2> requires std::is_arithmetic_v<T1> && std::is_arithmetic_v<T2>
        std::vector<T1> operator/(const std::vector<T1> &vec, const T2 val) {
            if (val == 0) throw std::runtime_error("Cannot divide by zero.");
            return Core::broadcast_Op(vec, val, std::divides<>{});
        }
        /* ------------ Vec, Const operator overrides ------------ */
    }

    namespace Models {
        namespace impl {
            using namespace Overloads;
            class LinearRegression {
                mutable std::size_t seed;
                std::vector<std::vector<double>> weights;
                std::vector<double> bias;

                public:
                    const std::vector<std::vector<double>> &getWeights() const { return weights; }
                    const std::vector<double> &getBias() const { return bias; }
                    LinearRegression(std::size_t seed = 42): seed(seed) {}

                    LinearRegression& fit(
                        const std::vector<std::vector<double>> &X, 
                        const std::vector<std::vector<double>> &y,
                        std::size_t iterations=1000, 
                        double learningRate=0.01
                    ) {
                        std::size_t nSamples {X.size()}, nFeats {X[0].size()}, nTargets {y[0].size()};
                        if (nSamples != y.size()) throw std::runtime_error("No. of samples do not match.");

                        if (weights.empty()) {
                            weights = Core::randn(nFeats, nTargets, seed);
                            bias = std::vector<double>(nTargets, 0);
                        }

                        for (std::size_t iter {0}; iter < iterations; iter++) {
                            std::vector<std::vector<double>> yPreds {predict(X)};
                            std::vector<std::vector<double>> yDelta {y - yPreds};
                            std::vector<std::vector<double>> dW {Core::dot(Core::transpose(X), yDelta) * -2. / nSamples};
                            std::vector<double> dB {Core::sum(yDelta, 0) * -2. / static_cast<double>(nSamples)};
                            weights = weights - (dW * learningRate);
                            bias = bias - (dB * learningRate);
                        }

                        return *this;
                    }

                    std::vector<std::vector<double>> predict(const std::vector<std::vector<double>> &X) const {
                        if (weights.empty()) throw std::runtime_error("Model not fit yet.");
                        return Core::dot(X, weights) + bias;
                    }

                    double score(const std::vector<std::vector<double>> &X, const std::vector<std::vector<double>> &y) const {
                        std::vector<std::vector<double>> yPreds {predict(X)};
                        return Core::sum((y - yPreds) ^ 2);
                    }

                    void save(const std::string &fpath) const {
                        if (weights.empty()) throw std::runtime_error("Model not fit yet.");

                        std::ofstream ofs {fpath, std::ios::binary | std::ios::out};
                        if (!ofs) throw std::runtime_error("Cannot open file for saving the model.");
                        
                        char HEADER[9] {"CPPLEARN"};
                        ofs.write(HEADER, 9);

                        std::size_t M {weights.size()}, N {bias.size()};
                        ofs.write(reinterpret_cast<const char*>(&seed), sizeof(seed));
                        ofs.write(reinterpret_cast<const char*>(&M), sizeof(M));
                        ofs.write(reinterpret_cast<const char*>(&N), sizeof(N));

                        // Write out the weights
                        for (std::size_t i {0}; i < M; i++) {
                            for (std::size_t j {0}; j < N; j++) {
                                const double &val {weights[i][j]};
                                ofs.write(reinterpret_cast<const char*>(&val), sizeof(val));
                            }
                        }

                        // Write out the bias
                        for (std::size_t i {0}; i < N; i++) {
                            const double &val {bias[i]};
                            ofs.write(reinterpret_cast<const char*>(&val), sizeof(val));
                        }
                    }

                    void load(const std::string &fpath) {
                        std::ifstream ifs {fpath, std::ios::binary};
                        if (!ifs) 
                            throw std::runtime_error("Cannot open file for reading the model.");

                        char HEADER[9];
                        ifs.read(HEADER, 9);
                        if (std::strcmp(HEADER, "CPPLEARN") != 0)
                            throw std::runtime_error("Malformed Binary");

                        std::size_t M, N;
                        ifs.read(reinterpret_cast<char *>(&seed), sizeof(seed));
                        ifs.read(reinterpret_cast<char *>(&M), sizeof(M));
                        ifs.read(reinterpret_cast<char *>(&N), sizeof(N));
                        if (ifs.fail()) throw std::runtime_error("Corrupted file, unable to read metadata.");

                        // Read the weights
                        weights.assign(M, std::vector<double>(N));
                        for (std::size_t i {0}; i < M; i++) {
                            for (std::size_t j {0}; j < N; j++) {
                                ifs.read(reinterpret_cast<char *>(&weights[i][j]), sizeof(weights[i][j]));
                                if (ifs.fail()) throw std::runtime_error("Corrupted file, failed to read the weights.");
                            }
                        }

                        // Read the bias
                        bias.assign(N, 0.);
                        for (std::size_t j {0}; j < N; j++) {
                            ifs.read(reinterpret_cast<char *>(&bias[j]), sizeof(bias[j]));
                            if (ifs.fail()) throw std::runtime_error("Corrupted file, failed to read the bias.");
                        }
                    }
            };
        }

        // Redirection to prevent namespace pollution on the client end
        using impl::LinearRegression;
    }

    namespace Utils {
        std::vector<std::vector<double>> from_numpy2D(py::array_t<double> &array) {
            py::buffer_info buf {array.request()};
            if (buf.ndim != 2) throw std::runtime_error("Expected a 2D array.");

            // Convert from the vector of doubles to vector
            std::size_t rows {static_cast<std::size_t>(buf.shape[0])}, cols {static_cast<std::size_t>(buf.shape[1])};
            std::vector<std::vector<double>> result(rows, std::vector<double>(cols));
            double *ptr {static_cast<double *>(buf.ptr)};
            for (std::size_t i {0}; i < rows; i++)
                for (std::size_t j {0}; j < cols; j++)
                    result[i][j] = ptr[i * cols + j];

            return result;
        }
    };
}

PYBIND11_MODULE(CPPLearn, root) {
    root.doc() = "Scikit Learn with C++";
    py::module_ models {root.def_submodule("models", "Machine Learning Models")};
    py::class_<CPPLearn::Models::LinearRegression>(models, "LinearRegression")
        .def(py::init<const std::size_t>(), py::arg("seed") = 42)
        .def("save", &CPPLearn::Models::LinearRegression::save, "Saves the trained model to disk as a binary.")
        .def("load", &CPPLearn::Models::LinearRegression::load, "Loads a trained model from disk.")

        .def_property_readonly("weights", [](CPPLearn::Models::LinearRegression &self) -> py::array_t<double> { 
            return py::cast(self.getWeights()); 
        }, "Get the weights of the model.")

        .def_property_readonly("bias", [](CPPLearn::Models::LinearRegression &self) -> py::array_t<double> { 
            return py::cast(self.getBias()); 
        }, "Get the bias of the model.")
                        
        .def("fit", [](
            CPPLearn::Models::LinearRegression &self, 
            py::array_t<double> &XTrain, py::array_t<double> &YTrain, 
            std::size_t iterations=1000, double learningRate=0.01
        ) {
            self.fit(CPPLearn::Utils::from_numpy2D(XTrain), CPPLearn::Utils::from_numpy2D(YTrain), iterations, learningRate);
        }, "Fit the model against provided set of inputs.")

        .def("predict", [](CPPLearn::Models::LinearRegression &self, py::array_t<double> &XTtest) -> py::array_t<double> {
            return py::cast(self.predict(CPPLearn::Utils::from_numpy2D(XTtest)));
        }, "Returns predictions against a set of inputs. Requires that the model be fit already.")

        .def("score", [](CPPLearn::Models::LinearRegression &self, py::array_t<double> &XTest, py::array_t<double> &YTest) {
            return self.score(CPPLearn::Utils::from_numpy2D(XTest), CPPLearn::Utils::from_numpy2D(YTest));
        }, "Computes the R2 score of the trained model against provided inputs.");
}

/*
int main() {
    using namespace CPPLearn::Overloads; 
    using namespace CPPLearn::Models;

    std::vector<std::vector<double>> XTrain {{1., 2.}, {3., 4.}, {5., 6.}, {7., 8.}, {9., 10.}, {2., 4.}, {1., 0.}, {-2, -5.}, {-1, -3}};
    std::vector<std::vector<double>> yTrain {{3, 1.5}, {7, 3.5}, {10, 5.}, {15, 7.5}, {20, 10.}, {6.2, 3.1}, {1, 0.48}, {-6.9, -3.4}, {-4, -2.}};

    std::vector<std::vector<double>> XTest {{9., 5.}, {2., 3.}, {-5, 5}, {-2, 5}};
    std::vector<std::vector<double>> yTest {{14., 7.}, {5., 2.5}, {0., 0.}, {3., 1.5}};

    LinearRegression model{};
    model.fit(XTrain, yTrain);
    model.save("test.bin");

    std::cout << "Train Score: " << model.score(XTrain, yTrain) << '\n';
    std::cout << "Test Score:" << model.score(XTest, yTest) << '\n';
}
*/
