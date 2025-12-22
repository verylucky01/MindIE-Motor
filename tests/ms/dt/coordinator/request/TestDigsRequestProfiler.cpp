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
#include "coordinator/request/request_profiler.h"

using namespace testing;

class TestDigsRequestProfiler : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @tc.name  : RequestProfiler_ShouldSetMaxSummaryCount_WhenConfigHasMaxSummaryCount
 * @tc.number: RequestProfilerTest_001
 * @tc.desc  : Test when RequestConfig has maxSummaryCount then RequestProfiler should set maxSummaryCount_
 */
TEST_F(TestDigsRequestProfiler, RequestProfiler_ShouldSetMaxSummaryCount_WhenConfigHasMaxSummaryCount)
{
    constexpr int maxSummaryCountValue = 10;
    MINDIE::MS::RequestConfig config;
    config.maxSummaryCount = maxSummaryCountValue;

    MINDIE::MS::RequestProfiler profiler(config);
    EXPECT_EQ(profiler.maxSummaryCount_, config.maxSummaryCount);
}

/**
 * @tc.name  : RequestProfiler_ShouldSetMaxSummaryCountToZero_WhenConfigHasNoMaxSummaryCount
 * @tc.number: RequestProfilerTest_002
 * @tc.desc  : Test when RequestConfig has no maxSummaryCount then RequestProfiler should set maxSummaryCount_ to zero
 */
TEST_F(TestDigsRequestProfiler, RequestProfiler_ShouldSetMaxSummaryCountToZero_WhenConfigHasNoMaxSummaryCount)
{
    constexpr int maxSummaryCountValue = 1048576;
    MINDIE::MS::RequestConfig config;
    MINDIE::MS::RequestProfiler profiler(config);
    EXPECT_EQ(profiler.maxSummaryCount_, maxSummaryCountValue);
}

/**
 * @tc.name  : getAvgInputLength_ShouldReturnZero_WhenNoInputs
 * @tc.number: getAvgInputLengthTest_002
 * @tc.desc  : Test GetAvgInputLength method returns zero when no inputs are provided
 */
TEST_F(TestDigsRequestProfiler, getAvgInputLength_ShouldReturnZero_WhenNoInputs)
{
    MINDIE::MS::RequestConfig config;
    MINDIE::MS::RequestProfiler profiler(config);
    EXPECT_EQ(profiler.GetAvgInputLength(), 0.0);
}

/**
 * @tc.name  : getAvgOutputLength_ShouldReturnZero_WhenNoOutput
 * @tc.number: getAvgOutputLengthTest_002
 * @tc.desc  : Test GetAvgOutputLength method returns zero when no output has been recorded
 */
TEST_F(TestDigsRequestProfiler, getAvgOutputLength_ShouldReturnZero_WhenNoOutput)
{
    MINDIE::MS::RequestConfig config;
    MINDIE::MS::RequestProfiler profiler(config);
    EXPECT_EQ(profiler.GetAvgOutputLength(), 0.0);
}
