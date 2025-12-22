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
#include <cstdlib>  // #include <stdlib.h>
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

// Fixture
class TestClusterMonitor : public ::testing::Test {
protected:
    void SetUp()
    {
        // 创建对象
        // ClusterNodes nodes;
    }
    void TearDown() {}
};

/*
测试描述: ClusterMonitor测试，测试函数功能能够正常运行
测试步骤:
    1. 调用函数进行操作
预期结果:
    1. 正确操作
*/
TEST_F(TestClusterMonitor, ClusterNodesFunctionsSuccessTCa)
{
    ClusterNodes nodes;

    std::unordered_set<std::string> expectedReq;

    std::unordered_set<std::string> noTask = {};

    InstanceInfo expected0("127.0.0.1", "8001", MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, "model0");
    InstanceInfo expected00("127.0.0.11", "8011", MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, "model01");
    InstanceInfo expected1("127.0.0.2", "8002", MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, "model0");
    InstanceInfo expected2("127.0.0.3", "8003", MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE, "model0");
    uint64_t expected0Id = 333;
    uint64_t expected1Id = 222;
    uint64_t expected2Id = 111;
    const std::string req0 = "123";
    const std::string req1 = "456";
    const std::string req2 = "789";
    nodes.GetInstanceInfos();

    // 验证在没有node的情况下，不会返回有node的信息,无法Decrease
    EXPECT_EQ(nodes.GetTask(expected0Id), -1);
    EXPECT_EQ(nodes.GetIp(expected0Id), "");
    EXPECT_EQ(nodes.GetPort(expected0Id), "");
    EXPECT_EQ(nodes.GetRole(expected0Id), MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE);
    EXPECT_EQ(nodes.GetId(expected0.ip, expected0.port), UINT64_MAX);
    EXPECT_EQ(nodes.GetRetry(expected0Id), 0);
    EXPECT_EQ(nodes.GetModelName(expected0Id), "");
    EXPECT_FALSE(nodes.HasModelName(expected0.modelName));
    EXPECT_EQ(nodes.GetTasksById(expected0Id), noTask);
    // DecreaseTask失败分支，无返回值
    nodes.DecreaseTask(expected0Id, req0);
    // GetTokenizerIns失败分支
    EXPECT_EQ(nodes.GetTokenizerIns(), UINT64_MAX);
    // RemoveInstance失败分支，无返回值
    nodes.RemoveInstance(expected0Id);
    // 空的时候非pd分离返回false
    auto conf = Configure::Singleton();
    conf->schedulerConfig["deploy_mode"] = "other";
    EXPECT_FALSE(nodes.IsAvailable());

    // AddInstance
    nodes.AddInstance(expected0Id, expected0.ip, expected0.port, expected0.role, expected0.modelName);
    EXPECT_EQ(nodes.GetTask(expected0Id), 0);
    EXPECT_EQ(nodes.GetIp(expected0Id), expected0.ip);
    EXPECT_EQ(nodes.GetPort(expected0Id), expected0.port);
    EXPECT_EQ(nodes.GetRole(expected0Id), expected0.role);
    EXPECT_EQ(nodes.GetId(expected0.ip, expected0.port), expected0Id);
    EXPECT_EQ(nodes.GetModelName(expected0Id), expected0.modelName);
    EXPECT_TRUE(nodes.HasModelName(expected0.modelName));
    EXPECT_EQ(nodes.GetTasksById(expected0Id), noTask);

    // 重复AddInstance
    nodes.AddInstance(expected0Id, expected00.ip, expected00.port, expected00.role, expected00.modelName);
    EXPECT_EQ(nodes.GetTask(expected0Id), 0);
    EXPECT_EQ(nodes.GetIp(expected0Id), expected0.ip);
    EXPECT_EQ(nodes.GetPort(expected0Id), expected0.port);
    EXPECT_EQ(nodes.GetRole(expected0Id), expected0.role);
    EXPECT_EQ(nodes.GetId(expected00.ip, expected00.port), UINT64_MAX);
    EXPECT_EQ(nodes.GetModelName(expected0Id), expected0.modelName);
    EXPECT_FALSE(nodes.HasModelName(expected00.modelName));

    // AddTask
    nodes.AddTask(expected0Id, req0);
    EXPECT_EQ(nodes.GetTask(expected0Id), 1);
    expectedReq = {req0};
    EXPECT_EQ(nodes.GetTasksById(expected0Id), expectedReq);
    nodes.AddTask(expected0Id, req1);
    EXPECT_EQ(nodes.GetTask(expected0Id), 2);
    expectedReq = {req0, req1};
    EXPECT_EQ(nodes.GetTasksById(expected0Id), expectedReq);
    // AddTask失败分支，无返回值
    nodes.AddTask(expected1Id, req0);

    // AddRetry
    nodes.AddRetry(expected0Id);
    EXPECT_EQ(nodes.GetRetry(expected0Id), 1);
    nodes.AddRetry(expected0Id);
    EXPECT_EQ(nodes.GetRetry(expected0Id), 2);
    // AddRetry失败分支，无返回值
    nodes.AddRetry(expected1Id);

    // AddInstance && GetTokenizerIns
    nodes.AddInstance(expected1Id, expected1.ip, expected1.port, expected1.role, expected1.modelName);
    EXPECT_EQ(nodes.GetTask(expected1Id), 0);
    EXPECT_EQ(nodes.GetIp(expected1Id), expected1.ip);
    EXPECT_EQ(nodes.GetPort(expected1Id), expected1.port);
    EXPECT_EQ(nodes.GetRole(expected1Id), expected1.role);
    EXPECT_EQ(nodes.GetId(expected1.ip, expected1.port), expected1Id);
    EXPECT_EQ(nodes.GetModelName(expected1Id), expected1.modelName);
    EXPECT_EQ(nodes.GetTasksById(expected1Id), noTask);
    // AddTask，使得新增的实例有一个Task
    nodes.AddTask(expected1Id, req2);
    EXPECT_EQ(nodes.GetTask(expected1Id), 1);
    expectedReq = {req2};
    EXPECT_EQ(nodes.GetTasksById(expected1Id), expectedReq);
    // GetTokenizerIns
    EXPECT_EQ(nodes.GetTokenizerIns(), expected1Id);

    // IsAvailable
    conf->schedulerConfig["deploy_mode"] = "pd_separate";
    EXPECT_TRUE(nodes.IsAvailable());

    // Roll
    std::vector<uint64_t> newIds = {expected1Id, expected2Id};
    auto roll_result = nodes.Roll(newIds);
    std::vector<uint64_t> expected_addVec = {expected2Id};
    std::vector<uint64_t> expected_updateVec = {expected1Id};
    std::vector<uint64_t> expected_removeVec = {expected0Id};
    EXPECT_EQ(std::get<0>(roll_result), expected_addVec);
    EXPECT_EQ(std::get<1>(roll_result), expected_updateVec);
    EXPECT_EQ(std::get<2>(roll_result), expected_removeVec);

    // DecreaseTask
    nodes.DecreaseTask(expected0Id, req0);
    EXPECT_EQ(nodes.GetTask(expected0Id), 1);
    std::unordered_set<std::string> dt1 = {req1};

    // RemoveInstance
    nodes.RemoveInstance(expected0Id);
    EXPECT_EQ(nodes.GetTask(expected0Id), -1);
    EXPECT_EQ(nodes.GetIp(expected0Id), "");
    EXPECT_EQ(nodes.GetPort(expected0Id), "");
    EXPECT_EQ(nodes.GetRole(expected0Id), MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE);
    EXPECT_EQ(nodes.GetId(expected0.ip, expected0.port), UINT64_MAX);
    EXPECT_EQ(nodes.GetRetry(expected0Id), 0);
    EXPECT_EQ(nodes.GetModelName(expected0Id), "");
    EXPECT_EQ(nodes.GetTasksById(expected0Id), noTask);

    // RemoveInstance
    nodes.RemoveInstance(expected1Id);
    EXPECT_EQ(nodes.GetTask(expected1Id), -1);
    EXPECT_EQ(nodes.GetIp(expected1Id), "");
    EXPECT_EQ(nodes.GetPort(expected1Id), "");
    EXPECT_EQ(nodes.GetRole(expected1Id), MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE);
    EXPECT_EQ(nodes.GetId(expected1.ip, expected1.port), UINT64_MAX);
    EXPECT_EQ(nodes.GetRetry(expected1Id), 0);
    EXPECT_EQ(nodes.GetModelName(expected1Id), "");
    EXPECT_EQ(nodes.GetTasksById(expected1Id), noTask);

    // AddInstance, role in un_def
    nodes.AddInstance(expected2Id, expected2.ip, expected2.port, expected2.role, expected2.modelName);
    EXPECT_EQ(nodes.GetTask(expected2Id), 0);
    EXPECT_EQ(nodes.GetIp(expected2Id), expected2.ip);
    EXPECT_EQ(nodes.GetPort(expected2Id), expected2.port);
    EXPECT_EQ(nodes.GetRole(expected2Id), expected2.role);
    EXPECT_EQ(nodes.GetId(expected2.ip, expected2.port), expected2Id);
    EXPECT_EQ(nodes.GetModelName(expected2Id), expected2.modelName);
    EXPECT_TRUE(nodes.HasModelName(expected2.modelName));
    EXPECT_EQ(nodes.GetTasksById(expected2Id), noTask);

    // RemoveInstance
    nodes.RemoveInstance(expected2Id);
    EXPECT_EQ(nodes.GetTask(expected2Id), -1);
    EXPECT_EQ(nodes.GetIp(expected2Id), "");
    EXPECT_EQ(nodes.GetPort(expected2Id), "");
    EXPECT_EQ(nodes.GetRole(expected2Id), MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE);
    EXPECT_EQ(nodes.GetId(expected2.ip, expected2.port), UINT64_MAX);
    EXPECT_EQ(nodes.GetRetry(expected2Id), 0);
    EXPECT_EQ(nodes.GetModelName(expected2Id), "");
    EXPECT_EQ(nodes.GetTasksById(expected2Id), noTask);
}

