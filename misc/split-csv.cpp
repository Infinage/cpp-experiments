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

// TODO: If buckets are not specified, creates one csv per value found.

class CSVSplit {
    private:
        struct FileHandle {
            std::unique_ptr<std::mutex> lock;
            std::ofstream stream;
            FileHandle(const std::string &fname): lock(std::make_unique<std::mutex>()), stream(fname) {
                if (!stream) {
                    std::cerr << "Unable to open file for writing: " << fname << "\n";
                    std::exit(1);
                }
            }
        };

        std::vector<std::thread> workers;
        std::condition_variable cv;
        std::mutex taskMutex;
        std::atomic_bool exitCondition {false};

        std::vector<FileHandle> outputHandles;
        std::queue<std::pair<std::size_t, std::string>> tasks;

        const std::string ifname;
        const std::size_t colNum;
        const std::size_t N_WORKERS; 
        const std::size_t N_BUCKETS;
        const std::size_t THRESHOLD;

    private:
        void flushBuffer(std::size_t bucket, std::unordered_map<std::size_t, std::ostringstream> &buffers) {
            std::lock_guard lock(*outputHandles[bucket].lock);
            outputHandles[bucket].stream << buffers[bucket].str();
            outputHandles[bucket].stream.flush();
            buffers[bucket].str("");
            buffers[bucket].clear();
        }

        void executeTask(std::size_t bucket, std::string &line, std::unordered_map<std::size_t, std::ostringstream> &buffers) {
            buffers[bucket] << line << "\n";
            if (buffers[bucket].tellp() >= static_cast<long>(THRESHOLD * 1024)) 
                flushBuffer(bucket, buffers);
        }

        void enqueue(const std::size_t bucket, const std::string &line) {
            std::unique_lock lock(taskMutex);
            tasks.push({bucket, line});
            lock.unlock();
            cv.notify_one();
        }

    public:
        CSVSplit (
            const std::string &ifname, const std::size_t colNum,
            const std::size_t n_workers = 10, 
            const std::size_t n_buckets = 4, 
            const std::size_t batch_size = 5000
        ): 
            ifname(ifname), 
            colNum(colNum),
            N_WORKERS(n_workers), 
            N_BUCKETS(n_buckets), 
            THRESHOLD(batch_size)
        {
            // Init the output handles
            for (std::size_t i {0}; i < N_BUCKETS; i++)
                outputHandles.emplace_back(std::to_string(i) + ".csv");

            // Spawn the threads
            for (std::size_t i {0}; i < N_WORKERS; i++) {
                workers.push_back(std::thread{[this]{
                    std::unordered_map<std::size_t, std::ostringstream> buffers;
                    for (;;) {
                        std::unique_lock lock(taskMutex);  
                        cv.wait(lock, [this]{ return exitCondition || !tasks.empty(); });
                        if (tasks.empty() && exitCondition) { 
                            for (std::size_t bufIdx {0}; bufIdx < N_BUCKETS; bufIdx++)
                                flushBuffer(bufIdx, buffers); 
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
        void splitFile() {
            std::hash<std::string> hasher;
            std::size_t counts {0};
            for (auto &row: CSVUtil::CSVReader{ifname}) {
                enqueue(hasher(row[colNum]) % N_BUCKETS, CSVUtil::writeCSVLine(row));
                counts++;
            }

            std::cout << "Read CSV records: " << counts << "\n";
        }

        ~CSVSplit() {
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
    if (argc != 4 && argc != 3) {
        std::cout << "Usage: split-csv <file> <colIdx> <buckets>\n"
                  << "If buckets are not specified, creates one csv per value found.\n";
    } 

    else {
        std::string ifile {argv[1]};
        std::size_t colIdx; 
        parseCLIArgument(argv[2], colIdx);
        std::size_t buckets{0};
        if (argc == 4) parseCLIArgument(argv[2], buckets);
        CSVSplit split{ifile, colIdx, buckets};
        split.splitFile();
    }
}
