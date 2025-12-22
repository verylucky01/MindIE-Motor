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
#ifndef THREADSAFEVECTOR_H
#define THREADSAFEVECTOR_H

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <stdexcept>

template <typename T>
class ThreadSafeVector {
public:
    // push_back添加元素到vector中
    void push_back(const T& element)
    {
        std::lock_guard<std::mutex> lock(mtx);
        data.push_back(element);
    }

    // emplace_back添加元素到vector中
    void emplace_back(const T& element)
    {
        std::lock_guard<std::mutex> lock(mtx);
        data.emplace_back(element);
    }

    // 获取vector中的元素
    T &operator[](size_t index)
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (index < data.size()) {
            return data[index];
        } else {
            throw std::out_of_range("Index out of range");
        }
    }
    const T &operator[](size_t index) const
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (index < data.size()) {
            return data[index];
        } else {
            throw std::out_of_range("Index out of range");
        }
    }

    // 获取vector的大小
    size_t size() const
    {
        std::lock_guard<std::mutex> lock(mtx);
        return data.size();
    }

    // 删除指定索引的元素
    void removeElement(size_t index)
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (index < data.size()) {
            data.erase(data.begin() + index);
        } else {
            throw std::out_of_range("Index out of range");
        }
    }

private:
    std::vector<T> data;
    mutable std::mutex mtx; // 使用mutable允许const成员函数中锁定互斥锁
};

#endif