// 辅助函数，用于创建测试用的 JSON 对象
nlohmann::json CreateTestJson(uint64_t id, uint64_t groupId, MINDIE::MS::DIGSInstanceRole role,
                              const std::vector<uint64_t> &peers)
{
    nlohmann::json j;
    j["id"] = id;
    j["static_info"]["group_id"] = groupId;
    j["static_info"]["role"] = static_cast<int>(role);
    j["dynamic_info"]["peers"] = peers;
    return j;
}

// 测试用例：正常情况 - 成功转换
TEST_F(TestClusterMonitor, NormalCase01)
{
    uint64_t flexId = 123;                           // 123 id
    uint64_t flexGroupId = 456;                      // 456 id
    std::vector<uint64_t> mNodeIds = {flexId, 789};  // 789 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();
    // 456 789 id
    instances.push_back(CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 456}));
    // 1011 1213  id
    instances.push_back(
        CreateTestJson(1011, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 1213}));

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE));
    EXPECT_EQ(instance["id"], 123);  // 123 id
    EXPECT_EQ(mNodeIds[0], 123);     // 123 id
    EXPECT_EQ(mNodeIds[1], 789);     // 789 id

    // 检查 instances 中的修改
    for (const auto &ins : instances) {
        if (ins["id"] == 789) {  // 789 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 1);
            EXPECT_EQ(ins["dynamic_info"]["peers"][0], 456);  // 456 id
        } else if (ins["id"] == 1011) {                       // 1011 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 1);
            EXPECT_EQ(ins["dynamic_info"]["peers"][0], 1213);  // 1213 id
        }
    }
}

