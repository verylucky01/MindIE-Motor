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
#include "gtest/gtest.h"
#include "common.h"


class TestDigsCommon : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TestDigsCommon, GetBlockSizeReturnsDefault)
{
    constexpr int defaultBlockSize = 128;
    EXPECT_EQ(MINDIE::MS::common::GetBlockSize(), defaultBlockSize);
}

// 测试SetBlockSize方法是否正确设置了g_blockSize
TEST_F(TestDigsCommon, SetBlockSize_ShouldSetCorrectly_WhenCalled)
{
    size_t expectedBlockSize = 1024;
    MINDIE::MS::common::SetBlockSize(expectedBlockSize);
    EXPECT_EQ(MINDIE::MS::common::GetBlockSize(), expectedBlockSize);
}

// 测试 GetTimeNow 是否返回非零时间戳
TEST_F(TestDigsCommon, GetTimeNowReturnsNonZero)
{
    uint64_t timeNow = MINDIE::MS::common::GetTimeNow();
    EXPECT_GT(timeNow, 0); // 假设测试运行时的时间戳总是大于零
}

// Dim2Str 测试0个参数
TEST_F(TestDigsCommon, Dims2Str_ShouldReturnCorrectString_WhenDimsArrayIsEmpty)
{
    uint64_t dims[] = {};
    size_t dimCount = 0;
    std::string expected = "[]";
    EXPECT_EQ(MINDIE::MS::common::Dims2Str(dims, dimCount), expected);
}

// Dim2Str 测试1个参数
TEST_F(TestDigsCommon, Dims2Str_ShouldReturnCorrectString_WhenDimsArrayHasOneElement)
{
    uint64_t dims[] = {1};
    size_t dimCount = 1;
    std::string expected = "[1]";
    EXPECT_EQ(MINDIE::MS::common::Dims2Str(dims, dimCount), expected);
}

// Dim2Str 测试多个参数
TEST_F(TestDigsCommon, Dims2Str_ShouldReturnCorrectString_WhenDimsArrayHasMultipleElements)
{
    uint64_t dims[] = {1, 2, 3};
    size_t dimCount = 3;
    std::string expected = "[1,2,3]";
    EXPECT_EQ(MINDIE::MS::common::Dims2Str(dims, dimCount), expected);
}

// GetConfig 测试用例1：当配置项存在时，应返回true并设置config
TEST_F(TestDigsCommon, GetConfig_ShouldReturnTrueAndSetConfig_WhenConfigExists)
{
    std::string name = "testConfig";
    std::string config;
    std::map<std::string, std::string> configs = {{"testConfig", "value"}};

    bool result = MINDIE::MS::common::GetConfig(name, config, configs);

    EXPECT_TRUE(result);
    EXPECT_EQ(config, "value");
}

// GetConfig 测试用例2：当配置项不存在时，应返回false
TEST_F(TestDigsCommon, GetConfig_ShouldReturnFalse_WhenConfigNotExists)
{
    std::string name = "nonExistentConfig";
    std::string config;
    std::map<std::string, std::string> configs = {{"testConfig", "value"}};

    bool result = MINDIE::MS::common::GetConfig(name, config, configs);

    EXPECT_FALSE(result);
}


// GetConfig 测试用例3：当configs中存在name并且可以转换成size_t、uint32_t、double、int32_t时
TEST_F(TestDigsCommon, GetConfig_shouledReturnTrue_whenConfigExistsAndCanConvertToSize)
{
    std::map<std::string, std::string> configs = {{"testConfig", "123"}};
    size_t config = 0;
    uint32_t config1 = 0;
    double config2 = 0;
    int32_t config3 = 0;
    constexpr int testConfigValue = 123;
    EXPECT_TRUE(MINDIE::MS::common::GetConfig("testConfig", config, configs));
    EXPECT_TRUE(MINDIE::MS::common::GetConfig("testConfig", config1, configs));
    EXPECT_TRUE(MINDIE::MS::common::GetConfig("testConfig", config2, configs));
    EXPECT_TRUE(MINDIE::MS::common::GetConfig("testConfig", config3, configs));
    EXPECT_EQ(config, testConfigValue);
}

