#include "CSVUtil.hpp"
#include "ThreadPool.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <unordered_set>

class SplitStrategy {
    public:
        virtual std::size_t getBucket(const CSVUtil::CSVRecord&) = 0;
        virtual ~SplitStrategy() = default;
        virtual std::size_t totalBuckets() const = 0;
};

class RecordCountStrategy: public SplitStrategy {
    const std::size_t maxCounts;
    std::size_t currBucket {0}, currBucketCount {0};
    std::vector<std::size_t> &pruneBuckets;

    public:
        RecordCountStrategy(const std::size_t counts, std::vector<std::size_t> &pruneBuckets): 
            maxCounts(counts), pruneBuckets(pruneBuckets) {}

        std::size_t totalBuckets() const override { return currBucket + 1; }

        std::size_t getBucket(const CSVUtil::CSVRecord&) override {
            if (currBucketCount >= maxCounts) {
                currBucketCount = 0;
                pruneBuckets.push_back(currBucket++);
            } 

            currBucketCount++;
            return currBucket;
        }
};

class SplitSizeStrategy: public SplitStrategy {
    const std::size_t maxSize;
    std::size_t currBucket {0}, currBucketSize {0};
    std::vector<std::size_t> &pruneBuckets;

    public:
        SplitSizeStrategy(const std::size_t size, std::vector<std::size_t> &pruneBuckets): 
            maxSize(size), pruneBuckets(pruneBuckets) {}

        std::size_t totalBuckets() const override { return currBucket + 1; }

        std::size_t getBucket(const CSVUtil::CSVRecord &row) override {
            if (currBucketSize >= maxSize * 1024 * 1024) {
                currBucketSize = 0; 
                pruneBuckets.push_back(currBucket++);
            }

            currBucketSize += row.memory();
            return currBucket;
        }
};

class HashColumnStrategy: public SplitStrategy {
    const std::size_t colIdx, bucketSize;
    const std::hash<std::string> hasher{};
    std::unordered_set<std::size_t> buckets;

    public:
        HashColumnStrategy(const std::size_t colIdx, std::size_t bucketSize): 
            colIdx(colIdx), bucketSize(bucketSize) {}

        std::size_t totalBuckets() const override { return buckets.size(); }

        std::size_t getBucket(const CSVUtil::CSVRecord &row) override {
            std::size_t bucket {hasher(row[colIdx]) % bucketSize};
            buckets.insert(bucket);
            return bucket;
        }
};

class GroupColumnStrategy: public SplitStrategy {
    const std::size_t colIdx, groupSize;
    std::unordered_map<std::string, std::size_t> field2BucketMapping;
    std::unordered_map<std::size_t, std::size_t> bucketCounts;
    std::size_t currBucket{0};

    public:
        GroupColumnStrategy(const std::size_t colIdx, std::size_t groupSize): 
            colIdx(colIdx), groupSize(groupSize) {}

        std::size_t totalBuckets() const override { return currBucket + 1; }

        std::size_t getBucket(const CSVUtil::CSVRecord &row) override {
            const std::string &field {row[colIdx]};
            if (field2BucketMapping.find(field) != field2BucketMapping.end()) {
                return field2BucketMapping[field];
            } else if (bucketCounts[currBucket] < groupSize) {
                bucketCounts[currBucket]++;
                return field2BucketMapping[field] = currBucket;
            } else {
                currBucket++;
                bucketCounts[currBucket]++;
                return field2BucketMapping[field] = currBucket;
            }
        }
};

class CSVSplit {
    private:
        // Map of open file handles
        std::unordered_map<std::size_t, std::ofstream> outputHandles;

        // Variables set via constructor
        const std::string ifname;
        const std::filesystem::path outDir;
        std::unique_ptr<SplitStrategy> strategy;
        std::vector<std::size_t> &pruneBuckets;

        // Variables created at constructor
        const CSVUtil::CSVReader readHandle;
        const std::size_t N_COLS;
        const std::string CSVHeader;

    private:
        // Write & flush
        void flushBuffer(std::size_t bucket, std::unordered_map<std::size_t, std::ostringstream> &buffers) {
            if (buffers[bucket].tellp() > 0) {
                outputHandles[bucket] << buffers[bucket].str();
                outputHandles[bucket].flush();
                buffers[bucket].str("");
                buffers[bucket].clear();
            }
        }

    public:
        CSVSplit (const std::string &ifname, const std::string &outDir, std::unique_ptr<SplitStrategy> strategy, std::vector<std::size_t> &pruneBuckets): 
            ifname(ifname), outDir(outDir),
            strategy(std::move(strategy)), 
            pruneBuckets(pruneBuckets),
            readHandle(CSVUtil::CSVReader{ifname}), 
            N_COLS((*readHandle.begin()).size()), 
            CSVHeader({CSVUtil::extractHeader(ifname) + "\n"})
        {}