// 测试用例：异常情况 - flexId 不存在于 mNodeIds
TEST_F(TestClusterMonitor, FlexIdNotFound)
{
    uint64_t flexId = 123;                   // 123 id
    uint64_t flexGroupId = 456;              // 456 id
    std::vector<uint64_t> mNodeIds = {789};  // 789 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();
    // 456 id 789
    instances.push_back(CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 456}));

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：异常情况 - InstancePreProcInConvertMToD 返回错误
TEST_F(TestClusterMonitor, InstancePreProcError)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    std::vector<uint64_t> mNodeIds = {flexId};
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(nlohmann::json{});

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：异常情况 - RemoveRedundantInsInFlexPeers 返回错误
TEST_F(TestClusterMonitor, RemoveRedundantInsError)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    std::vector<uint64_t> mNodeIds = {flexId};
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(
        CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 456}));  // 456 id

    ClusterNodes clusterNodes;
    // 模拟 RemoveRedundantInsInFlexPeers 返回错误
    instance["dynamic_info"].erase("peers");
    int32_t result = clusterNodes.ConvertMInstanceToD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：边界情况 - instances 为空
TEST_F(TestClusterMonitor, EmptyInstances)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    std::vector<uint64_t> mNodeIds = {flexId};
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE));
    EXPECT_EQ(instance["id"], 123);  // 123 id
    EXPECT_EQ(mNodeIds[0], 123);     // 123 id
}

