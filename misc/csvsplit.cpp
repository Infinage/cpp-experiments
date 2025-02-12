#include "CSVUtil.hpp"

#include <charconv>
#include <cstring>
#include <fstream>
#include <functional>
#include <memory>
#include <numeric>
#include <sstream>
#include <unordered_map>

class SplitStrategy {
    public:
        virtual std::size_t getBucket(const std::vector<std::string>&) = 0;
        virtual ~SplitStrategy() = default;
};

class RecordCountStrategy: public SplitStrategy {
    const std::size_t maxCounts;
    std::size_t currBucket {0}, currBucketCount {0};

    public:
        RecordCountStrategy(const std::size_t counts): maxCounts(counts) {}
        std::size_t getBucket(const std::vector<std::string>&) override {
            if (currBucketCount >= maxCounts) {
                currBucketCount = 0; 
                currBucket++; 
            } 

            currBucketCount++;
            return currBucket;
        }
};

class SplitSizeStrategy: public SplitStrategy {
    const std::size_t maxSize;
    std::size_t currBucket {0}, currBucketSize {0};

    public:
        SplitSizeStrategy(const std::size_t size): maxSize(size) {}
        std::size_t getBucket(const std::vector<std::string> &row) override {
            if (currBucketSize >= maxSize * 1024 * 1024) {
                currBucketSize = 0; 
                currBucket++; 
            }

            std::size_t currSize {std::accumulate(row.cbegin(), row.cend(), 0UL, 
                [](std::size_t acc, const std::string &field) { 
                    return field.size() + acc; 
                }
            )};
            currBucketSize += currSize;
            return currBucket;
        }
};

class HashColumnStrategy: public SplitStrategy {
    const std::size_t colIdx, bucketSize;
    const std::hash<std::string> hasher{};

    public:
        HashColumnStrategy(const std::size_t colIdx, std::size_t bucketSize): 
            colIdx(colIdx), bucketSize(bucketSize) {}

