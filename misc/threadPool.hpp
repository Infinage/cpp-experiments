#pragma once

#include <concepts>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Helper struct to capture the function return type - with state
template <typename F, typename State>
struct FuncReturnType { using type = std::invoke_result_t<F, State>; };

// Helper struct to capture the function return type - without state
template <typename F>
struct FuncReturnType<F, std::monostate> { using type = std::invoke_result_t<F>; };

// Optional support for a pool of states
template <typename F, typename State = std::monostate>
class ThreadPool {
    private:
        using FUNC_RTYPE = typename FuncReturnType<F, State>::type;
        const std::size_t N_WORKERS;
        std::vector<std::thread> workers;
        std::vector<State> states;
        std::queue<std::pair<F, std::promise<FUNC_RTYPE>>> tasks;
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
            for (std::size_t i {0}; i < N_WORKERS; ++i) {
                if (initState) states.emplace_back(initState());
                workers.emplace_back(std::thread([this, i]{
                    for (;;) {
                        std::unique_lock lock(taskMutex); 
                        tasksCV.wait(lock, [this]{ return exitCondition || !tasks.empty(); });
                        if (tasks.empty() && exitCondition) return;
                        else {
                            // Pick and execute one task
                            auto taskPair {std::move(tasks.front())};
                            tasks.pop(); activeTasks++; lock.unlock();
                            F task {std::move(taskPair.first)};
                            std::promise<FUNC_RTYPE> promise {std::move(taskPair.second)};

                            // Execute task based on template
                            try {
                                if constexpr(std::is_same_v<State, std::monostate>) {
                                    if constexpr(std::is_same_v<FUNC_RTYPE, void>) {
                                        task(); promise.set_value();
                                    } else {
                                        promise.set_value(task());
                                    }
                                } else {
                                    if constexpr(std::is_same_v<FUNC_RTYPE, void>) {
                                        task(states[i]); promise.set_value();
                                    } else {
                                        promise.set_value(task(states[i]));
                                    }
                                }
                            } catch(...) {
                                promise.set_exception(std::current_exception());
                            }

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
        std::future<FUNC_RTYPE> enqueue(F &&task) {
            std::promise<FUNC_RTYPE> promise;
            std::future<FUNC_RTYPE> future {promise.get_future()};
            std::lock_guard lock(taskMutex);
            tasks.emplace(std::move(task), std::move(promise));
            tasksCV.notify_one();
            return future;
        }

        // Enqueue multiple tasks into the queue (burst)
        template<std::ranges::common_range T> requires std::convertible_to<std::iter_value_t<T>, F>
        std::vector<std::future<FUNC_RTYPE>> enqueueAll(T &&tasks) {
            std::vector<std::future<FUNC_RTYPE>> futures;
            std::lock_guard lock(taskMutex);
            for (auto &&task: tasks) {
                std::promise<FUNC_RTYPE> promise;
                std::future<FUNC_RTYPE> future {promise.get_future()};
                this->tasks.emplace(std::move(task), std::move(promise));
                futures.emplace_back(std::move(future));
            }
            tasksCV.notify_all();
            return futures;
        }
};