        // Splits a CSV file based on provided column & hash function into N buckets
        void splitFile() {
            std::size_t counts {0};
            std::unordered_map<std::size_t, std::ostringstream> buffers;
            for (const CSVUtil::CSVRecord &row: readHandle) {
                if (counts++) {
                    std::size_t bucket {strategy->getBucket(row)};

                    // Create handle for the bucket if new
                    if (outputHandles.find(bucket) == outputHandles.end()) {
                        std::string ofname {"split-" + std::to_string(bucket + 1) + ".csv"};
                        outputHandles[bucket] = std::ofstream{outDir / ofname};
                        outputHandles[bucket] << CSVHeader;
                    }

                    // Buffer until there is 1 MB worth data
                    buffers[bucket] << row << '\n';
                    if (buffers[bucket].tellp() >= static_cast<long>(1024 * 1024))
                        flushBuffer(bucket, buffers);

                    // Prune any handles no longer needed (updated by SplitStrategy)
                    while (!pruneBuckets.empty()) {
                        std::size_t pruneBucket {pruneBuckets.back()};
                        flushBuffer(pruneBucket, buffers);
                        buffers.erase(pruneBucket); 
                        outputHandles.erase(pruneBucket);
                        pruneBuckets.pop_back(); 
                    }
                }
            }

            // Flush any unwritten content
            for (std::pair<const std::size_t, std::ostringstream> &kv: buffers)
                flushBuffer(kv.first, buffers); 

            std::cout << "Read CSV records: " << counts << "\n"
                      << "Files created: " << strategy->totalBuckets() << "\n";
        }
};

class CSVMerge {
    private:
        const std::string csvHeader;
        const std::filesystem::path outDir;

        // Write & flush if threshold is hit
        static void writeBuffer(
                const CSVUtil::CSVRecord &csvRow, std::ostringstream &buffer, 
                std::ofstream &ofile, std::size_t thresh, 
                std::mutex *mutex = nullptr
        ) {
            if (!csvRow.empty()) buffer << csvRow << '\n';
            if (buffer.tellp() >= static_cast<long>(thresh)) {
                if (mutex) { std::lock_guard lock(*mutex); ofile << buffer.str(); ofile.flush(); }
                else { ofile << buffer.str(); ofile.flush(); }
                buffer.str(""); buffer.clear();
            }
        }

        void mergeSync(const std::vector<std::string> &files) {
            std::ofstream ofile {outDir / "merged-sync.csv"}; 
            std::ostringstream buffer;
            ofile << csvHeader;
            ofile.flush();
            std::size_t fileCounts{0}, recCounts{0};
            for (const std::string &fname: files) {
                fileCounts++;
                std::size_t counts {0};
                for (const CSVUtil::CSVRecord &row: CSVUtil::CSVReader{fname}) {
                    if (counts++) writeBuffer(row, buffer, ofile, 1024 * 1024);
                }

                // Update for each file
                recCounts += counts;
            }

            // Final write (dont care about thresholds, write empty and just flush)
            writeBuffer(CSVUtil::CSVRecord{}, buffer, ofile, 0);

            std::cout << "Read CSV Files: " << fileCounts << "\n"
                      << "Records written: " << recCounts << "\n";
        }

        void mergeAsync(const std::vector<std::string> &files) {
            std::ofstream ofile {outDir / "merged-async.csv"};
            ofile << csvHeader; ofile.flush();
            std::mutex ofileMutex;
            std::size_t fileCounts{0};
            std::atomic_size_t recCounts{0};
            ThreadPool<std::function<void()>> pool(std::thread::hardware_concurrency());
            for (const std::string &fname: files) {
                fileCounts++;
                pool.enqueue([fname, &recCounts, &ofile, &ofileMutex]() {
                    std::ostringstream buffer;
                    std::size_t counts {0};
                    for (const CSVUtil::CSVRecord &row: CSVUtil::CSVReader{fname}) {
                        if (counts++) writeBuffer(row, buffer, ofile, 1024 * 1024, &ofileMutex);
                    }

                    // Final write (dont care about thresholds, write empty and just flush)
                    writeBuffer(CSVUtil::CSVRecord{}, buffer, ofile, 0, &ofileMutex);
                    recCounts += counts;
                });
            }

            // Wait for all threads to complete
            pool.wait();

            std::cout << "Read CSV Files: " << fileCounts << "\n"
                      << "Records written: " << recCounts << "\n";
        }

