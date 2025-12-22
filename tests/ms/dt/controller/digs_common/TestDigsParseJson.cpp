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
#include "parse_json.h"
#include "Logger.h"
#include "common.h"

using namespace testing;

class TestDigsParseJson : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

/**
 * @tc.name  : Object2Double_ShouldReturnIllegalParameter_WhenConfigIsNull
 * @tc.number: Object2DoubleTest_001
 * @tc.desc  : 当config为空时，Object2Double应返回ILLEGAL_PARAMETER
 */
TEST_F(TestDigsParseJson, Object2Double_ShouldReturnIllegalParameter_WhenConfigIsNull)
{
    Json config = nullptr;
    std::string key = "testKey";
    int32_t result = MINDIE::MS::common::Object2Double(config, key);
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

/**
 * @tc.name  : Object2Double_ShouldReturnIllegalParameter_WhenKeyIsNotSet
 * @tc.number: Object2DoubleTest_002
 * @tc.desc  : 当config中没有设置key时，Object2Double应返回ILLEGAL_PARAMETER
 */
TEST_F(TestDigsParseJson, Object2Double_ShouldReturnIllegalParameter_WhenKeyIsNotSet)
{
    Json config;
    std::string key = "testKey";
    int32_t result = MINDIE::MS::common::Object2Double(config, key);
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

/**
 * @tc.name  : Object2Double_ShouldReturnIllegalParameter_WhenKeyIsStringAndCannotConvertToDouble
 * @tc.number: Object2DoubleTest_003
 * @tc.desc  : 当key为字符串且无法转换为double时，Object2Double应返回ILLEGAL_PARAMETER
 */
TEST_F(TestDigsParseJson, Object2Double_ShouldReturnIllegalParameter_WhenKeyIsStringAndCannotConvertToDouble)
{
    Json config;
    std::string key = "testKey";
    config[key] = "not_a_number";
    int32_t result = MINDIE::MS::common::Object2Double(config, key);
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

/**
 * @tc.name  : Object2Double_ShouldReturnOk_WhenKeyIsNumber
 * @tc.number: Object2DoubleTest_004
 * @tc.desc  : 当key为数字时，Object2Double应返回OK
 */
TEST_F(TestDigsParseJson, Object2Double_ShouldReturnOk_WhenKeyIsNumber)
{
    Json config;
    std::string key = "testKey";
    constexpr double attributeForObjectValue = 123.456;
    config[key] = attributeForObjectValue;
    int32_t result = MINDIE::MS::common::Object2Double(config, key);
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
}

/**
 * @tc.name  : Object2Double_ShouldReturnOk_WhenKeyIsStringAndCanConvertToDouble
 * @tc.number: Object2DoubleTest_005
 * @tc.desc  : 当key为字符串且可以转换为double时，Object2Double应返回OK
 */
TEST_F(TestDigsParseJson, Object2Double_ShouldReturnOk_WhenKeyIsStringAndCanConvertToDouble)
{
    Json config;
    std::string key = "testKey";
    constexpr double attributeForObjectValue = 123.456;
    config[key] = attributeForObjectValue;
    int32_t result = MINDIE::MS::common::Object2Double(config, key);
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
}