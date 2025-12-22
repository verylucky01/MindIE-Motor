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

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <memory>

class ThreadPool {
public:
    typedef std::pair<std::function<void(int)>, int> TASK;
    ThreadPool() = default;
    ~ThreadPool();
    bool Init(int number = 0, const std::vector<uint32_t> &cores = {});
    void Active();
    void Deactive();
    void Enqueue(TASK&& task);

private:
    void Start(int threadIndex);
    void Complete(TASK &&task);

    int mNumberThread = 0;
    std::mutex mWorkMutex;
    std::condition_variable mCondition;
    std::atomic<int> mActiveCount = {0};
    std::atomic<bool> mStop = {false};
    std::vector<std::thread> threadVec;
    std::pair<TASK, std::vector<std::unique_ptr<std::atomic<bool>>>> mWorks;
};