    public:
        CSVMerge(const std::string &header, const std::string &outDir): 
            csvHeader(header + "\n"), outDir(outDir) {}

        void mergeFiles(const std::vector<std::string> &files, bool sync) {
            if (sync) mergeSync(files);
            else mergeAsync(files);
        }
};

class CSVStat {
    public:
        static void statFiles(const std::vector<std::string> &files) {
            // Create a thread pool to add tasks to queue
            ThreadPool<std::function<void()>> pool(std::thread::hardware_concurrency());

            // Variables to track total stats if there are multiple files
            std::size_t totalRows {0}, totalCols {0}, totalLines {0}; 
            double totalSize {0}; std::mutex totalMutex;
            for (const std::string &fname: files) {
                pool.enqueue([fname, &totalRows, &totalCols, &totalLines, &totalSize, &totalMutex]() {
                    std::size_t rows {0}, cols {0}, lineCounts {0};
                    std::ifstream ifs {fname};
                    std::string line, acc;
                    while (CSVUtil::safeGetline(ifs, line)) {
                        lineCounts++; acc += line;
                        std::size_t currCols {CSVUtil::parseCSVLine(acc).size()};
                        if (!currCols) acc += "\n";
                        else {
                            if (cols > 0 && cols != currCols) {
                                std::lock_guard lock(totalMutex);
                                std::cerr << fname << ": Column counts mismatch at line# " 
                                          << lineCounts << "; Expected " << cols << ", found " 
                                          << currCols << "\n";
                                return;
                            }

                            // Update for next iteration
                            cols = currCols; rows++; acc.clear();
                        }
                    }

                    // Display the stats 
                    double fSize {static_cast<double>(std::filesystem::file_size(fname)) / (1024 * 1024.)};
                    std::lock_guard lock(totalMutex);
                    totalRows += rows; totalCols += cols; 
                    totalLines += lineCounts; totalSize += fSize;
                    std::cout << lineCounts << '\t' << rows << '\t' << cols << '\t' << fSize << '\t' << fname << '\n';
                });
            }

            // Wait for all threads to join
            pool.wait();

            // If more than one file, print total stats
            if (files.size() > 1)
                std::cout << totalLines << '\t' << totalRows << '\t' << totalCols << '\t' << totalSize << "\ttotal\n";
        }
};

template<typename T>
void parseCLIArgument(const std::string &arg, T &placeholder) {
    std::from_chars_result parseResult {std::from_chars(arg.c_str(), arg.c_str() + arg.size(), placeholder)};
    if (parseResult.ec != std::errc() || parseResult.ptr != arg.c_str() + arg.size()) {
        std::cerr << "Error: Invalid value passed to argument: " << arg.c_str() << "\n";
        std::exit(1);
    }
}

// Validate and return a vector of files, validates the CSV header if provided else header validation is skipped
const std::vector<std::string> getFileList(int argc, char **argv, int start, const std::string &firstFileHeader = "") {
    std::vector<std::string> files;
    for (int i {start}; i < argc; i++) {
        std::string fname {argv[i]};
        if (!std::filesystem::is_regular_file(fname)) {
            std::cerr << "Error: File: " << argv[i] << " is not a valid file.\n";
            std::exit(1);
        } else if (!firstFileHeader.empty() && CSVUtil::extractHeader(fname) != firstFileHeader) {
            std::cerr << "Error: File: " << argv[i] << " header doesn't match with the first file.\n";
            std::exit(1);
        }
        files.push_back(argv[i]);
    }
    return files;
}

