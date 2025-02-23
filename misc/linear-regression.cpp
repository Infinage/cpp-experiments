#include <fstream>
#include <iostream>
#include <ostream>
#include <random>
#include <stdexcept>
#include <vector>

// Compilation: g++ linear-regression.cpp -shared -o CPPLearn.so -fPIC -std=c++23 -O2 $(python3-config --includes --ldflags)
// Typehints: stubgen -p CPPLearn -o .
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
namespace py = pybind11;

namespace CPPLearn {
    namespace Core {
        /* Wrapper over std::vector for some `vectorized` operation support */
        template <typename T> requires std::is_arithmetic_v<T>
        class Vector {
            private:
                std::vector<T> data;

                template <typename Op>
                static Vector binaryOp(const Vector<T> &vec1, const Vector<T> &vec2, Op op) {
                    std::size_t N {vec1.size()};
                    if (N != vec2.size()) throw std::runtime_error("Dimension mismatch, cannot broadcast.");
                    Vector result {N};
                    for (std::size_t idx {0}; idx < N; idx++)
                        result[idx] = op(vec1[idx], vec2[idx]);
                    return result;
                }

                template <typename Op>
                static Vector binaryOp(const Vector<T> &vec, const T &val, Op op) {
                    std::size_t N {vec.size()};
                    Vector result {N};
                    for (std::size_t idx {0}; idx < N; idx++)
                        result[idx] = op(vec[idx], val);
                    return result;
                }

            public:
                const T* ptr() const { return data.data(); }
                std::size_t size() const { return data.size(); }

                Vector(): data() {}
                Vector(const std::vector<T> &data): data(data) {}
                Vector(const std::size_t len, const T initVal = T{}): 
                    data(len, initVal) {}

                // Access operators
                inline T &operator[](std::size_t idx) { return data[idx]; }
                inline const T &operator[](std::size_t idx) const { return data[idx]; }

                template<typename Op>
                void apply(Op op) const { 
                    for (const T &val: data) op(val); 
                }

                T sum() const {
                    T result {static_cast<T>(0)};
                    apply([&result](const T& val) { result += val; });
                    return result;
                }

                T mean() const {
                    return sum() / size();
                }

                // Vector, Vector ops
                Vector operator+(const Vector<T> &other) const { return binaryOp(*this, other, std::plus<>{}); }
                Vector operator-(const Vector<T> &other) const { return binaryOp(*this, other, std::minus<>{}); }

                // Matrix, Constant ops
                Vector operator+(const T &other) const { return binaryOp(*this, other, std::plus<>{}); }
                Vector operator-(const T &other) const { return binaryOp(*this, other, std::minus<>{}); }
                Vector operator*(const T &other) const { return binaryOp(*this, other, std::multiplies<>{}); }
                Vector operator/(const T &other) const { 
                    if (other == 0) throw std::runtime_error("Cannot divide by zero.");
                    return binaryOp(*this, other, std::divides<>{}); 
                }
        };

        /* Templated Matrix class for better efficiency */
        template <typename T> requires std::is_arithmetic_v<T>
        class Matrix {
            private:
                std::vector<T> data;

                template <typename Op>
                static Matrix binaryOp(const Matrix<T> &mat1, const Matrix<T> &mat2, Op op) {
                    if (mat1.rows != mat2.rows || mat1.cols != mat2.cols)
                        throw std::runtime_error("Dimensions do not match.");
                    Matrix result {mat1.rows, mat1.cols};
                    for (std::size_t row {0}; row < mat1.rows; row++)
                        for (std::size_t col {0}; col < mat1.cols; col++)
                       result(row, col) = op(mat1(row, col), mat2(row, col)); 
                    return result;
                }

                template <typename Op>
                static Matrix binaryOp(const Matrix<T> &mat, const Vector<T> &vec, Op op) {
                    if (vec.size() != mat.cols) throw std::runtime_error("Dimension mismatch, cannot broadcast.");
                    Matrix result {mat.rows, mat.cols};
                    for (std::size_t row {0}; row < mat.rows; row++)
                        for (std::size_t col {0}; col < mat.cols; col++)
                            result(row, col) = op(mat(row, col), vec[col]);
                    return result;
                }

