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
#include <cstdlib> // #include <stdlib.h>
#include <string>
#include <memory>
#include <pthread.h>
#include "gtest/gtest.h"
#include "ControllerConfig.h"
#include "group_generator/DefaultGroupGenerator.h"
#include "Helper.h"
using namespace MINDIE::MS;
class TestControllerGroupGenerator : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }
    void SetUp()
    {
        auto controllerJson = GetMSControllerTestJsonPath();
        auto testJson = GetServerRequestHandlerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "multi_node_infer_config", "multi_node_infer_enable", false);
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        ControllerConfig::GetInstance()->Init();
    }
};

/*
测试描述: 生成P和D节点的分组测试，P:D比率为1:1，节点数为3P3D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate和DRate分别为1。
    3. 添加3个P节点和3个D节点：
        - P节点：id 分别为 0, 1, 2
        - D节点：id 分别为 0, 1, 2
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成1个组。
    3. 组中包含3个P节点和3个D节点。
*/
TEST(TestControllerGroupGenerator, GenerateGroups01)
{
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加3个P节点和3个D节点
    for (int i = 0; i < 3; ++i) {
        MINDIE::MS::DIGSRoleDecision pNode;
        pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        pNode.id = i;
        instances.push_back(pNode);

        MINDIE::MS::DIGSRoleDecision dNode;
        dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        dNode.id = i;
        instances.push_back(dNode);
    }

    // 设置P和D的比例为1:1
    size_t pRate = 1;
    size_t dRate = 1;

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(0, result);

    // 检查生成的分组信息是否符合预期
    // 预期只有一个组，并且这个组中有3个P节点和3个D节点
    EXPECT_EQ(1, groups.size());
    EXPECT_EQ(3, groups[0].first.size());
    EXPECT_EQ(3, groups[0].second.size());
}

/*
测试描述: 生成P和D节点的分组测试，P:D比率为1:1，节点数为9P8D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate和DRate分别为1。
    3. 添加9个P节点和8个D节点：
        - P节点：id 分别为 0, 1, 2, 3, 4, 5, 6, 7, 8
        - D节点：id 分别为 0, 1, 2, 3, 4, 5, 6, 7
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成2个组。
    3. 第一个组包含5个P节点和4个D节点，第二个组包含4个P节点和4个D节点。
*/
TEST(TestControllerGroupGenerator, GenerateGroups02)
{
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加9个P节点和8个D节点
    for (int i = 0; i < 9; ++i) {
        MINDIE::MS::DIGSRoleDecision pNode;
        pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        pNode.id = i;
        instances.push_back(pNode);
    }
    for (int i = 0; i < 8; ++i) {
        MINDIE::MS::DIGSRoleDecision dNode;
        dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        dNode.id = i;
        instances.push_back(dNode);
    }

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(-1, result);
}

/*
测试描述: 生成P和D节点的分组测试，P:D比率为2:1，节点数为6P3D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate和DRate分别为2和1。
    3. 添加6个P节点和3个D节点：
        - P节点：id 分别为 0, 1, 2, 3, 4, 5
        - D节点：id 分别为 0, 1, 2
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成1个组。
    3. 组中包含6个P节点和3个D节点。
*/
TEST(TestControllerGroupGenerator, GenerateGroups03)
{
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加6个P节点和3个D节点
    for (int i = 0; i < 6; ++i) {
        MINDIE::MS::DIGSRoleDecision pNode;
        pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        pNode.id = i;
        instances.push_back(pNode);
    }
    for (int i = 0; i < 3; ++i) {
        MINDIE::MS::DIGSRoleDecision dNode;
        dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        dNode.id = i;
        instances.push_back(dNode);
    }

    // 设置P和D的比例为2:1
    size_t pRate = 2;
    size_t dRate = 1;

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(0, result);

    // 检查生成的分组信息是否符合预期
    // 预期只有一个组，并且这个组中有6个P节点和3个D节点
    EXPECT_EQ(1, groups.size());
    EXPECT_EQ(6, groups[0].first.size());
    EXPECT_EQ(3, groups[0].second.size());
}

/*
测试描述: 生成P和D节点的分组测试，P:D比率为15:1，节点数为15P1D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate和DRate分别为15和1。
    3. 添加15个P节点和1个D节点：
        - P节点：id 分别为 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
        - D节点：id 分别为 0
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成1个组。
    3. 组中包含15个P节点和1个D节点。
*/
TEST(TestControllerGroupGenerator, GenerateGroups04)
{
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加15个P节点和1个D节点
    for (int i = 0; i < 15; ++i) {
        MINDIE::MS::DIGSRoleDecision pNode;
        pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        pNode.id = i;
        instances.push_back(pNode);
    }

    MINDIE::MS::DIGSRoleDecision dNode;
    dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    dNode.id = 0;
    instances.push_back(dNode);

    // 设置P和D的比例为15:1
    size_t pRate = 15;
    size_t dRate = 1;

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(0, result);

    // 检查生成的分组信息是否符合预期
    // 预期只有一个组，并且这个组中有15个P节点和1个D节点
    EXPECT_EQ(1, groups.size());
    EXPECT_EQ(15, groups[0].first.size());
    EXPECT_EQ(1, groups[0].second.size());
}

