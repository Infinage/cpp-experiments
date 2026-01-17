#pragma once

#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace async {
    class ThreadPool {
        public:
            enum class ExitPolicy {ALL_TASKS_COMPLETE, CURRENT_TASK_COMPLETE};

        private:
            ExitPolicy epolicy;
            std::vector<std::thread> workers;
            std::queue<std::packaged_task<void()>> tasks;
            std::mutex taskMutex;
            std::condition_variable tasksCV, completedCV;
            std::atomic<bool> exitCondition {false};
            int activeTasks {0};

        public:
            ~ThreadPool() {
                exitCondition = true;
                tasksCV.notify_all();
                for (std::thread &worker: workers)
                    if (worker.joinable())
                        worker.join();
            }

            ThreadPool(
                const std::size_t N_WORKERS = std::thread::hardware_concurrency(), 
                ExitPolicy policy = ExitPolicy::ALL_TASKS_COMPLETE): epolicy {policy} 
            {
                for (std::size_t i {}; i < N_WORKERS; ++i) {
                    workers.emplace_back(std::thread([this]{
                        for (;;) {
                            std::unique_lock lock(taskMutex); 
                            tasksCV.wait(lock, [this]{ return exitCondition || !tasks.empty(); });
                            if (exitCondition) {
                                if (epolicy == ExitPolicy::CURRENT_TASK_COMPLETE) return;
                                if (epolicy == ExitPolicy::ALL_TASKS_COMPLETE && tasks.empty()) return;
                            } else {
                                // Pick and execute one task
                                auto task = std::move(tasks.front());
                                tasks.pop(); activeTasks++; 
                                lock.unlock();
                                task();

                                // We have finished one task, notify all waiters
                                {
                                    std::lock_guard lock(taskMutex);
                                    --activeTasks;
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
            template<typename Fn>
            auto enqueue(Fn &&fn) -> std::future<std::invoke_result_t<Fn>> {
                using R = std::invoke_result_t<Fn>;
                std::packaged_task<R()> task{std::forward<Fn>(fn)};
                std::future<R> future = task.get_future();
                std::lock_guard lock(taskMutex);
                tasks.emplace(std::move(task));
                tasksCV.notify_one();
                return future;
            }
    };
}
