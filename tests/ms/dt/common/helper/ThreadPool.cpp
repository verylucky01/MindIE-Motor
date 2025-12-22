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

#include <sstream>
#include "ThreadPool.h"

bool ThreadPool::Init(int numberThread, const std::vector<uint32_t> &cores)
{
    mNumberThread = numberThread;
    mActiveCount = 0;
    for (int i = 0; i < mNumberThread; ++i) {
        auto temp = std::unique_ptr<std::atomic<bool>>(new (std::nothrow) std::atomic<bool> { false });
        if (temp == nullptr) {
            return false;
        }
        mWorks.second.emplace_back(std::move(temp));
    }
    std::stringstream ss;
    ss << std::this_thread::get_id();
    std::string threadPoolName = std::string("DOPAI_ThreadPool_") + ss.str();
    for (int i = 1; i < mNumberThread; ++i) {
        threadVec.emplace_back([this, i, threadPoolName]() {
            std::string threadName = threadPoolName + std::string("_") + std::to_string(i);
            pthread_setname_np(pthread_self(), threadName.c_str());
            while (!mStop) {
                Start(i);
            }
        });
    }
    if (cores.empty()) {
        return true;
    }

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (auto coreNum : cores) {
        CPU_SET(coreNum, &cpuset);
    }
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    for (int i = 1; i < mNumberThread; i++) {
        pthread_setaffinity_np(threadVec[i - 1].native_handle(), sizeof(cpu_set_t), &cpuset);
    }

    return true;
}

ThreadPool::~ThreadPool()
{
    {
        std::lock_guard<std::mutex> lock(mWorkMutex);
        mStop = true;
    }
    // waiting for all threads are activated
    bool allActive = false;
    while (!allActive) {
        allActive = true;
        for (auto &th : threadVec) {
            allActive &= th.joinable();
        }
    }
    mCondition.notify_all();
    for (auto &th : threadVec) {
        th.join();
    }
}

void ThreadPool::Active()
{
    if (mNumberThread <= 1) {
        return;
    }
    mActiveCount++;
    std::unique_lock<std::mutex> lock(mWorkMutex);
    mCondition.notify_all();
}

void ThreadPool::Deactive()
{
    if (mNumberThread > 1 && mActiveCount > 0) {
        mActiveCount--;
    }
}

void ThreadPool::Enqueue(TASK &&task)
{
    if (task.second <= 1 || mNumberThread <= 1) { // 不需要线程池，本线程直接完成
        for (int i = 0; i < task.second; ++i) {
            task.first(i);
        }
        return;
    }
    Complete(std::move(task));
}

void ThreadPool::Start(int threadIndex)
{
    while (mActiveCount > 0) {
        if (!*mWorks.second[threadIndex]) {
            continue;
        }
        mWorks.first.first(threadIndex);
        *mWorks.second[threadIndex] = false;
        std::this_thread::yield();
    }
    std::unique_lock<std::mutex> lock(mWorkMutex);
    mCondition.wait(lock, [this] { return mStop || mActiveCount > 0; });
}

void ThreadPool::Complete(TASK &&task)
{
    int taskNum = task.second;
    if (taskNum > mNumberThread) {
        mWorks.first = std::make_pair(
            [taskNum, &task, this](int tId) {
                for (int t = tId; t < taskNum; t += mNumberThread) {
                    task.first(t);
                }
            },
            mNumberThread);
        taskNum = mNumberThread;
    } else {
        mWorks.first = std::move(task);
    }

    for (int i = 1; i < taskNum; ++i) {
        *mWorks.second[i] = true;
    }

    mWorks.first.first(0);
    bool isFinished = true;
    do {
        std::this_thread::yield();
        isFinished = true;
        for (int i = 1; i < taskNum; ++i) {
            if (*mWorks.second[i]) {
                isFinished = false;
                break;
            }
        }
    } while (!isFinished);
}

