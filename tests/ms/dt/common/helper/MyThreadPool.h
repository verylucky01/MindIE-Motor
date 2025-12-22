/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 * MindIE is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *         http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef MINDIE_MS_MYTHREADPOOL_H
#define MINDIE_MS_MYTHREADPOOL_H

#include <pthread.h>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <stdexcept>

class MyThreadPool {
public:
    // Constructor starts the specified number of threads.
    explicit MyThreadPool(size_t threadCount) : running(false) {
        if (threadCount == 0) {
            throw std::invalid_argument("Number of threads must be greater than zero.");
        }
        running = true;
        for (size_t i = 0; i < threadCount; ++i) {
            threads.emplace_back([this] {
                for (;;) {
                    std::function<void()> job;

                    {
                        std::unique_lock<std::mutex> lock(queueLock);
                        workAvailable.wait(lock, [this]{ return !jobs.empty() || !running; });

                        if (!running && jobs.empty())
                            return; // Exit thread

                        job = std::move(jobs.front());
                        jobs.pop();
                    }

                    job();
                }
            });
        }
    }

    // Enqueues a task into the thread pool.
    template <typename Func, typename... Args>
    auto submit(Func&& func, Args&&... args)
        -> std::future<typename std::invoke_result<Func, Args...>::type> {
        using result_type = typename std::invoke_result<Func, Args...>::type;

        auto task = std::make_shared<std::packaged_task<result_type()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>(args)...));

        std::future<result_type> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lock(queueLock);
            if (!running)
                throw std::runtime_error("Cannot enqueue to stopped MyThreadPool.");
            jobs.emplace([task](){ (*task)(); });
        }
        workAvailable.notify_one();
        return fut;
    }

    // Destructor waits for all threads to finish their tasks.
    ~MyThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueLock);
            running = false;
        }
        workAvailable.notify_all(); // Notify all threads to stop.
        for (std::thread& th : threads)
            th.join(); // Wait for all threads to finish.
    }

    static MyThreadPool &GetInstance()
    {
        static MyThreadPool pools(64);
        return pools;
    }

private:
    std::vector<std::thread> threads; // Worker threads.
    std::queue<std::function<void()>> jobs; // Task queue.
    std::mutex queueLock; // Mutex for synchronizing access to the queue.
    std::condition_variable workAvailable; // Condition variable for notifying threads about available work.
    std::atomic<bool> running; // Atomic flag indicating whether the thread pool is running.
};

#endif // MINDIE_MS_MYTHREADPOOL_H