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
#include <map>
#include <iostream>
#include <string>
#include "gtest/gtest.h"
#include "Metrics.h"
#include "Helper.h"
#include "stub.h"

using namespace MINDIE::MS;

// 假设 Metrics 类和相关函数已经定义
class TestParseMetric : public testing::Test {
protected:
    void SetUp() override
    {
        test_json.clear();
        test_line.clear();
    }

    void TearDown() override
    {
        // 清理测试环境
    }

    nlohmann::json test_json;
    std::string test_line;
};

/**
   测试 ParseMetricHelp 函数的基本功能
 */
TEST_F(TestParseMetric, ParseMetricHelp_NormalCase)
{
    // 测试输入：包含 # HELP 的有效行
    std::string input_line = "# HELP test_metric This is a test help message\n";
    Metrics metricInstance;
    metricInstance.InitMetricsPattern();
    int32_t result = metricInstance.ParseMetricHelp(test_json, input_line);

    // 验证返回值
    EXPECT_EQ(result, 0);

    // 验证解析结果
    EXPECT_EQ(test_json["NAME"], "test_metric");
    EXPECT_EQ(test_json["HELP"], "This is a test help message");
}

/**
   测试 ParseMetricHelp 函数在缺少 # HELP 前缀时的行为
 */
TEST_F(TestParseMetric, ParseMetricHelp_MissingHELP)
{
    // 测试输入：缺少 # HELP 前缀
    std::string input_line = "test_metric This is a test help message\n";
    Metrics metricInstance;
    metricInstance.InitMetricsPattern();
    int32_t result = metricInstance.ParseMetricHelp(test_json, input_line);

    // 验证返回值
    EXPECT_EQ(result, -1);
}

/**
   测试 ParseMetricHelp 函数在缺少 metric name 时的行为
 */
TEST_F(TestParseMetric, ParseMetricHelp_MissingMetricName)
{
    // 测试输入：缺少 metric name
    std::string input_line = "# HELP  This is a test help message\n";
    Metrics metricInstance;
    metricInstance.InitMetricsPattern();
    int32_t result = metricInstance.ParseMetricHelp(test_json, input_line);
}

/**
   测试 ParseMetricHelp 函数在缺少帮助文本时的行为
 */
TEST_F(TestParseMetric, ParseMetricHelp_MissingHelpText)
{
    // 测试输入：缺少帮助文本
    std::string input_line = "# HELP test_metric\n";
    Metrics metricInstance;
    metricInstance.InitMetricsPattern();
    int32_t result = metricInstance.ParseMetricHelp(test_json, input_line);

    // 验证返回值
    EXPECT_EQ(result, -1);
}

/**
   测试 ParseMetricHelp 函数在行尾没有 \n 时的行为
 */
TEST_F(TestParseMetric, ParseMetricHelp_MissingLineBreak)
{
    // 测试输入：行尾没有 \n
    std::string input_line = "# HELP test_metric This is a test help message";
    Metrics metricInstance;
    metricInstance.InitMetricsPattern();
    int32_t result = metricInstance.ParseMetricHelp(test_json, input_line);

    // 验证返回值
    EXPECT_EQ(result, -1);
}

/**
   测试 ParseMetricHelp 函数在 metric name 为空时的行为
 */
TEST_F(TestParseMetric, ParseMetricHelp_EmptyMetricName)
{
    // 测试输入：metric name 为空
    std::string input_line = "# HELP  This is a test help message\n";
    Metrics metricInstance;
    metricInstance.InitMetricsPattern();
    int32_t result = metricInstance.ParseMetricHelp(test_json, input_line);
}

/**
   测试 ParseMetricHelp 函数在帮助文本为空时的行为
 */
TEST_F(TestParseMetric, ParseMetricHelp_EmptyHelpText)
{
    // 测试输入：帮助文本为空
    std::string input_line = "# HELP test_metric \n";
    Metrics metricInstance;
    metricInstance.InitMetricsPattern();
    int32_t result = metricInstance.ParseMetricHelp(test_json, input_line);
}

