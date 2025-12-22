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
#include <vector>
#include <gtest/gtest.h>
#include "SharedMemoryUtils.h"

using namespace MINDIE::MS;

const std::string SHM_NAME = "/test_shm";
const std::string SEM_NAME = "/test_sem";
const size_t BUFFER_SIZE = 1024;

class TestSharedMemoryUtils : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 清理资源防止冲突
        SharedMemoryUtils::ClearResources(SHM_NAME, SEM_NAME);
    }

    void TearDown() override
    {
        SharedMemoryUtils::ClearResources(SHM_NAME, SEM_NAME);
    }
};

// 测试 Retain 模式下的写入与读取
TEST_F(TestSharedMemoryUtils, RetainMode_WriteAndRead)
{
    RetainSharedMemoryUtils writer(SHM_NAME, SEM_NAME);
    RetainSharedMemoryUtils reader(SHM_NAME, SEM_NAME);

    const std::string msg1 = "Hello, World!";
    const std::string msg2 = "Another message.#2";
    const std::string msg3 = "Another message.#3";
    const std::string msg4 = "Another message.#4";
    const std::string msg5 = "Another message.#5";

    EXPECT_TRUE(writer.Write(msg1));
    EXPECT_EQ(reader.Read(), msg1);
    EXPECT_TRUE(writer.Write(msg2));
    EXPECT_EQ(reader.Read(), msg2);
    EXPECT_TRUE(writer.Write(msg3));
    EXPECT_EQ(reader.Read(), msg3);
    EXPECT_TRUE(writer.Write(msg4));
    EXPECT_EQ(reader.Read(), msg4);
    EXPECT_TRUE(writer.Write(msg5));
    EXPECT_EQ(reader.Read(), msg5);
}

// 测试缓冲区满时写入失败
TEST_F(TestSharedMemoryUtils, RetainMode_BufferFull)
{
    RetainSharedMemoryUtils writer(SHM_NAME, SEM_NAME);
    RetainSharedMemoryUtils reader(SHM_NAME, SEM_NAME);

    size_t bufferSize = writer.bufferSize_;
    std::string bigMsg(bufferSize - 1, 'A'); // 消息长度刚好填满 buffer

    EXPECT_FALSE(writer.Write(bigMsg)); // 因为 '\0' 占一位，所以会失败

    std::string smallMsg("small");
    EXPECT_TRUE(writer.Write(smallMsg));
    EXPECT_EQ(reader.Read(), smallMsg);
}

// 测试 Overwrite 模式始终覆盖上一条消息
TEST_F(TestSharedMemoryUtils, OverwriteMode_Overwrites)
{
    OverwriteSharedMemoryUtils writer(SHM_NAME, SEM_NAME);
    OverwriteSharedMemoryUtils reader(SHM_NAME, SEM_NAME);

    const std::string msg1 = "Hello, World!";
    const std::string msg2 = "Another message.#2";
    const std::string msg3 = "Another message.#3";
    const std::string msg4 = "Another message.#4";
    const std::string msg5 = "Another message.#5";

    EXPECT_TRUE(writer.Write(msg1));
    EXPECT_EQ(reader.Read(), msg1);
    EXPECT_TRUE(writer.Write(msg2));
    EXPECT_EQ(reader.Read(), msg2);
    EXPECT_TRUE(writer.Write(msg3));
    EXPECT_EQ(reader.Read(), msg3);
    EXPECT_TRUE(writer.Write(msg4));
    EXPECT_EQ(reader.Read(), msg4);
    EXPECT_TRUE(writer.Write(msg5));
    EXPECT_EQ(reader.Read(), msg5);
}

// 测试共享内存初始化是否正常
TEST_F(TestSharedMemoryUtils, InitializationSuccess)
{
    RetainSharedMemoryUtils utils(SHM_NAME, SEM_NAME);
    EXPECT_NE(utils.data_, nullptr);
    EXPECT_NE(utils.cb_, nullptr);
    EXPECT_NE(utils.sem_, SEM_FAILED);
}

// 测试Owner实例析构，非Owner实例读取
TEST(SharedMemoryUtilsDeathTest, NonOwnerReadAfterOwnerDestroyed)
{
    // Step 1: 创建 Owner 实例
    {
        RetainSharedMemoryUtils owner(SHM_NAME, SEM_NAME, BUFFER_SIZE);
        EXPECT_TRUE(owner.Write("Hello from owner"));
    } // Owner 析构，会清理 shm 和 sem

    // Step 2: 非Owner尝试连接
    RetainSharedMemoryUtils nonOwner(SHM_NAME, SEM_NAME, BUFFER_SIZE);

    // Step 3: 尝试读取
    std::string result = nonOwner.Read(); // 应该失败

    // 根据当前实现，sem_wait 会失败（因为sem已经被unlink），所以 Read() 会直接返回 ""
    EXPECT_EQ(result, "");
}