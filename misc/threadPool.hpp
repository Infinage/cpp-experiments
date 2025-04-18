#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <variant>
#include <vector>

template <typename F, typename State = std::monostate>
class ThreadPool {
    private:
        const std::size_t N_WORKERS;
        std::vector<std::thread> workers;
        std::vector<State> states;
        std::queue<F> tasks;
        std::mutex taskMutex;
        std::condition_variable tasksCV, completedCV;
        int activeTasks {0};
        std::atomic<bool> exitCondition {false};

    public:
        ~ThreadPool() {
            exitCondition = true;
            tasksCV.notify_all();
            for (std::thread &worker: workers)
                if (worker.joinable()) 
                    worker.join();
        }

        ThreadPool(const std::size_t N_WORKERS, const std::function<State()> &initState = {}): 
            N_WORKERS(N_WORKERS)
        {
            for (std::size_t i {0}; i < this->N_WORKERS; i++) {
                if (initState) states.emplace_back(initState());
                workers.emplace_back(std::thread([this, i]{
                    for (;;) {
                        std::unique_lock<std::mutex> lock(taskMutex); 
                        tasksCV.wait(lock, [this]{ return exitCondition || !tasks.empty(); });
                        if (tasks.empty() && exitCondition) return;
                        else {
                            // Pick and execute one task
                            F task{std::move(tasks.front())}; 
                            tasks.pop();
                            activeTasks++;
                            lock.unlock();

                            // Execute task based on template
                            if constexpr(std::is_same_v<State, std::monostate>)
                                task();
                            else
                                task(states[i]);

                            // We have finished one task, notify all waiters
                            {
                                std::lock_guard lock(taskMutex);
                                activeTasks--;
                            }
                            completedCV.notify_all();
                        }
                    }
                }));
            }
        }

        // Wait for completion without destroying thread pool
        void wait() {
            std::unique_lock lock(taskMutex);
            completedCV.wait(lock, [this]{ return tasks.empty() && activeTasks == 0; });
        }

        // Enqueue a single task into the queue
        void enqueue(F &&task) {
            {
                std::lock_guard lock(taskMutex);
                tasks.push(std::move(task));
            }
            tasksCV.notify_one();
        }
};