// 测试用例：边界情况 - peers 为空
TEST_F(TestClusterMonitor, EmptyPeers)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    std::vector<uint64_t> mNodeIds = {flexId};
    nlohmann::json instance = CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {});
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {}));  // 789 id

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE));
    EXPECT_EQ(instance["id"], 123);  // 123 id
    EXPECT_EQ(mNodeIds[0], 123);     // 123 id

    // 检查 instances 中的修改
    for (const auto &ins : instances) {
        EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 0);
    }
}

// 测试用例：边界情况 - peers 中没有 flexId
TEST_F(TestClusterMonitor, PeersWithoutFlexId)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    std::vector<uint64_t> mNodeIds = {flexId};
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {456, 789});  // 456 789 id
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(
        CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {456, 789}));  // 456 789 id

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE));
    EXPECT_EQ(instance["id"], 123);  // 123 id
    EXPECT_EQ(mNodeIds[0], 123);     // 123 id

    // 检查 instances 中的修改
    for (const auto &ins : instances) {
        if (ins["id"] == 789) {                                 // 789 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 2);  // 2 id
            EXPECT_EQ(ins["dynamic_info"]["peers"][0], 456);    // 456 id
            EXPECT_EQ(ins["dynamic_info"]["peers"][1], 789);    // 789 id
        }
    }
}

// 测试用例：边界情况 - peers 中有多个 flexId
TEST_F(TestClusterMonitor, MultipleFlexIds)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    std::vector<uint64_t> mNodeIds = {flexId};
    nlohmann::json instance = CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                                             {flexId, flexId, 456});  // 456 id
    nlohmann::json instances = nlohmann::json::array();
    // 789 id
    instances.push_back(CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                                       {flexId, flexId, 456}));  // 456 id

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE));
    EXPECT_EQ(instance["id"], 123);  // 123 id
    EXPECT_EQ(mNodeIds[0], 123);     // 123 id

    // 检查 instances 中的修改
    for (const auto &ins : instances) {
        if (ins["id"] == 789) {  // 789 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 2);
            EXPECT_EQ(ins["dynamic_info"]["peers"][0], 123);  // 123 id
            EXPECT_EQ(ins["dynamic_info"]["peers"][1], 456);  // 456 id
        }
    }
}

// 测试用例：正常情况 - 成功转换
TEST_F(TestClusterMonitor, NormalCase)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});
    nlohmann::json instances = nlohmann::json::array();
    // 456 789 id
    instances.push_back(
        CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, 456}));
    // 1213 1011 id
    instances.push_back(
        CreateTestJson(1011, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, 1213}));

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE));

    // 检查 instances 中的修改
    for (const auto &ins : instances) {
        if (ins["id"] == 789) {  // 789 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 1);
            EXPECT_EQ(ins["dynamic_info"]["peers"][0], 456);  // 456 id
        } else if (ins["id"] == 1011) {                       // 1011 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 1);
            EXPECT_EQ(ins["dynamic_info"]["peers"][0], 1213);  // 1213 id
        }
    }
}