// 测试用例：ParseMetricType 正常情况
TEST_F(TestParseMetric, NormalCase)
{
    nlohmann::json singleMetirc;
    std::string line = "# TYPE example_metric gauge\n";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricType(singleMetirc, line);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(singleMetirc["TYPE"], "gauge");
}

// 测试用例：ParseMetricType 缺少# TYPE
TEST_F(TestParseMetric, MissingType)
{
    nlohmann::json singleMetirc;
    std::string line = "TYPE example_metric gauge\n";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricType(singleMetirc, line);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("NAME"));
    EXPECT_FALSE(singleMetirc.contains("TYPE"));
}

// 测试用例：ParseMetricType 缺少指标名称
TEST_F(TestParseMetric, MissingMetricName)
{
    nlohmann::json singleMetirc;
    std::string line = "# TYPE gauge\n";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricType(singleMetirc, line);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("NAME"));
    EXPECT_FALSE(singleMetirc.contains("TYPE"));
}

// 测试用例：ParseMetricType 缺少类型
TEST_F(TestParseMetric, MissingTypeValue)
{
    nlohmann::json singleMetirc;
    std::string line = "# TYPE example_metric\n";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricType(singleMetirc, line);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("NAME"));
    EXPECT_FALSE(singleMetirc.contains("TYPE"));
}

// 测试用例：ParseMetricType 格式错误（缺少空格）
TEST_F(TestParseMetric, FormatErrorNoSpace)
{
    nlohmann::json singleMetirc;
    std::string line = "#TYPEexample_metricgauge\n";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricType(singleMetirc, line);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("NAME"));
    EXPECT_FALSE(singleMetirc.contains("TYPE"));
}

// 测试用例：ParseMetricType 格式错误（指标名称前无空格）
TEST_F(TestParseMetric, FormatErrorNoSpaceAfterType)
{
    nlohmann::json singleMetirc;
    std::string line = "# TYPEexample_metric gauge\n";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricType(singleMetirc, line);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("NAME"));
    EXPECT_FALSE(singleMetirc.contains("TYPE"));
}

// 测试用例：ParseMetricType 额外内容
TEST_F(TestParseMetric, ExtraContent)
{
    nlohmann::json singleMetirc;
    std::string line = "# TYPE example_metric gauge extra\n";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricType(singleMetirc, line);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("NAME"));
}

// 测试用例：ParseMetricBodyBlock 正常情况
TEST_F(TestParseMetric, NormalCase01)
{
    nlohmann::json singleMetirc;
    std::string body = "label1 123.45";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBodyBlock(singleMetirc, body);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(singleMetirc["LABEL"][0], "label1");
    EXPECT_DOUBLE_EQ(singleMetirc["VALUE"][0], 123.45);
}

// 测试用例：ParseMetricBodyBlock 缺少标签
TEST_F(TestParseMetric, MissingLabel01)
{
    nlohmann::json singleMetirc;
    std::string body = "123.45";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBodyBlock(singleMetirc, body);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("VALUE"));
}

// 测试用例：ParseMetricBodyBlock 缺少数值
TEST_F(TestParseMetric, MissingValue01)
{
    nlohmann::json singleMetirc;
    std::string body = "label1";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBodyBlock(singleMetirc, body);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("VALUE"));
}

// 测试用例：ParseMetricBodyBlock 标签和数值之间缺少空格
TEST_F(TestParseMetric, MissingSpace01)
{
    nlohmann::json singleMetirc;
    std::string body = "label1123.45";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBodyBlock(singleMetirc, body);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("VALUE"));
}

// 测试用例：ParseMetricBodyBlock 数值格式错误
TEST_F(TestParseMetric, InvalidValue01)
{
    nlohmann::json singleMetirc;
    std::string body = "label1 abc";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBodyBlock(singleMetirc, body);

    EXPECT_EQ(result, -1);
    EXPECT_FALSE(singleMetirc.contains("VALUE"));
}