// GetConfig 测试用例4：当configs中存在name但不能转换为size_t、uint32_t、double、int32_t时
TEST_F(TestDigsCommon, GetConfig_ShouldReturnFalse_WhenConfigExistsButCannotConvertToSizeT)
{
    std::map<std::string, std::string> configs = {{"testConfig", "abc"}};
    size_t config = 0;
    uint32_t config1 = 0;
    double config2 = 0;
    int32_t config3 = 0;
    EXPECT_FALSE(MINDIE::MS::common::GetConfig("testConfig", config, configs));
    EXPECT_FALSE(MINDIE::MS::common::GetConfig("testConfig", config1, configs));
    EXPECT_FALSE(MINDIE::MS::common::GetConfig("testConfig", config2, configs));
    EXPECT_FALSE(MINDIE::MS::common::GetConfig("testConfig", config3, configs));
}

// GetConfig 测试用例5：当configs中不存在name时
TEST_F(TestDigsCommon, GetConfig_ShouldReturnFalse_WhenConfigDoesNotExist)
{
    std::map<std::string, std::string> configs = {{"testConfig", "123"}};
    size_t config = 0;
    uint32_t config1 = 0;
    double config2 = 0;
    int32_t config3 = 0;
    EXPECT_FALSE(MINDIE::MS::common::GetConfig("nonExistentConfig", config, configs));
    EXPECT_FALSE(MINDIE::MS::common::GetConfig("nonExistentConfig", config1, configs));
    EXPECT_FALSE(MINDIE::MS::common::GetConfig("nonExistentConfig", config2, configs));
    EXPECT_FALSE(MINDIE::MS::common::GetConfig("nonExistentConfig", config3, configs));
}

// BlockNUm 测试用例，分别测试tokens和blockSize为0和不为0的四种组合情况
TEST_F(TestDigsCommon, BlockSize_ShouldReturn_BlockSizeAndTokens)
{
    size_t tokens0 = 0;
    size_t tokens1 = 10;
    size_t blockSize0 = 0;
    size_t blockSize1 = 3;
    constexpr int attributeForBlockSizeValue = 4;
    EXPECT_EQ(MINDIE::MS::common::BlockNum(tokens1, blockSize0), 0); // blockSize为0
    EXPECT_EQ(MINDIE::MS::common::BlockNum(tokens0, blockSize1), 0); // blockSize不为0、tokens为0
    EXPECT_EQ(MINDIE::MS::common::BlockNum(tokens1, blockSize1), attributeForBlockSizeValue); // blockSize不为0、tokens不为0
}

// Str2Bool 测试用例，分别测试输入为true、false、空字符串""、其他字符串
TEST_F(TestDigsCommon, Str2BoolTest001)
{
    std::string strValueTrue = "true";
    std::string strValueFalse = "false";
    std::string strValueNull = "";
    std::string strValueOther = "Hello";
    bool boolValue;
    MINDIE::MS::common::Str2Bool(strValueTrue, boolValue);
    EXPECT_TRUE(boolValue);
    MINDIE::MS::common::Str2Bool(strValueFalse, boolValue);
    EXPECT_FALSE(boolValue);
    MINDIE::MS::common::Str2Bool(strValueNull, boolValue);
    EXPECT_FALSE(boolValue);
    MINDIE::MS::common::Str2Bool(strValueOther, boolValue);
    EXPECT_FALSE(boolValue);
}

// DoubleIsZero 测试用例，分别测试大于、小于、等于0 的三种输入情况
TEST_F(TestDigsCommon, DoubleIsZeroTest001)
{
    double dnum0 = 0;
    double dnum1 = 666.666;
    EXPECT_TRUE(MINDIE::MS::common::DoubleIsZero(dnum0));
    EXPECT_FALSE(MINDIE::MS::common::DoubleIsZero(dnum1));
}