                template <typename Op>
                static Matrix binaryOp(const Matrix &mat, const T &val, Op op) {
                    Matrix result {mat.rows, mat.cols};
                    for (std::size_t row {0}; row < mat.rows; row++)
                        for (std::size_t col {0}; col < mat.cols; col++)
                       result(row, col) = op(mat(row, col), val); 
                    return result;
                }

            public:
                const T* ptr() const { return data.data(); }
                static Matrix<double> randn(const std::size_t rows, const std::size_t cols, std::size_t seed) {
                    std::mt19937 random_gen {seed};
                    std::uniform_real_distribution<double> dist{-1, 1};
                    Matrix<double> result{rows, cols};
                    for (std::size_t i {0}; i < rows; i++)
                        for (std::size_t j {0}; j < cols; j++)
                            result(i, j) = dist(random_gen);
                    return result;
                }

                mutable std::size_t rows, cols;

                // Default constructor
                Matrix(): rows(0), cols(0) {}

                // Construct from a 2D vector
                Matrix(const std::vector<std::vector<T>> &mat): 
                    rows(mat.size()), cols(mat.empty()? 0: mat[0].size()) 
                {
                    data.reserve(rows * cols);
                    for (const std::vector<T> &row: mat)
                        for (const T& val: row) data.push_back(val);
                }

                // Construct from pointers - py::array_t
                Matrix(const T* arr, const std::size_t rows, const std::size_t cols): 
                    data(arr, arr + (rows * cols)), rows(rows), cols(cols)
                {}

                // Zero Matrix
                Matrix(const std::size_t rows, const std::size_t cols): 
                    data(rows * cols, 0), rows(rows), cols(cols) {}

                // Copy constructor
                Matrix(const Matrix &mat): data(mat.data), rows(mat.rows), cols(mat.cols) {}

                inline Matrix& operator=(Matrix other) {
                    data = std::move(other.data);
                    rows = other.rows;
                    cols = other.cols;
                    return *this;
                }

                // Index 1D array with 2D indices
                inline T &operator()(std::size_t row, std::size_t col) {
                    return data[row * cols + col];
                }

                // Index 1D array with 2D indices
                inline const T &operator()(std::size_t row, std::size_t col) const {
                    return data[row * cols + col];
                }

                template<typename Op>
                void apply(Op op) const { 
                    for (const T &val: data) op(val); 
                }

                template<typename Op>
                Vector<T> reduce(const short axis, const T initVal, Op op) const {
                    Vector result(axis == 0? cols: rows, initVal);
                    for (std::size_t row {0}; row < rows; row++) {
                        for (std::size_t col {0}; col < cols; col++) {
                            if (axis == 0) 
                                result[col] = op(result[col], (*this)(row, col));
                            else 
                                result[row] = op(result[row], (*this)(row, col));
                        }
                    }
                    return result;
                }

                T sum() const {
                    T result {static_cast<T>(0)};
                    apply([&result](const T& val) { result += val; });
                    return result;
                }

                T mean() const {
                    return sum() / (rows * cols);
                }

                Vector<T> sum(short axis) {
                    return reduce(axis, 0, std::plus<>{});
                }

                Vector<T> mean(short axis) {
                    return reduce(axis, 0, std::plus<>{}) / static_cast<T>(axis == 0 ? rows: cols);
                }

                static Matrix dot(const Matrix &mat1, const Matrix &mat2) {
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

                Matrix transpose() const {
                    Matrix result{cols, rows};
                    for (std::size_t i {0}; i < rows; i++)
                        for (std::size_t j {0}; j < cols; j++)
                            result(j, i) = (*this)(i, j);

                    return result;
                }

                // Matrix, Matrix ops
                Matrix operator+(const Matrix<T> &other) const { return binaryOp(*this, other, std::plus<>{}); }
                Matrix operator-(const Matrix<T> &other) const { return binaryOp(*this, other, std::minus<>{}); }
                Matrix operator*(const Matrix<T> &other) const { return binaryOp(*this, other, std::multiplies<>{}); }

                // Matrix, Vector ops
                Matrix operator+(const Vector<T> &other) const { return binaryOp(*this, other, std::plus<>{}); }
                Matrix operator-(const Vector<T> &other) const { return binaryOp(*this, other, std::minus<>{}); }

                // Matrix, Constant ops
                Matrix operator+(const T &other) const { return binaryOp(*this, other, std::plus<>{}); }
                Matrix operator-(const T &other) const { return binaryOp(*this, other, std::minus<>{}); }
                Matrix operator*(const T &other) const { return binaryOp(*this, other, std::multiplies<>{}); }
                Matrix operator/(const T &other) const { 
                    if (other == 0) throw std::runtime_error("Cannot divide by zero.");
                    return binaryOp(*this, other, std::divides<>{}); 
                }
        };

