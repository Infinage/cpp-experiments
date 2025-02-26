#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <variant>
#include <vector>

template <class F, class State = std::monostate>
class ThreadPool {
    private:
        const std::size_t N_WORKERS;
        std::vector<std::thread> workers;
        std::vector<State> states;
        std::queue<F> tasks;
        std::mutex taskMutex;
        std::condition_variable cv;
        std::atomic<bool> exitCondition {false};

    public:
        ~ThreadPool() { join(); }
        ThreadPool(const std::size_t N_WORKERS, const std::function<State()> &initState = {}): 
            N_WORKERS(N_WORKERS)
        {
            for (std::size_t i {0}; i < this->N_WORKERS; i++) {
                if (initState) states.emplace_back(initState());
                workers.emplace_back(std::thread([this, i]{
                    for (;;) {
                        std::unique_lock<std::mutex> lock(taskMutex); 
                        cv.wait(lock, [this]{ return exitCondition || !tasks.empty(); });
                        if (tasks.empty() && exitCondition) return;
                        else {
                            F task{std::move(tasks.front())}; 
                            tasks.pop();
                            lock.unlock();
                            if constexpr(std::is_same_v<State, std::monostate>)
                                task();
                            else
                                task(states[i]);
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
                    if (worker.joinable()) 
                        worker.join();
            }
        }

        void enqueue(F &&task) {
            {
                std::lock_guard lock(taskMutex);
                tasks.push(std::move(task));
            }
            cv.notify_one();
        }
};