// 测试用例：ParseMetricBodyBlock 额外内容
TEST_F(TestParseMetric, ExtraContent02)
{
    nlohmann::json singleMetirc;
    std::string body = "label1 123.45 extra";
    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBodyBlock(singleMetirc, body);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(singleMetirc["LABEL"][0], "label1");
    EXPECT_DOUBLE_EQ(singleMetirc["VALUE"][0], 123.45);
}

// 辅助函数，用于比较字符串，忽略换行符和空格
bool CompareStrings(const std::string &str1, const std::string &str2)
{
    std::string s1 = str1;
    std::string s2 = str2;
    // 去除换行符和空格
    s1.erase(std::remove(s1.begin(), s1.end(), '\n'), s1.end());
    s1.erase(std::remove(s1.begin(), s1.end(), ' '), s1.end());
    s2.erase(std::remove(s2.begin(), s2.end(), '\n'), s2.end());
    s2.erase(std::remove(s2.begin(), s2.end(), ' '), s2.end());
    return s1 == s2;
}

// 测试用例：ParseMetricBody 正常情况
TEST_F(TestParseMetric, NormalCase02)
{
    nlohmann::json metricArray;
    nlohmann::json singleMetirc;
    std::string line = "label1 123.45\nlabel2 678.90\n# END\n";
    uint32_t count = 0;
    bool isParsing = true;

    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBody(metricArray, singleMetirc, line, count, isParsing);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 1);
    EXPECT_EQ(metricArray.size(), 1);
    EXPECT_EQ(metricArray[0]["LABEL"].size(), 2);
    EXPECT_DOUBLE_EQ(metricArray[0]["VALUE"][0], 123.45);
    EXPECT_DOUBLE_EQ(metricArray[0]["VALUE"][1], 678.90);
}

// 测试用例：ParseMetricBody 缺少结束标记
TEST_F(TestParseMetric, MissingEndMarker02)
{
    nlohmann::json metricArray;
    nlohmann::json singleMetirc;
    std::string line = "label1 123.45\nlabel2 678.90\n";
    uint32_t count = 0;
    bool isParsing = true;

    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBody(metricArray, singleMetirc, line, count, isParsing);

    EXPECT_EQ(result, 0);
    EXPECT_EQ(count, 1);
    EXPECT_FALSE(isParsing);
    EXPECT_EQ(metricArray.size(), 1);
    EXPECT_EQ(metricArray[0]["LABEL"].size(), 2);
    EXPECT_DOUBLE_EQ(metricArray[0]["VALUE"][0], 123.45);
    EXPECT_DOUBLE_EQ(metricArray[0]["VALUE"][1], 678.90);
}

// 测试用例：ParseMetricBody 解析失败
TEST_F(TestParseMetric, ParseFailure02)
{
    nlohmann::json metricArray;
    nlohmann::json singleMetirc;
    std::string line = "label1 abc\n";
    uint32_t count = 0;
    bool isParsing = true;

    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBody(metricArray, singleMetirc, line, count, isParsing);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(count, 0);
    EXPECT_TRUE(isParsing);
    EXPECT_EQ(metricArray.size(), 0);
}

// 测试用例：ParseMetricBody 空输入
TEST_F(TestParseMetric, EmptyInput02)
{
    nlohmann::json metricArray;
    nlohmann::json singleMetirc;
    std::string line = "";
    uint32_t count = 0;
    bool isParsing = true;

    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBody(metricArray, singleMetirc, line, count, isParsing);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(count, 0);
    EXPECT_EQ(metricArray.size(), 0);
}

// 测试用例：ParseMetricBody 异常情况
TEST_F(TestParseMetric, InvalidFormat02)
{
    nlohmann::json metricArray;
    nlohmann::json singleMetirc;
    std::string line = "invalid line\n";
    uint32_t count = 0;
    bool isParsing = true;

    Metrics metricsParser;
    metricsParser.InitMetricsPattern();
    int32_t result = metricsParser.ParseMetricBody(metricArray, singleMetirc, line, count, isParsing);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(count, 0);
    EXPECT_TRUE(isParsing);
    EXPECT_EQ(metricArray.size(), 0);
}