        /* ------------ Print utils ------------ */
        template <typename T>
        std::ostream &operator<<(std::ostream &oss, const Vector<T> &vec) {
            std::size_t N {vec.size()};
            oss << "[ ";
            for (std::size_t i {0}; i < N; i++)
                oss << vec[i] << ' ';
            oss << ']';
            return oss;
        }

        template <typename T>
        std::ostream &operator<<(std::ostream &oss, const Matrix<T> &mat) {
            std::size_t rows {mat.rows}, cols {mat.cols};
            oss << "[ ";
            for (std::size_t i {0}; i < rows; i++) {
                oss << "[ ";
                for (std::size_t j {0}; j < cols; j++)
                    oss << mat(i, j) << ' ';
                oss << (i < rows - 1? "]\n": "] ]");
            }
            return oss;
        }
        /* ------------ Print utils ------------ */
    }

    namespace Models {
        class LinearRegression {
            mutable std::size_t seed;
            Core::Matrix<double> weights;
            Core::Vector<double> bias;

            public:
                const Core::Matrix<double> &getWeights() const { return weights; }
                const Core::Vector<double> &getBias() const { return bias; }
                LinearRegression(std::size_t seed = 42): seed(seed) {}

                LinearRegression& fit(
                    const Core::Matrix<double> &X, 
                    const Core::Matrix<double> &y,
                    std::size_t iterations=250, 
                    double learningRate=0.01
                ) {
                    std::size_t nSamples {X.rows}, nFeats {X.cols}, nTargets {y.cols};
                    if (nSamples != y.rows) throw std::runtime_error("No. of samples do not match.");

                    if (weights.rows == 0) {
                        weights = Core::Matrix<double>::randn(nFeats, nTargets, seed);
                        bias = Core::Vector<double>(nTargets, 0);
                    }

                    Core::Matrix<double> xTransposed {X.transpose()};
                    for (std::size_t iter {0}; iter < iterations; iter++) {
                        Core::Matrix<double> yDelta {y - predict(X)};
                        Core::Matrix<double> dW {Core::Matrix<double>::dot(xTransposed, yDelta) * (-2. / nSamples)};
                        Core::Vector<double> dB {yDelta.sum(0) * (-2. / nSamples)};
                        weights = weights - (dW * learningRate);
                        bias = bias - (dB * learningRate);
                    }

                    return *this;
                }

                const Core::Matrix<double> predict(const Core::Matrix<double> &X) const {
                    if (weights.rows == 0) throw std::runtime_error("Model not fit yet.");
                    return Core::Matrix<double>::dot(X, weights) + bias;
                }

                double score(const Core::Matrix<double> &X, const Core::Matrix<double> &y) const {
                    const Core::Matrix<double> yDelta {y - predict(X)};
                    return (yDelta * yDelta).sum();
                }

                void save(const std::string &fpath) const {
                    if (weights.rows == 0) throw std::runtime_error("Model not fit yet.");

                    std::ofstream ofs {fpath, std::ios::binary | std::ios::out};
                    if (!ofs) throw std::runtime_error("Cannot open file for saving the model.");
                    
                    char HEADER[9] {"CPPLEARN"};
                    ofs.write(HEADER, 9);

                    std::size_t M {weights.rows}, N {bias.size()};
                    ofs.write(reinterpret_cast<const char*>(&seed), sizeof(seed));
                    ofs.write(reinterpret_cast<const char*>(&M), sizeof(M));
                    ofs.write(reinterpret_cast<const char*>(&N), sizeof(N));

                    // Write out the weights
                    for (std::size_t i {0}; i < M; i++) {
                        for (std::size_t j {0}; j < N; j++) {
                            const double &val {weights(i, j)};
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
                    if (!ifs) throw std::runtime_error("Cannot open file for reading the model.");

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
                    weights = Core::Matrix<double>{M, N};
                    for (std::size_t i {0}; i < M; i++) {
                        for (std::size_t j {0}; j < N; j++) {
                            ifs.read(reinterpret_cast<char *>(&weights(i, j)), sizeof(weights(i, j)));
                            if (ifs.fail()) throw std::runtime_error("Corrupted file, failed to read the weights.");
                        }
                    }

                    // Read the bias
                    bias = Core::Vector<double>{N, 0};
                    for (std::size_t j {0}; j < N; j++) {
                        ifs.read(reinterpret_cast<char *>(&bias[j]), sizeof(bias[j]));
                        if (ifs.fail()) throw std::runtime_error("Corrupted file, failed to read the bias.");
                    }
                }
        };
    }

