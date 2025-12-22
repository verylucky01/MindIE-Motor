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
#include <thread>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "HeartbeatProducer.h"
#include "SharedMemoryUtils.h"

namespace MINDIE {
namespace MS {
    class SMUHeartbeatProducerTestAccessor : public HeartbeatProducer {
    public:
        SMUHeartbeatProducerTestAccessor(
            std::chrono::milliseconds interval,
            const std::string& shmName,
            const std::string& semName,
            size_t bufferSize)
            : HeartbeatProducer(interval, shmName, semName, bufferSize) {}

        std::string CreateHeartbeatMessagePublic(uint64_t sequenceNumber, long long timestamp)
        {
            return CreateHeartbeatMessage(sequenceNumber, timestamp);
        }
    };
}
}

using namespace MINDIE::MS;
using namespace ::testing;

class HeartbeatProducerTest : public ::testing::Test {
protected:
    std::string testShmName = "test_shm_hb_ut";
    std::string testSemName = "test_sem_hb_ut";
    size_t testBufferSize = 1024;
    std::chrono::milliseconds testInterval = std::chrono::milliseconds(10);

    void SetUp() override
    {
        SharedMemoryUtils::ClearResources(testShmName, testSemName);
    }

    void TearDown() override
    {
        SharedMemoryUtils::ClearResources(testShmName, testSemName);
    }
};

// --- Test Cases ---

/**
 * 测试描述: 构造函数正确初始化
 * 测试步骤:
 * 1. 使用有效的参数构造HeartbeatProducer对象。
 * 预期结果:
 * 1. 构造函数不抛出任何异常。
 */
TEST_F(HeartbeatProducerTest, ConstructorInitializesCorrectly)
{
    ASSERT_NO_THROW({
        HeartbeatProducer producer(testInterval, testShmName, testSemName, testBufferSize);
    });
}

/**
 * 测试描述: CreateHeartbeatMessage函数正确格式化心跳消息
 * 测试步骤:
 * 1. 使用不同的序列号和时间戳调用CreateHeartbeatMessagePublic函数。
 * 预期结果:
 * 1. 返回的JSON字符串与预期格式匹配。
 */
TEST_F(HeartbeatProducerTest, CreateHeartbeatMessageFormatsCorrectly)
{
    SMUHeartbeatProducerTestAccessor producerAccessor(testInterval, testShmName, testSemName, testBufferSize);
    
    uint64_t seq = 123;
    long long ts = 456789;
    std::string expectedJson = "{\"seq\":123,\"timestamp\":456789}";
    
    EXPECT_EQ(producerAccessor.CreateHeartbeatMessagePublic(seq, ts), expectedJson);

    seq = 0;
    ts = 0;
    expectedJson = "{\"seq\":0,\"timestamp\":0}";
    EXPECT_EQ(producerAccessor.CreateHeartbeatMessagePublic(seq, ts), expectedJson);

    seq = 999999999999999999ULL;
    ts = 1678886400000LL;
    expectedJson = "{\"seq\":999999999999999999,\"timestamp\":1678886400000}";
    EXPECT_EQ(producerAccessor.CreateHeartbeatMessagePublic(seq, ts), expectedJson);
}

/**
 * 测试描述: Start和Stop功能正常
 * 测试步骤:
 * 1. 启动心跳生产者。
 * 2. 等待一段时间。
 * 3. 停止心跳生产者。
 * 4. 再次停止心跳生产者。
 * 5. 再次启动心跳生产者。
 * 6. 等待一段时间。
 * 7. 再次停止心跳生产者。
 * 预期结果:
 * 1. Start和Stop操作不抛出异常，且心跳线程能够正常启动和停止。
 */
TEST_F(HeartbeatProducerTest, StartAndStopFunctionality)
{
    HeartbeatProducer producer(testInterval, testShmName, testSemName, testBufferSize);

    producer.Start();
    std::this_thread::sleep_for(testInterval * 2); // 等待2倍于testInterval后再停止心跳
    
    producer.Stop();

    producer.Stop();

    producer.Start();
    std::this_thread::sleep_for(testInterval * 2); // 等待2倍于testInterval后再停止心跳
    producer.Stop();
}

/**
 * 测试描述: 多次调用Start函数
 * 测试步骤:
 * 1. 连续多次调用Start函数。
 * 2. 等待一段时间。
 * 3. 停止心跳生产者。
 * 预期结果:
 * 1. 多次调用Start函数不会导致异常或错误。
 */
TEST_F(HeartbeatProducerTest, CallingStartMultipleTimes)
{
    HeartbeatProducer producer(testInterval, testShmName, testSemName, testBufferSize);

    producer.Start();
    producer.Start();
    std::this_thread::sleep_for(testInterval * 2); // 等待2倍于testInterval后再停止心跳
    producer.Stop();
}

/**
 * 测试描述: 多次调用Stop函数
 * 测试步骤:
 * 1. 启动心跳生产者。
 * 2. 等待一段时间。
 * 3. 连续多次调用Stop函数。
 * 预期结果:
 * 1. 多次调用Stop函数不会导致异常或错误。
 */
TEST_F(HeartbeatProducerTest, CallingStopMultipleTimes)
{
    HeartbeatProducer producer(testInterval, testShmName, testSemName, testBufferSize);

    producer.Start();
    std::this_thread::sleep_for(testInterval);
    producer.Stop();
    producer.Stop();
}

/**
 * 测试描述: 心跳生产者正常运行和停止
 * 测试步骤:
 * 1. 启动心跳生产者。
 * 2. 等待足够长的时间，确保心跳线程运行多次。
 * 3. 停止心跳生产者。
 * 预期结果:
 * 1. 心跳生产者能够正常启动、运行和停止，不抛出异常。
 */
TEST_F(HeartbeatProducerTest, HeartbeatProducerRunsAndStops)
{
    HeartbeatProducer producer(testInterval, testShmName, testSemName, testBufferSize);
    producer.Start();
    std::this_thread::sleep_for(testInterval * 3 + std::chrono::milliseconds(5)); // 等待3倍testInterval+5ms后再停止心跳
    producer.Stop();
}

/**
 * 测试描述: 析构函数停止心跳线程
 * 测试步骤:
 * 1. 在一个作用域内创建并启动HeartbeatProducer对象。
 * 2. 等待一段时间。
 * 3. 对象超出作用域，触发析构函数。
 * 预期结果:
 * 1. 析构函数能够正确停止心跳线程，测试成功完成。
 */
TEST_F(HeartbeatProducerTest, DestructorStopsThread)
{
    {
        HeartbeatProducer producer(testInterval, testShmName, testSemName, testBufferSize);
        producer.Start();
        std::this_thread::sleep_for(testInterval);
    }
    SUCCEED();
}
