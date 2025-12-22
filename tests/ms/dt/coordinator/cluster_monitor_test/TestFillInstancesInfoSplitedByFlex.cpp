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
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <memory>
#include <map>
#include <random>
#include <unistd.h>
#include <variant>
#include <vector>
#include <pthread.h>
#include "gtest/gtest.h"

#include "ClusterNodes.h"
#include "Configure.h"
#include "Helper.h"
using namespace MINDIE::MS;
using namespace nlohmann;

// Fixture
class TestFillInstancesInfoSplitedByFlex : public ::testing::Test {
protected:
    void SetUp()
    {
        // 创建对象
        // ClusterNodes nodes;
    }
    void TearDown() {}
};

// 辅助函数，用于创建测试用的 JSON 对象
json CreateTestJson(uint64_t id, uint64_t pPercentage, MINDIE::MS::DIGSInstanceRole role)
{
    json j;
    j["id"] = id;
    j["static_info"]["p_percentage"] = pPercentage;
    j["static_info"]["role"] = static_cast<int>(role);
    return j;
}

// 测试用例：边界情况 - instances 为空
TEST(TestFillInstancesInfoSplitedByFlex, EmptyInstances01)
{
    json instances = json::array();

    std::vector<uint64_t> nodeIds;
    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ProcessFlexInstance(nodeIds, instances);

    EXPECT_EQ(result, 0);
}

// 测试用例：边界情况 - ConvertMInstanceToD 返回错误
TEST(TestFillInstancesInfoSplitedByFlex, ConvertToDError01)
{
    uint64_t flexId = 123;  // 123 id
    json instances = json::array();
    instances.push_back(CreateTestJson(flexId, 0, MINDIE::MS::DIGSInstanceRole::FLEX_INSTANCE));

    std::vector<uint64_t> nodeIds;
    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ProcessFlexInstance(nodeIds, instances);

    EXPECT_EQ(result, -1);
}