    namespace Utils {
        Core::Matrix<double> from_numpy2D(const py::array_t<double> &array) {
            const py::buffer_info &buf {array.request()};
            if (buf.ndim != 2) 
                throw std::runtime_error("Expected a 2D array, got " + std::to_string(buf.ndim) + "D instead.");

            // Convert from double* to our custom class Matrix
            return Core::Matrix<double> {
                reinterpret_cast<double*>(buf.ptr), 
                static_cast<std::size_t>(array.shape(0)), 
                static_cast<std::size_t>(array.shape(1))
            };
        }

        py::array_t<double> to_numpy(const Core::Matrix<double> &mat) {
            const double *data {mat.ptr()};
            return py::array_t<double>(
                {mat.rows, mat.cols}, {sizeof(double) * mat.cols, sizeof(double)}, data
            );
        }

        py::array_t<double> to_numpy(const Core::Vector<double> &vec) {
            const double *data {vec.ptr()};
            return py::array_t<double>(
                {vec.size()}, {sizeof(double)}, data
            );
        }
    }
}

/* PYBIND11 Syntax for enabling imports from python */
PYBIND11_MODULE(CPPLearn, root) {
    root.doc() = "Scikit Learn with C++";
    py::module_ models {root.def_submodule("models", "Machine Learning Models")};

    py::class_<CPPLearn::Models::LinearRegression>(models, "LinearRegression")
        .def(py::init<const std::size_t>(), py::arg("seed") = 42)
        .def("save", &CPPLearn::Models::LinearRegression::save, "Saves the trained model to disk as a binary.", py::arg("fPath") = "model.bin")
        .def("load", &CPPLearn::Models::LinearRegression::load, "Loads a trained model from disk.", py::arg("fpath") = "model.bin")

        .def_property_readonly("weights", [](CPPLearn::Models::LinearRegression &self) -> py::array_t<double> { 
            return CPPLearn::Utils::to_numpy(self.getWeights()); 
        }, "Get the weights of the model.")

        .def_property_readonly("bias", [](CPPLearn::Models::LinearRegression &self) -> py::array_t<double> { 
            return CPPLearn::Utils::to_numpy(self.getBias()); 
        }, "Get the bias of the model.")
                        
        .def("fit", [](
            CPPLearn::Models::LinearRegression &self, 
            const py::array_t<double> &XTrain, const py::array_t<double> &YTrain, 
            std::size_t iterations, double learningRate
        ) {
            return self.fit(CPPLearn::Utils::from_numpy2D(XTrain), CPPLearn::Utils::from_numpy2D(YTrain), iterations, learningRate);
        }, "Fit the model against provided set of inputs.",
        py::arg("XTrain"), py::arg("YTrain"), py::arg("iterations") = 250, py::arg("learningRate") = 0.01
        )

        .def("predict", [](CPPLearn::Models::LinearRegression &self, py::array_t<double> &XTest) -> py::array_t<double> {
            return CPPLearn::Utils::to_numpy(self.predict(CPPLearn::Utils::from_numpy2D(XTest)));
        }, "Returns predictions against a set of inputs. Requires that the model be fit already.", py::arg("XTest"))

        .def("score", [](CPPLearn::Models::LinearRegression &self, py::array_t<double> &XTest, py::array_t<double> &YTest) {
            return self.score(CPPLearn::Utils::from_numpy2D(XTest), CPPLearn::Utils::from_numpy2D(YTest));
        }, "Computes the R2 score of the trained model against provided inputs.", py::arg("XTest"), py::arg("YTest"));
}
