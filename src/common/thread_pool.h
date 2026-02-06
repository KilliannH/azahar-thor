// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "common/cpu_affinity.h"
#include "common/logging/log.h"

namespace Common {

/**
 * Thread pool optimized for Snapdragon 8 Gen 2
 * Threads are pinned to big cores and reused for performance
 */
    class ThreadPool {
    public:
        explicit ThreadPool(size_t num_threads = 3) : stop(false) {
            LOG_INFO(Common, "Creating thread pool with {} workers", num_threads);

            for (size_t i = 0; i < num_threads; ++i) {
                workers.emplace_back([this, i] {
#ifdef __ANDROID__
                    SetBigCoreAffinity();
                LOG_DEBUG(Common, "Thread pool worker {} pinned to big cores", i);
#endif

                    while (true) {
                        std::function<void()> task;

                        {
                            std::unique_lock<std::mutex> lock(queue_mutex);
                            condition.wait(lock, [this] {
                                return stop || !tasks.empty();
                            });

                            if (stop && tasks.empty()) {
                                return;
                            }

                            task = std::move(tasks.front());
                            tasks.pop();
                        }

                        task();
                    }
                });
            }
        }

        template<class F, class... Args>
        auto Enqueue(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {

            using return_type = typename std::invoke_result<F, Args...>::type;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
            );

            std::future<return_type> res = task->get_future();

            {
                std::unique_lock<std::mutex> lock(queue_mutex);

                if (stop) {
                    throw std::runtime_error("Enqueue on stopped ThreadPool");
                }

                tasks.emplace([task]() { (*task)(); });
            }

            condition.notify_one();
            return res;
        }

        void WaitForTasks() {
            while (true) {
                std::unique_lock<std::mutex> lock(queue_mutex);
                if (tasks.empty()) {
                    break;
                }
                lock.unlock();
                std::this_thread::yield();
            }
        }

        size_t GetThreadCount() const {
            return workers.size();
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex);
                stop = true;
            }

            condition.notify_all();

            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }

            LOG_INFO(Common, "Thread pool destroyed");
        }

    private:
        std::vector<std::thread> workers;
        std::queue<std::function<void()>> tasks;
        std::mutex queue_mutex;
        std::condition_variable condition;
        std::atomic<bool> stop;
    };

/**
 * Initialize the global thread pool
 * Call this once at application startup
 */
    void InitializeThreadPool(size_t num_threads = 3);

/**
 * Get the global thread pool instance
 * Automatically initializes if not already done
 */
    ThreadPool& GetThreadPool();

/**
 * Shutdown the global thread pool
 * Call this at application shutdown
 */
    void ShutdownThreadPool();

} // namespace Common