int main(int argc, char **argv) {
    constexpr const char* HELP_MESSAGE {
        "Usage: csvsplit <mode> <options> <file>\n"
        "\n"
        "Modes:\n"
        "  stat <file1> <file2> ...\n"
        "                           - Asynchronously gathers CSV file statistics.\n"
        "                           - Outputs: <lines> <rows> <columns> <file_size> <filename>\n\n"
        "  rows <count> <file>      - Split CSV into chunks of at most <count> records each.\n\n"
        "  size <size> <file>       - Split CSV into chunks of approximately <size> MB.\n\n"
        "  hash <colIdx> <buckets> <file>\n"
        "                           - Hash column <colIdx> and distribute into <buckets> files.\n\n"
        "  group <colIdx> <groupSize> <file>\n"
        "                           - Assign unique values of <colIdx> into groups of <groupSize>.\n"
        "                           - If <groupSize> is 1, creates one file per unique value.\n\n"
        "  revert <sync|async> <file1> <file2> <file3> ...\n"
        "                           - Merge multiple CSVs back into a single file.\n"
        "                           - 'sync' maintains order (file1 -> file2 -> file3 -> ...).\n"
        "                           - 'async' merges in parallel without order guarantees.\n"
        "\n"
        "Output Directory:\n"
        "  - The output directory can be set using the environment variable 'CSVOUT'.\n"
        "  - If 'CSVOUT' is not set, outputs are created in the directory where 'csvsplit' is run.\n"
        "\n"
        "Notes:\n"
        "  - Assumes CSVs have headers, which are preserved in splits and ignored when merging.\n"
        "  - 'rows' and 'size' split sequentially and are the most efficient.\n"
        "  - 'hash' is slightly less efficient but works well for large datasets.\n"
        "  - 'group' is the least efficient and not recommended for very large CSVs.\n"
    };

    try {

        // Get the output dir from env variable
        char * envVal {std::getenv("CSVOUT")};
        std::string outDir {envVal? std::string(envVal): std::filesystem::current_path().string()};

        // Create output path if doesn't exist
        if (std::filesystem::exists(outDir) && !std::filesystem::is_directory(outDir)) {
            std::cerr << "Output Directory in path is not valid.\n";
            return 1;
        } else if (!std::filesystem::exists(outDir)) {
            std::filesystem::create_directories(outDir);
        }

        if (argc >= 3 && std::strcmp(argv[1], "stat") == 0) {
            // Stat the input file(s)
            CSVStat::statFiles(getFileList(argc, argv, 2));

        }

        else if (argc >= 4 && std::strcmp(argv[1], "revert") == 0) {

            // Parse the list of files, ensure they exist & validate the headers match
            const std::string csvHeader {CSVUtil::extractHeader(argv[3])};
            std::vector<std::string> files {getFileList(argc, argv, 3, csvHeader)};
            CSVMerge merge{csvHeader, outDir};
            if (std::strcmp(argv[2], "sync") != 0 && std::strcmp(argv[2], "async") != 0) {
                std::cerr << "Error: Revert must be provided with either sync or async, "
                          << argv[2] << " was provided.\n";
                return 1;
            } 

            merge.mergeFiles(files, std::strcmp(argv[2], "sync") == 0);

        } else {

            // Define the strategy based on the CLI inputs
            std::unique_ptr<SplitStrategy> strategy;
            std::vector<std::size_t> pruneBuckets;
            std::string ifile {argv[argc - 1]};

            if (argc == 4 && std::strcmp(argv[1], "rows") == 0) {
                std::size_t counts;
                parseCLIArgument(argv[2], counts);
                strategy = std::make_unique<RecordCountStrategy>(counts, pruneBuckets);
            }

            else if (argc == 4 && std::strcmp(argv[1], "size") == 0) {
                std::size_t size;
                parseCLIArgument(argv[2], size);
                strategy = std::make_unique<SplitSizeStrategy>(size, pruneBuckets);
            }

            else if (argc == 5 && std::strcmp(argv[1], "hash") == 0) {
                std::size_t colIdx, bucketSize;
                parseCLIArgument(argv[2], colIdx);
                parseCLIArgument(argv[3], bucketSize);
                const std::size_t colCounts {CSVUtil::parseCSVLine(CSVUtil::extractHeader(ifile)).size()};
                if (colIdx >= colCounts) { std::cerr << "Error: Requested Col#: " << colIdx << " out of bounds. Actual column count: " << colCounts << "\n"; std::exit(1); }
                if (bucketSize == 0) { std::cerr << "Error: Bucket Size must be greater than 0.\n"; std::exit(1); }
                strategy = std::make_unique<HashColumnStrategy>(colIdx, bucketSize);
            }

            else if (argc == 5 && std::strcmp(argv[1], "group") == 0) {
                std::size_t colIdx, groupSize;
                parseCLIArgument(argv[2], colIdx);
                parseCLIArgument(argv[3], groupSize);
                const std::size_t colCounts {CSVUtil::parseCSVLine(CSVUtil::extractHeader(ifile)).size()};
                if (colIdx >= colCounts) { std::cerr << "Error: Requested Col#: " << colIdx << " out of bounds. Actual column count: " << colCounts << "\n"; std::exit(1); }
                if (groupSize == 0) { std::cerr << "Error: Group Size must be greater than 0.\n"; std::exit(1); }
                strategy = std::make_unique<GroupColumnStrategy>(colIdx, groupSize);
            }

            else {
                std::cout << HELP_MESSAGE;
                return 0;
            }

            // Create spliter and split CSV
            CSVSplit split{ifile, outDir, std::move(strategy), pruneBuckets};
            split.splitFile();
        }

        return 0;
    } 

    catch (std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
