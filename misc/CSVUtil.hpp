#pragma once

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace CSVUtil {
    // Count the no. of cols, returns 0 if line is incomplete
    inline std::size_t countCols(const std::string &line, const char delim = ',') {
        bool insideStr {false};
        std::size_t cols {1};
        for (const char &ch: line) {
            if (!insideStr && ch == delim) cols++;
            else if (ch == '"') insideStr = !insideStr;
        }
        return insideStr? 0: cols;
    }

    // Parse a single CSV line into a vector of strings (assumes that the line is 'complete')
    inline std::vector<std::string> parseCSVLine(const std::string &line, const char delim = ',') {
        std::vector<std::string> result;
        bool insideStr {false};
        std::string acc; std::size_t idx {0}, N {line.size()};
        while (idx < N) {
            if (!insideStr && line[idx] == delim) {
                result.push_back(acc);
                acc.clear();
            } else if (line[idx] == '"') {
                if (insideStr && idx + 1 < N && line[idx + 1] == '"') {
                    acc += '"'; idx++;
                } else insideStr = !insideStr;
            } else {
                acc += line[idx];
            }

            idx++;
        }

        result.push_back(acc);
        return result;
    }

    // Print out the rows / cols in a CSV file
    inline std::pair<std::size_t, std::size_t> stat(const std::string &fname) {
        std::size_t rows {0}, cols {0};
        std::ifstream ifs {fname};
        if (!ifs) std::cerr << "'" << fname << "' is not a valid file.\n";
        else {
            std::string line, acc;
            while (std::getline(ifs, line)) {
                acc += line;
                std::size_t currCols {countCols(acc)};
                if (!currCols) acc += "\n";
                else {
                    if (cols > 0 && cols != currCols) {
                        std::cerr << "Count of columns doesn't match with the first row."
                            << "Expected: " << cols << "; Found: " << currCols << "\n";
                        break;
                    } 

                    cols = currCols;
                    rows++; acc.clear();
                }
            }
        }

        // Print out the stats
        return {rows, cols};
    }

    /*
     * @class CSVReader
     * 
     * This class reads a CSV file and provides an iterator-based interface 
     * for processing rows. Each row is returned as a `std::vector<std::string>`.
     *
     * Example usage:
     * @code
     *   CSVUtil::CSVReader reader{argv[1]};
     *   for (const std::vector<std::string> &row: reader) {
     *       for (const std::string &field: row)
     *           std::cout << field << ",";
     *       std::cout << "\n";
     *   }
     * @endcode
     */
    class CSVReader {
        private:
            const std::string fname;
            mutable std::size_t cols;
            mutable std::ifstream csvStream;
            mutable std::vector<std::string> currentRow;

        public:
            class CSVIterator {
                private:
                    const CSVReader *reader;
                    bool end;

                public:
                    CSVIterator(const CSVReader *reader, bool end = false): 
                        reader(reader), end(end) { ++(*this); }

                    CSVIterator &operator++() {
                        if (!end) end = !reader->nextCSVLine();
                        return *this;
                    }

                    const std::vector<std::string> &operator*() const {
                        return reader->currentRow;
                    }

                    bool operator!=(const CSVIterator &other) const {
                        return end != other.end;
                    }
            };

            CSVReader(const std::string &fname, std::size_t cols = 0):
                fname(fname), cols(cols), csvStream(fname)
            {
                if (!csvStream) 
                    throw std::runtime_error("Failed to open file: " + fname);
            }

            // Get a single CSV line & store it the result into `currentRow`
            bool nextCSVLine() const {
                std::string acc, line; 
                currentRow.clear();
                while (std::getline(csvStream, acc)) {
                    line += acc;
                    std::size_t currCols {countCols(line)};
                    if (!currCols) {
                        line += "\n"; 
                    } else if (cols > 0 && currCols != cols) {
                        throw std::runtime_error("CSV file column count doesn't match.\n");
                    } else {
                        currentRow = parseCSVLine(line);
                        cols = currCols;
                        break;
                    }
                }
                return !currentRow.empty();
            }

            // Define the const iterators
            CSVIterator end() const { return CSVIterator(this, true); }
            CSVIterator begin() const {
                csvStream.clear();
                csvStream.seekg(0, std::ios::beg);
                return CSVIterator(this); 
            }
    };
}
