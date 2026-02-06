// Copyright Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/thread_pool.h"
#include <memory>

namespace Common {

    static std::unique_ptr<ThreadPool> g_thread_pool;
    static std::mutex g_pool_mutex;

    void InitializeThreadPool(size_t num_threads) {
        std::lock_guard<std::mutex> lock(g_pool_mutex);

        if (!g_thread_pool) {
            g_thread_pool = std::make_unique<ThreadPool>(num_threads);
            LOG_INFO(Common, "Global thread pool initialized with {} threads", num_threads);
        }
    }

    ThreadPool& GetThreadPool() {
        std::lock_guard<std::mutex> lock(g_pool_mutex);

        if (!g_thread_pool) {
            // Auto-initialize with default 3 threads if not explicitly initialized
            g_thread_pool = std::make_unique<ThreadPool>(3);
            LOG_WARNING(Common, "Thread pool auto-initialized (should call InitializeThreadPool explicitly)");
        }

        return *g_thread_pool;
    }

    void ShutdownThreadPool() {
        std::lock_guard<std::mutex> lock(g_pool_mutex);

        if (g_thread_pool) {
            g_thread_pool.reset();
            LOG_INFO(Common, "Global thread pool shutdown");
        }
    }

} // namespace Common