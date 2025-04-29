// $ time ./ma-backtester $(echo data/*.csv | sed 's/ /,/g')
#include "../misc/CSVUtil.hpp"
#include "../misc/threadPool.hpp"
#include "../cli/argparse.hpp"
#include <cmath>

class MovingAverage {
    private:
        const std::size_t window;
        std::queue<double> queue;
        double total {0};

    public:
        MovingAverage(const std::size_t window): window(window) {}
        inline std::size_t size() const { return queue.size(); }
        inline std::size_t windowSize() const { return window; }
        inline bool ready() const { return queue.size() == window; }
        inline double get() const { return total / static_cast<double>(queue.size()); }
        inline void update(double val) {
            queue.push(val);  
            total += val;
            if (queue.size() > window) {
                total -= std::move(queue.front());
                queue.pop();
            }
        }
};

inline short getTradeSignal(double delta) {
    return delta < 0? -1: delta > 0? 1: 0;
}

[[nodiscard]] 
double trade(
    const std::string &fileName, 
    double corpus = 100'000, 
    const std::size_t shortWin = 15, 
    const std::size_t longWin = 50) 
{

    // Read the CSV file
    CSVUtil::CSVReader reader {fileName}; 
    MovingAverage shortMA {shortWin}, longMA {longWin};
    double amtAvailable {corpus}, close {0}, stocks {0}; 
    std::size_t idx {0}; short prev {0};
    for (CSVUtil::CSVRecord &rec: reader) {
        // Skip header
        if (idx++ == 0) continue;

        // Skip rows without any values
        if (rec[4].empty()) continue;

        rec[4] >> close;
        shortMA.update(close); longMA.update(close);
        if (prev != 0) {
            short curr {getTradeSignal(shortMA.get() - longMA.get())};
            if (prev == 1 && curr == -1 && stocks > 0) {
                // Bearish Cross - Panic Sell - assuming we can sell all own
                amtAvailable += static_cast<double>(stocks) * close;
                stocks = 0;
            } else if (prev == -1 && curr == 1) {
                // Golden Cross - Greedy Buy - assuming we can buy all we can
                double canBuy {std::floor(amtAvailable / close)};
                stocks += canBuy;
                amtAvailable -= canBuy * close;
            }
        } 

        if (longMA.ready()) {
            prev = getTradeSignal(shortMA.get() - longMA.get());
        }
    }

    // Print final worth
    return (amtAvailable + (stocks * close)) - corpus;
}

int main(int argc, char **argv) {
    try {
        // Configure the CLI args
        argparse::ArgumentParser program {"ma-backtester"};
        program.description("A simple moving average crossover backtester");
        program.addArgument("corpus", argparse::NAMED)
           .defaultValue(100'000.).alias("c")
           .help("Starting corpus for the trading simulations");
        program.addArgument("short-window", argparse::NAMED).defaultValue(15)
           .alias("s").help("Short window size for the backtester");
        program.addArgument("long-window", argparse::NAMED).defaultValue(50)
           .alias("l").help("Long window size for the backtester");
        program.addArgument("output", argparse::NAMED).defaultValue("output.csv")
           .alias("o").help("Output path to write the log");
        program.addArgument("files").scan<std::vector<std::string>>().required()
            .help("List of CSV files to backtest against.");
        program.parseArgs(argc, argv);

        // Read the CLI args as parameters
        const double corpus {program.get<double>("corpus")};
        const std::vector<std::string> files {program.get<std::vector<std::string>>("files")};
        const std::string ofile {program.get("output")};
        const std::size_t shortWin {static_cast<std::size_t>(program.get<int>("short-window"))}, 
                          longWin {static_cast<std::size_t>(program.get<int>("long-window"))};

        // Define the pool of threads
        ThreadPool<std::function<std::pair<std::string, double>(void)>> pool {
            std::thread::hardware_concurrency()};

        // Add the tasks to our pool of threads
        std::vector<std::future<std::pair<std::string, double>>> futures;
        for (const std::string &file: files) {
            futures.emplace_back(
                pool.enqueue([file, corpus, shortWin, longWin]{
                    return std::make_pair(file, trade(file, corpus, shortWin, longWin));
                })
            );
        }

        // Process the results
        std::ofstream ofs {ofile};
        ofs << "File,Profit,Profit%\n";
        for (std::future<std::pair<std::string, double>> &future: futures) {
            std::string file; double profit;
            std::tie(file, profit) = future.get();
            double profitPercentage = (profit / corpus) * 100.0;
            ofs << CSVUtil::writeCSVLine({file, std::to_string(profit), std::to_string(profitPercentage)}) << '\n';
        }
    
        return 0;
    }

    catch (std::exception &ex) {
        std::cerr << "Fatal " << ex.what() << '\n';
        return 1;
    }
}