// 测试用例：异常情况 - instance 中缺少 id
TEST_F(TestClusterMonitor, MissingInstanceId)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    instance.erase("id");
    nlohmann::json instances = nlohmann::json::array();

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：异常情况 - instance 中 static_info 缺少 group_id
TEST_F(TestClusterMonitor, MissingGroupId)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    instance["static_info"].erase("group_id");
    nlohmann::json instances = nlohmann::json::array();

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：异常情况 - instances 中包含无效的 JSON 对象
TEST_F(TestClusterMonitor, InvalidInstanceInInstances)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(nlohmann::json{});

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：异常情况 - RemoveRedundantInsInFlexPeers 返回错误
TEST_F(TestClusterMonitor, RemoveRedundantInsError04)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(
        CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, 456}));  // 456 789 id

    ClusterNodes clusterNodes;
    // 模拟 RemoveRedundantInsInFlexPeers 返回错误
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：边界情况 - instances 为空
TEST_F(TestClusterMonitor, EmptyInstances03)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE));
}

// 测试用例：边界情况 - peers 为空
TEST_F(TestClusterMonitor, EmptyPeers02)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance = CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {});
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(
        CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {}));  // 789 id

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE));
    // 检查 instances 中的修改
    for (const auto &ins : instances) {
        if (ins["id"] == 789) {  // 789 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 0);
        }
    }
}

// 测试用例：边界情况 - peers 中没有 flexId
TEST_F(TestClusterMonitor, PeersWithoutFlexId01)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance = CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                                             {456, 789});  // 789 id 456 id
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(
        CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {456, 789}));  // 789 id 456 id

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE));

    // 检查 instances 中的修改
    for (const auto &ins : instances) {
        if (ins["id"] == 789) {                                 // 789 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 2);  // 2 size
            EXPECT_EQ(ins["dynamic_info"]["peers"][0], 456);    // 456 id
            EXPECT_EQ(ins["dynamic_info"]["peers"][1], 789);    // 789 id
        }
    }
}

// 测试用例：边界情况 - peers 中有多个 flexId
TEST_F(TestClusterMonitor, MultipleFlexIds01)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance = CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                                             {flexId, flexId, 456});  // 456 id
    nlohmann::json instances = nlohmann::json::array();
    // 456 789 id
    instances.push_back(
        CreateTestJson(789, flexGroupId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, {flexId, flexId, 456}));

    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.ConvertMInstanceToP(instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(instance["static_info"]["role"], static_cast<int>(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE));

    // 检查 instances 中的修改
    for (const auto &ins : instances) {
        if (ins["id"] == 789) {                                 // 789 id
            EXPECT_EQ(ins["dynamic_info"]["peers"].size(), 2);  // 2 size
            EXPECT_EQ(ins["dynamic_info"]["peers"][0], 123);    // 123 id
        }
    }
}
// 测试用例：异常情况 - instance 中缺少 id
TEST_F(TestClusterMonitor, MissingInstanceId04)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    instance.erase("id");
    nlohmann::json instances = nlohmann::json::array();

    std::vector<uint64_t> mNodeIds;
    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.SplitMInstanceToPAndD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：异常情况 - instance 中 static_info 缺少 group_id
TEST_F(TestClusterMonitor, MissingGroupId04)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    instance["static_info"].erase("group_id");
    nlohmann::json instances = nlohmann::json::array();

    std::vector<uint64_t> mNodeIds;
    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.SplitMInstanceToPAndD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：异常情况 - instances 中包含无效的 JSON 对象
TEST_F(TestClusterMonitor, InvalidInstanceInInstances04)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();
    instances.push_back(nlohmann::json{});

    std::vector<uint64_t> mNodeIds;
    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.SplitMInstanceToPAndD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
}

// 测试用例：边界情况 - instances 为空
TEST_F(TestClusterMonitor, EmptyInstances04)
{
    uint64_t flexId = 123;       // 123 id
    uint64_t flexGroupId = 456;  // 456 id
    nlohmann::json instance =
        CreateTestJson(flexId, flexGroupId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, {flexId, 789});  // 789 id
    nlohmann::json instances = nlohmann::json::array();

    std::vector<uint64_t> mNodeIds;
    ClusterNodes clusterNodes;
    int32_t result = clusterNodes.SplitMInstanceToPAndD(mNodeIds, instance, instances);

    EXPECT_EQ(result, -1);
    EXPECT_EQ(mNodeIds.size(), 0);
}