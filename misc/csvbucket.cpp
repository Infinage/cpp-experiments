#include "CSVUtil.hpp"

#include <atomic>
#include <charconv>
#include <condition_variable>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <utility>

class CSVBucket {
    private:
        struct FileHandle {
            std::mutex lock;
            std::ofstream stream;
            FileHandle(const std::string &fname, const std::string &header): lock(std::mutex{}), stream(fname) {
                if (!stream) {
                    std::cerr << "Unable to open file for writing: " << fname << "\n";
                    std::exit(1);
                }

                // Add the header and flush if specified
                if (!header.empty()) { stream << header; stream.flush(); }
            }
        };

        // Pool specific members
        std::vector<std::thread> workers;
        std::condition_variable cv;
        std::mutex taskMutex;
        std::atomic_bool exitCondition {false};
        std::unordered_map<std::size_t, std::unique_ptr<FileHandle>> outputHandles;
        std::queue<std::pair<std::size_t, std::string>> tasks;

        // Quick string to bucket lookup (used only when N_BUCKETS is set to 0)
        std::unordered_map<std::string, std::size_t> bucketMap;
        std::size_t uniqueCounts {0};

        // Variables defined at constructor
        const std::string ifname;
        const std::size_t colNum;
        const std::size_t N_BUCKETS;
        const std::size_t N_WORKERS; 
        const std::size_t THRESHOLD;
        const CSVUtil::CSVReader readHandle;
        const std::string CSVHeader;

    private:
        // Acquite lock on the output handle, write & flush
        void flushBuffer(std::size_t bucket, std::unordered_map<std::size_t, std::ostringstream> &buffers) {
            std::lock_guard lock(outputHandles[bucket]->lock);
            outputHandles[bucket]->stream << buffers[bucket].str();
            outputHandles[bucket]->stream.flush();
            buffers[bucket].str("");
            buffers[bucket].clear();
        }

        // Fill the buffer, if buffer exceeds threshold then flush it
        void executeTask(std::size_t bucket, std::string &line, std::unordered_map<std::size_t, std::ostringstream> &buffers) {
            buffers[bucket] << line;
            if (buffers[bucket].tellp() >= static_cast<long>(THRESHOLD * 1024))
                flushBuffer(bucket, buffers);
        }

        // Enqueue a single task, create a new handle if doesn't exist
        void enqueue(std::queue<std::pair<std::size_t, std::string>> &localQueue) {
            // Create scoped lock
            { 
                std::scoped_lock lock(taskMutex); 
                while (!localQueue.empty()) {
                    tasks.emplace(localQueue.front()); 
                    localQueue.pop();
                }
            }
            cv.notify_all();
        }

        // Get bucket number given value
        std::size_t getBucket(const std::string &val) {
            if (N_BUCKETS) {
                return std::hash<std::string>{}(val) % N_BUCKETS;
            } else if (bucketMap.find(val) != bucketMap.end()) {
                return bucketMap[val];
            } else {
                return bucketMap[val] = uniqueCounts++; 
            }
        }

    public:
        CSVBucket (
            const std::string &ifname, 
            const std::size_t colNum,
            const std::size_t n_buckets,
            const std::size_t n_workers, 
            const std::size_t bufferThresholdKB
        ): 
            ifname(ifname), 
            colNum(colNum),
            N_BUCKETS(n_buckets), 
            N_WORKERS(n_workers), 
            THRESHOLD(bufferThresholdKB),
            readHandle(CSVUtil::CSVReader{ifname}),
            CSVHeader({CSVUtil::writeCSVLine(*readHandle.begin()) + "\n"})
        {
            // Spawn the threads
            for (std::size_t i {0}; i < N_WORKERS; i++) {
                workers.push_back(std::thread{[this]{
                    std::unordered_map<std::size_t, std::ostringstream> buffers;
                    for (;;) {
                        std::unique_lock lock(taskMutex);  
                        cv.wait(lock, [this]{ return exitCondition || !tasks.empty(); });
                        if (tasks.empty() && exitCondition) { 
                            for (std::pair<const std::size_t, std::ostringstream> &kv: buffers)
                                flushBuffer(kv.first, buffers); 
                            return; 
                        } else {
                            auto [bucket, line] {tasks.front()};
                            tasks.pop();
                            lock.unlock();
                            executeTask(bucket, line, buffers);
                        }
                    }
                }});
            }
        }

        // Splits a CSV file based on provided column & hash function into N buckets
        // Note: This function runs synchronously pushing tasks to the pool
        void bucketFile() {
            std::size_t counts {0}, batchEnqueueCount {1000};
            std::queue<std::pair<std::size_t, std::string>> localQueue;
            for (auto &row: readHandle) {
                if (colNum >= row.size()) { std::cerr << "Column no. out of bounds.\n"; std::exit(1); }
                if (counts++) {
                    std::size_t bucket {getBucket(row[colNum])};
                    if (outputHandles.find(bucket) == outputHandles.end())
                        outputHandles[bucket] = std::make_unique<FileHandle>(std::to_string(bucket) + ".csv", CSVHeader);
                    localQueue.emplace(bucket, CSVUtil::writeCSVLine(row) + '\n');
                    if (counts % batchEnqueueCount == 0) enqueue(localQueue);
                }
            }

            // Enqueue any pending tasks
            enqueue(localQueue);

            std::cout << "Read CSV records: " << counts << "\n"
                      << "Files created: " << (N_BUCKETS? N_BUCKETS: uniqueCounts) << "\n";
        }

        ~CSVBucket() {
            exitCondition = true;
            cv.notify_all();
            for (std::thread &worker: workers)
                worker.join();
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

int main(int argc, char **argv) {
    if (argc < 3 || argc > 6) {
        std::cout << "Splits a CSV file into smaller CSV files in the current directory.\n"
                  << "Usage: csvbucket <file> <colIdx> [buckets] [workers] [thresholdKB]\n"
                  << "  <file>       : Path to input CSV file\n"
                  << "  <colIdx>     : Column index (0-based) used for splitting\n"
                  << "  [buckets]    : Number of output files (default: 0)\n"
                  << "                 - 0 (default): Creates one file per unique value in the column\n"
                  << "                 - N (>0): Buckets values into N output files\n"
                  << "  [workers]    : Number of worker threads (default: 8)\n"
                  << "  [thresholdKB]: Buffer flush threshold in KB (default: 512)\n";
    } 

    else {
        // Parse the CLI args
        std::string ifile {argv[1]};
        std::size_t colIdx, buckets{0}, workers {8}, threshold {512}; 
        parseCLIArgument(argv[2], colIdx);
        if (argc >= 4) parseCLIArgument(argv[3], buckets);
        if (argc >= 5) parseCLIArgument(argv[4], workers);
        if (argc == 6) parseCLIArgument(argv[5], threshold);

        // Create spliter and split CSV
        CSVBucket split{ifile, colIdx, buckets, workers, threshold};
        split.bucketFile();
    }
}