/*
测试描述: 生成P和D节点的分组测试，P:D比率为15:1，节点数为15P2D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate和DRate分别为15和1。
    3. 添加15个P节点和2个D节点：
        - P节点：id 分别为 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
        - D节点：id 分别为 0, 1
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成2个组。
    3. 第一个组包含8个P节点和1个D节点，第二个组包含7个P节点和1个D节点。
*/

TEST(TestControllerGroupGenerator, GenerateGroups05)
{
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加15个P节点和2个D节点
    for (int i = 0; i < 15; ++i) {
        MINDIE::MS::DIGSRoleDecision pNode;
        pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        pNode.id = i;
        instances.push_back(pNode);
    }

    for (int i = 0; i < 2; i++) {
        MINDIE::MS::DIGSRoleDecision dNode;
        dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        dNode.id = i;
        instances.push_back(dNode);
    }

    // 设置P和D的比例为15:1
    size_t pRate = 15;
    size_t dRate = 1;

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(-1, result);
}

/*
测试描述: 生成P和D节点的分组测试，P:D比率为1:15，节点数为1P15D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate和DRate分别为1和15。
    3. 添加1个P节点和15个D节点：
        - P节点：id 分别为 0
        - D节点：id 分别为 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成1个组。
    3. 组中包含1个P节点和15个D节点。
*/
TEST(TestControllerGroupGenerator, GenerateGroups06)
{
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加1个P节点和15个D节点
    MINDIE::MS::DIGSRoleDecision pNode;
    pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    pNode.id = 0;
    instances.push_back(pNode);

    for (int i = 0; i < 15; ++i) {
        MINDIE::MS::DIGSRoleDecision dNode;
        dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        dNode.id = i;
        instances.push_back(dNode);
    }

    // 设置P和D的比例为1:15
    size_t pRate = 1;
    size_t dRate = 15;

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(0, result);
}

/*
测试描述: 生成P和D节点的分组测试，P:D比率为15:1，节点数为15P2D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate和DRate分别为15和1。
    3. 添加15个P节点和2个D节点：
        - P节点：id 分别为 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
        - D节点：id 分别为 0, 1
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成2个组。
    3. 第一个组包含8个P节点和1个D节点，第二个组包含7个P节点和1个D节点。
*/
TEST(TestControllerGroupGenerator, GenerateGroups07)
{
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加15个P节点和2个D节点
    for (int i = 0; i < 2; ++i) {
        MINDIE::MS::DIGSRoleDecision pNode;
        pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        pNode.id = i;
        instances.push_back(pNode);
    }

    for (int i = 0; i < 15; i++) {
        MINDIE::MS::DIGSRoleDecision dNode;
        dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        dNode.id = i;
        instances.push_back(dNode);
    }

    // 设置P和D的比例为15:1
    size_t pRate = 1;
    size_t dRate = 15;

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(-1, result);

}

/*
测试描述: 生成P和D节点的分组测试，P:D比率为1:15，节点数为5P89D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate为1，DRate为15。
    3. 添加5个P节点和89个D节点：
        - P节点：id 分别为 0-4
        - D节点：id 分别为 5-93
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成5个组。
*/
TEST(TestControllerGroupGenerator, GenerateGroups08) {
    auto testJson = GetServerRequestHandlerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加5个P节点
    for (int i = 0; i < 5; ++i) {
        MINDIE::MS::DIGSRoleDecision pNode;
        pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        pNode.id = i;
        instances.push_back(pNode);
    }

    // 添加89个D节点
    for (int i = 5; i < 94; ++i) {
        MINDIE::MS::DIGSRoleDecision dNode;
        dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        dNode.id = i;
        instances.push_back(dNode);
    }

    // 设置P和D的比例为1:15
    size_t pRate = 1;
    size_t dRate = 15;

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(-1, result);
}

/*
测试描述: 生成P和D节点的分组测试，P:D比率为1:15，节点数为6P89D
测试步骤:
    1. 创建一个DefaultGroupGenerator实例。
    2. 设置PRate为1，DRate为15。
    3. 添加6个P节点和89个D节点：
        - P节点：id 分别为 0-5
        - D节点：id 分别为 6-95
    4. 调用GenerateGroups方法，传入上述实例和比率。
    5. 解析生成的组信息。
预期结果:
    1. GenerateGroups方法返回0表示成功。
    2. 生成5个组。
*/
TEST(TestControllerGroupGenerator, GenerateGroups09) {
    auto testJson = GetServerRequestHandlerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    // 创建一个DefaultGroupGenerator实例
    DefaultGroupGenerator generator;

    // 设置一些测试数据
    std::vector<MINDIE::MS::DIGSRoleDecision> instances;

    // 添加6个P节点
    for (int i = 0; i < 6; ++i) {
        MINDIE::MS::DIGSRoleDecision pNode;
        pNode.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        pNode.id = i;
        instances.push_back(pNode);
    }

    // 添加89个D节点
    for (int i = 6; i < 95; ++i) {
        MINDIE::MS::DIGSRoleDecision dNode;
        dNode.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        dNode.id = i;
        instances.push_back(dNode);
    }

    // 设置P和D的比例为1:15
    size_t pRate = 1;
    size_t dRate = 15;

    // 调用要测试的函数
    std::vector<std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::vector<std::vector<uint64_t>> flexGroups;
    int32_t result = generator.GenerateGroups(instances, groups, flexGroups);

    // 检查结果是否符合预期
    EXPECT_EQ(-1, result);
}