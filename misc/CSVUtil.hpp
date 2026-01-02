#pragma once

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace CSVUtil {
    // Unwrap the optional layer
    template<typename T> struct UnwrapOptional { using type = T; };
    template<typename T> struct UnwrapOptional<std::optional<T>> { using type = T; };
    template<typename T> using Unwrapped = typename UnwrapOptional<T>::type;

    // Simple Proxy Convertible type
    template<typename T>
    concept SimpleConvertible = (
        (std::is_arithmetic_v<Unwrapped<T>> && !std::is_same_v<Unwrapped<T>, bool>) || 
        std::is_same_v<Unwrapped<T>, std::string>
    );

    // Helper to remove '/r' when reading lines
    inline bool safeGetline(std::istream &is, std::string &line) {
        if (!std::getline(is, line)) return false;
        if (!line.empty() && line.back() == '\r') 
            line.pop_back();
        return true;
    }

    // Parse a single CSV line into a vector of strings, returns empty is line is incomplete
    inline std::vector<std::string> parseCSVLine(const std::string &line, const char delim = ',', const char quoteChar = '"') {
        std::vector<std::string> result;
        bool insideStr {false};
        std::string acc; std::size_t N {line.size()};
        for (std::size_t idx {0}; idx < N; idx++) {
            // Delim is considered only if we are not inside a quoted field
            if (!insideStr && line[idx] == delim) {
                result.emplace_back(acc);
                acc.clear();
            } 

            // Start quote IFF it is present at the beginning for a field
            else if (!insideStr && line[idx] == quoteChar && (idx == 0 || line[idx - 1] == delim)) {
                insideStr = true;
            } 

            // Might be the end of a quoted field or could be simply escaped
            else if (insideStr && line[idx] == quoteChar) {
                if (idx + 1 < N && line[idx + 1] != delim && line[idx + 1] != quoteChar) {
                    throw std::runtime_error("Not a valid CSV: " + line);
                } else if (idx + 1 < N && line[idx + 1] == quoteChar) {
                    acc += quoteChar; idx++;
                } else {
                    insideStr = false;
                }
            }

            else {
                acc += line[idx];
            }
        }

        result.emplace_back(acc);
        if (insideStr) result.clear();

        return result;
    }

    // Helper to output a single CSV field as a string
    inline std::string writeCSVField(const std::string_view field, const char delim = ',') {
        std::string result;
        bool splChars {false};
        for (const char &ch: field) {
            if (ch == '"') result += '"';
            result += ch;
            splChars |= (
                ch == '"' || ch == delim || ch == '\n'
             || ch == '\r' || ch == '\f'
            );
        }

        // Enclose inside quotes if any of spl char exist
        if (splChars) result = '"' + result + '"';
        return result;
    }

    // Output a single CSV line as a string
    inline std::string writeCSVLine(const std::vector<std::string> &row, const char delim = ',') {
        std::string result;
        for (const std::string &field: row)
            result += writeCSVField(field, delim) + ',';

        if (!result.empty()) result.pop_back();
        return result;
    }

    class CSVRecord {
        private:
            mutable std::string serialized;
            mutable std::vector<std::string> records;
            mutable bool modified {false};

            class Proxy {
                private:
                    CSVRecord &record;
                    std::size_t idx;

                public:
                    Proxy(CSVRecord &record, std::size_t idx): record(record), idx(idx) {}
                    Proxy &operator= (const std::string &val) { 
                        record.records[idx] = val; 
                        record.modified = true; 
                        return *this; 
                    }

                    inline operator std::string&() const { return record.records[idx]; }
                    inline bool empty() const { return record.records[idx].empty(); }

                    inline friend std::ostream &operator<<(std::ostream &os, const Proxy& field) {
                        os << field.record.records[field.idx];
                        return os;
                    }

                    template<SimpleConvertible T>
                    inline void operator>>(T &val) const {
                        std::string field {record.records[idx]};
                        if constexpr(std::is_same_v<T, std::string>) {
                            val = field;
                        } else {
                            std::from_chars_result parseRes {std::from_chars(field.data(), field.data() + field.size(), val)};
                            if(parseRes.ec != std::errc{} || parseRes.ptr != field.data() + field.size()) {
                                throw std::runtime_error(
                                    "Failed to parse field " + field + " (#" + 
                                    std::to_string(idx) + ") as type: " + 
                                    std::string{typeid(T).name()});
                            }
                        }
                    }

                    template<SimpleConvertible T>
                    inline void operator>>(std::optional<T> &val) const {
                        if (empty()) val = std::nullopt;
                        else {
                            T tmp; this->operator>>(tmp);
                            val = std::move(tmp);
                        }                    
                    }
            };

        public:
            explicit CSVRecord() {}

            explicit CSVRecord(const std::string &serialized, const std::vector<std::string> &records = {}): 
                serialized(serialized), 
                records(records.empty()? parseCSVLine(serialized): records) 
            {}

            explicit CSVRecord(const std::vector<std::string> &records):
                serialized(writeCSVLine(records)), records(records)
            {}

            bool empty() const { return serialized.empty(); }
            std::size_t memory() const { return serialized.size() + 1; }
            std::size_t size() const { return records.size(); }
            const std::string &to_string() const { 
                if (modified) { serialized = writeCSVLine(records); modified = false; }
                return serialized; 
            }

            std::vector<std::string>::const_iterator begin() const { return records.cbegin(); }
            std::vector<std::string>::const_iterator end() const { return records.cend(); }

            Proxy operator[] (std::size_t idx) {
                if (idx >= records.size()) 
                    throw std::runtime_error("Index out of bounds: " + std::to_string(idx));
                return Proxy{*this, idx};
            }

            Proxy operator[](std::size_t idx) const {
                if (idx >= records.size()) 
                    throw std::runtime_error("Index out of bounds: " + std::to_string(idx));
                return Proxy{const_cast<CSVRecord&>(*this), idx};
            }

            // Unpack CSV row into a tuple, i.e unpack(row);
            template<typename ...Args> requires(SimpleConvertible<Args> && ...)
            void unpack(std::tuple<Args...> &tuple) const {
                std::apply(
                    [this](Args&... elems) { unpack(elems...); },
                    tuple
                );
            }

            // Unpack CSV row into a tuple of args, i.e unpack(col1, col2, ...);
            template<typename ...Args> requires(SimpleConvertible<Args> && ...)
            void unpack(Args &...values) const {
                if (sizeof...(Args) != records.size())
                    throw std::runtime_error(
                        "CSV to Tuple conversion failed: CSV only has " + 
                        std::to_string(records.size()) + " fields");

                // Parse into the tuple structure
                std::apply([this](auto&... arg) {
                    std::size_t idx {0}; (((*this)[idx++] >> arg), ...);
                }, std::tie(values...));
            }
    };

    // Overload stream operators
    inline std::ostream &operator<< (std::ostream &os, const CSVRecord &record) {
        os << record.to_string(); 
        return os;
    }

    // Extract the first line from a CSV File
    inline CSVRecord extractHeader(const std::string &fname) {
        std::ifstream ifs {fname};
        if (!ifs) throw std::runtime_error("Couldn't open file: " + fname);
        std::string header, line;

        // Continue reading header incase it spans multiple lines
        while (safeGetline(ifs, line)) {
            header += line;
            std::vector<std::string> row {parseCSVLine(header)};
            if (row.empty()) header += '\n'; 
            else return CSVRecord{header, row};
        }

        throw std::runtime_error("Failed to read the header: " + fname);
    }

    /*
     * @class CSVReader
     * 
     * This class reads a CSV file and provides an iterator-based interface 
     * for processing rows. Each row is returned as a `CSVRecord`.
     *
     * Example usage:
     * @code
     *   CSVUtil::CSVReader reader{argv[1]};
     *   for (const CSVRecord &row: reader) {
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
            mutable CSVRecord currentRow;
            std::size_t skipRows = 0;

        public:
            class CSVIterator {
                private:
                    const CSVReader *reader;
                    bool end;

                public:
                    CSVIterator(const CSVReader *reader, bool end = false): 
                        reader(reader), end(end) 
                    { 
                            ++(*this); // Init reader state
                            for (std::size_t i {}; i < reader->skipRows; ++i)
                                ++(*this);
                    }

                    CSVIterator &operator++() {
                        if (!end) end = !reader->nextCSVLine();
                        return *this;
                    }

                    const CSVRecord &operator*() const { return reader->currentRow; }
                    CSVRecord &operator*() { return reader->currentRow; }

                    bool operator!=(const CSVIterator &other) const {
                        return end != other.end;
                    }
            };

            CSVReader(const std::string &fname, std::size_t cols = 0, bool skipRows = 0):
                fname(fname), cols(cols), csvStream(fname), skipRows(skipRows)
            {
                if (!csvStream) 
                    throw std::runtime_error("Failed to open file: " + fname);
            }

            // Get a single CSV line & store it the result into `currentRow`
            bool nextCSVLine() const {
                std::string acc, line; 
                currentRow = CSVRecord{};
                while (safeGetline(csvStream, acc)) {
                    line += acc;
                    std::vector<std::string> row {parseCSVLine(line)};
                    if (row.empty()) {
                        line += '\n'; 
                    } else if (cols > 0 && row.size() != cols) {
                        throw std::runtime_error(
                            "CSV file column count doesn't match.\n"
                            "Expected: " + std::to_string(cols) + "; "
                            "Found: " + std::to_string(row.size()) + "\n"
                        );
                    } else {
                        currentRow = CSVRecord{line, row};
                        cols = row.size();
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
