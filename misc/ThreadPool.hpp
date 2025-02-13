#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

template <class F>
class ThreadPool {
    private:
        const std::size_t N_WORKERS;
        std::vector<std::thread> workers;
        std::queue<F> tasks;
        std::mutex taskMutex;
        std::condition_variable cv;
        std::atomic<bool> exitCondition = false;

    public:
        ~ThreadPool() { join(); }
        ThreadPool(const std::size_t N_WORKERS): N_WORKERS(N_WORKERS) {
            for (std::size_t i {0}; i < this->N_WORKERS; i++) {
                workers.push_back(std::thread([this]{
                    for (;;) {
                        std::unique_lock<std::mutex> lock(taskMutex); 
                        cv.wait(lock, [this]{ return exitCondition || !tasks.empty(); });
                        if (tasks.empty() && exitCondition) return;
                        else {
                            F task{tasks.front()}; 
                            tasks.pop();
                            lock.unlock();
                            task();
                        }
                    }
                }));
            }
        }

        // If not already joined
        void join() {
            if (!exitCondition) {
                exitCondition = true;
                cv.notify_all();
                for (std::thread &worker: workers)
                    worker.join();
            }
        }

        void enqueue(F &&task) {
            taskMutex.lock();
            tasks.push(task);
            taskMutex.unlock();
            cv.notify_one();
        }
};