        std::size_t getBucket(const std::vector<std::string> &row) override {
            return hasher(row[colIdx]) % bucketSize; 
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

        std::size_t getBucket(const std::vector<std::string> &row) override {
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

        // Variables defined at constructor
        std::unique_ptr<SplitStrategy> strategy;
        const std::string ifname;
        const CSVUtil::CSVReader readHandle;
        const std::size_t N_COLS;
        const std::string CSVHeader;

    private:
        // Write & flush
        void flushBuffer(std::size_t bucket, std::unordered_map<std::size_t, std::ostringstream> &buffers) {
            outputHandles[bucket] << buffers[bucket].str();
            outputHandles[bucket].flush();
            buffers[bucket].str("");
            buffers[bucket].clear();
        }

    public:
        CSVSplit (const std::string &ifname, std::unique_ptr<SplitStrategy> strategy): 
            strategy(std::move(strategy)), ifname(ifname),
            readHandle(CSVUtil::CSVReader{ifname}), 
            N_COLS((*readHandle.begin()).size()), 
            CSVHeader({CSVUtil::writeCSVLine(*readHandle.begin()) + "\n"})
        {}

        // Splits a CSV file based on provided column & hash function into N buckets
        void splitFile() {
            std::size_t counts {0};
            std::unordered_map<std::size_t, std::ostringstream> buffers;
            for (const std::vector<std::string> &row: readHandle) {
                if (counts++) {
                    std::size_t bucket {strategy->getBucket(row)};
                    if (outputHandles.find(bucket) == outputHandles.end()) {
                        outputHandles[bucket] = std::ofstream{std::to_string(bucket) + ".csv"};
                        outputHandles[bucket] << CSVHeader;
                    }

                    buffers[bucket] << CSVUtil::writeCSVLine(row) + '\n';
                    if (buffers[bucket].tellp() >= static_cast<long>(1024 * 1024))
                        flushBuffer(bucket, buffers);
                }
            }

            // Flush any unwritten content
            for (std::pair<const std::size_t, std::ostringstream> &kv: buffers)
                flushBuffer(kv.first, buffers); 

            std::cout << "Read CSV records: " << counts << "\n"
                      << "Files created: " << outputHandles.size() << "\n";
        }
};

template<typename T>
void parseCLIArgument(const std::string &arg, T &placeholder) {
    std::from_chars_result parseResult {std::from_chars(arg.c_str(), arg.c_str() + arg.size(), placeholder)};
    if (parseResult.ec != std::errc() || parseResult.ptr != arg.c_str() + arg.size()) {
        std::cerr << "Invalid value passed to argument: " << arg.c_str() << "\n";
        std::exit(1);
    }
}

void assertColIdxWithinBounds(const std::string &fname, const std::size_t colIdx) {
    std::ifstream ifs {fname};
    std::string header;
    std::getline(ifs, header);
    std::size_t colCount {CSVUtil::parseCSVLine(header).size()};
    if (colIdx >= colCount) {
        std::cerr << "Requested Col#: " << colIdx << " out of bounds. Actual column count: " << colCount << "\n";
        std::exit(1);
    }
}

int main(int argc, char **argv) {

    // Define the strategy based on the CLI inputs
    std::unique_ptr<SplitStrategy> strategy;
    std::string ifile {argv[argc - 1]};

    if (argc == 4 && std::strcmp(argv[1], "rows") == 0) {
        std::size_t counts;
        parseCLIArgument(argv[2], counts);
        strategy = std::make_unique<RecordCountStrategy>(counts);
    }

    else if (argc == 4 && std::strcmp(argv[1], "size") == 0) {
        std::size_t size;
        parseCLIArgument(argv[2], size);
        strategy = std::make_unique<SplitSizeStrategy>(size);
    }

    else if (argc == 5 && std::strcmp(argv[1], "hash") == 0) {
        std::size_t colIdx, bucketSize;
        parseCLIArgument(argv[2], colIdx);
        parseCLIArgument(argv[3], bucketSize);
        assertColIdxWithinBounds(ifile, colIdx);
        if (bucketSize == 0) { std::cerr << "Bucket Size must be greater than 0.\n"; std::exit(1); }
        strategy = std::make_unique<HashColumnStrategy>(colIdx, bucketSize);
    }

    else if (argc == 5 && std::strcmp(argv[1], "group") == 0) {
        std::size_t colIdx, groupSize;
        parseCLIArgument(argv[2], colIdx);
        parseCLIArgument(argv[3], groupSize);
        assertColIdxWithinBounds(ifile, colIdx);
        if (groupSize == 0) { std::cerr << "Group Size must be greater than 0.\n"; std::exit(1); }
        strategy = std::make_unique<GroupColumnStrategy>(colIdx, groupSize);
    }

    else {
        std::cout <<
            "Usage: csvsplit <mode> [options] <file>\n"
            "\n"
            "Modes:\n"
            "  rows <count> <file>   - Split CSV into chunks of at most <count> records each.\n"
            "  size <size> <file>       - Split CSV into chunks of approximately <size> (Size in MB).\n"
            "  hash <colIdx> <buckets> <file>\n"
            "                           - Hash column <colIdx> and distribute into <buckets> files.\n"
            "  group <colIdx> <groupSize> <file>\n"
            "                           - Assign unique values of <colIdx> into groups of <groupSize>.\n"
            "                           - If <groupSize> is 1, creates one file per unique value.\n"
            "\n"
            "Performance Notes:\n"
            "  - 'rows' and 'size' split sequentially and are the most efficient.\n"
            "  - 'hash' is slightly less efficient but works well for large datasets.\n"
            "  - 'group' is the least efficient and not recommended for very large CSVs.\n\n";

        return 0;
    } 

    // Create spliter and split CSV
    CSVSplit split{ifile, std::move(strategy)};
    split.splitFile();
}
