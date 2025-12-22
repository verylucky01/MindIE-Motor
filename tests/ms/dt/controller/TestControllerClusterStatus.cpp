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
#include <cstdint>
#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <fstream>
#include <cstdlib> // #include <stdlib.h>
#include <string>
#include <memory>
#include <pthread.h>
#include <nlohmann/json.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include "gtest/gtest.h"
#include "ControllerConfig.h"
#include "Helper.h"
#include "CoordinatorStore.h"
#include "NodeStatus.h"
#include "ClusterStatusWriter.h"
using namespace MINDIE::MS;
class TestControllerClusterStatus : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }

    void SetUp()
    {
        CopyDefaultConfig();
        auto controllerJson = GetMSControllerTestJsonPath();
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
        // std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
    }
};

void ThreadFuncIns(std::shared_ptr<CoordinatorStore> obj)
{
    for (int i = 0; i < 1000; ++i) {
        auto tempCoor = std::make_unique<Coordinator>();
        std::vector<std::unique_ptr<Coordinator>> tempVec;
        tempVec.push_back(std::move(tempCoor));
        obj->UpdateCoordinators(tempVec);
        std::cout << "FuncIns, i: " << (i + 1) << std::endl;
    }
}
void ThreadFuncGet(std::shared_ptr<CoordinatorStore> obj)
{
    for (int i = 0; i < 1000; ++i) {
        auto tempVec = obj->GetCoordinators();
        std::cout << "FuncGet, i: " << (i + 1) << "\tsize: " << tempVec.size() << std::endl;
    }
}

/*
测试描述: CoordinatorStore，coordinate节点信息的增删改查。正例。
测试步骤:
    1. 构造两个coordinate节点，以及CoordinatorStore实例
    2. 测试UpdateCoordinators()
    3. 测试UpdateCoordinatorStatus()
    4. 测试GetCoordinators()，预期结果1
    5. 测试UpdateCoordinators()，预期结果2
    6. 测试UpdateCoordinators()的参数是空的vector
    7. 测试UpdateCoordinators()的参数是空的vector
    8. 测试多线程交叉读写，用到GetCoordinators()和UpdateCoordinators()
预期结果:
    1. 得到的vector只有一个成员，并且查看其内容，与修改的一致
    2. 得到的vector只有一个成员，并且查看其内容，是新传入的vector内的节点
*/
TEST_F(TestControllerClusterStatus, TestCoordinatorStoreTC01)
{
    ControllerConfig::GetInstance()->Init();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto node1 = std::make_unique<Coordinator>();
    auto node2 = std::make_unique<Coordinator>();
    std::string ip01 = "127.0.0.1";
    std::string ip02 = "127.0.0.2";
    node1->ip = ip01;
    node2->ip = ip02;
    node1->port = "1031";
    node2->port = "1032";
    node1->isHealthy = false;
    node2->isHealthy = false;
    // 2
    std::vector<std::unique_ptr<Coordinator>> coordinatorVec1;
    coordinatorVec1.push_back(std::move(node1)); // 记得用move
    coordinatorStore->UpdateCoordinators(coordinatorVec1);
    // 3
    coordinatorStore->UpdateCoordinatorStatus(ip01, true);
    // 4
    auto vec = coordinatorStore->GetCoordinators();
    for (auto &coordinator : vec) {
        std::cout << std::boolalpha;
        std::cout << "ip: " << coordinator->ip << std::endl;
        std::cout << "port: " << coordinator->port << std::endl;
        std::cout << "isHealthy: " << coordinator->isHealthy << std::endl;
        std::cout << std::noboolalpha;
    }
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0]->isHealthy, true);
    // 5
    std::vector<std::unique_ptr<Coordinator>> coordinatorVec2;
    coordinatorVec2.push_back(std::move(node2)); // 记得用move
    coordinatorStore->UpdateCoordinators(coordinatorVec2);
    vec = coordinatorStore->GetCoordinators();
    for (auto &coordinator : vec) {
        std::cout << std::boolalpha;
        std::cout << "ip: " << coordinator->ip << std::endl;
        std::cout << "port: " << coordinator->port << std::endl;
        std::cout << "isHealthy: " << coordinator->isHealthy << std::endl;
        std::cout << std::noboolalpha;
    }
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0]->ip, ip02);
    EXPECT_EQ(vec[0]->port, "1032");
    EXPECT_EQ(vec[0]->isHealthy, false);
    // 6
    std::vector<std::unique_ptr<Coordinator>> coordinatorVec3;
    coordinatorStore->UpdateCoordinators(coordinatorVec3);
    // 7
    coordinatorStore->UpdateCoordinators(coordinatorVec3);
    // 8
    std::thread threadIns(ThreadFuncIns, std::ref(coordinatorStore));
    std::thread threadGet(ThreadFuncGet, std::ref(coordinatorStore));

    threadIns.join();
    threadGet.join();
}

void SetNodeInfo(std::unique_ptr<NodeInfo>& nodeInfo1, std::unique_ptr<NodeInfo>& nodeInfo2)
{
    nodeInfo1->ip = "172.17.0.1";
    nodeInfo1->port = "8080";
    nodeInfo1->isHealthy = true;
    nodeInfo1->isInitialized = true;
    nodeInfo1->roleState = "RoleReady";
    nodeInfo1->modelName = "model1";
    nodeInfo1->peers = { 33558956 };
    nodeInfo1->activePeers = { 33558956 };
    nodeInfo1->instanceInfo.staticInfo.id = 16781740;
    nodeInfo1->instanceInfo.staticInfo.groupId = 1;
    nodeInfo1->instanceInfo.staticInfo.maxSeqLen = 2048;
    nodeInfo1->instanceInfo.staticInfo.maxOutputLen = 512;
    nodeInfo1->instanceInfo.staticInfo.totalSlotsNum = 200;
    nodeInfo1->instanceInfo.staticInfo.totalBlockNum = 1024;
    nodeInfo1->instanceInfo.staticInfo.blockSize = 128;
    nodeInfo1->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER;
    nodeInfo1->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;

    nodeInfo2->ip = "172.17.0.2";
    nodeInfo2->port = "8081";
    nodeInfo2->isHealthy = true;
    nodeInfo2->isInitialized = false;
    nodeInfo2->roleState = "RoleReady";
    nodeInfo2->modelName = "model2";
    nodeInfo2->peers = { 16781740 };
    nodeInfo2->activePeers = { 16781740 };
    nodeInfo2->instanceInfo.staticInfo.id = 33558956;
    nodeInfo2->instanceInfo.staticInfo.groupId = 2;
    nodeInfo2->instanceInfo.staticInfo.maxSeqLen = 2048;
    nodeInfo2->instanceInfo.staticInfo.maxOutputLen = 512;
    nodeInfo2->instanceInfo.staticInfo.totalSlotsNum = 300;
    nodeInfo2->instanceInfo.staticInfo.totalBlockNum = 2048;
    nodeInfo2->instanceInfo.staticInfo.blockSize = 256;
    nodeInfo2->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER;
    nodeInfo2->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
}

NodeInfo GetNodeInfoInstance(std::unique_ptr<NodeInfo>& nodeInfo)
{
    NodeInfo node;
    node.instanceInfo.staticInfo.role = nodeInfo->instanceInfo.staticInfo.role;
    node.instanceInfo.staticInfo.label = nodeInfo->instanceInfo.staticInfo.label;
    node.roleState = nodeInfo->roleState;
    node.peers = nodeInfo->peers;
    node.isHealthy = nodeInfo->isHealthy;
    node.isInitialized = nodeInfo->isInitialized;
    return node;
}

void ThreadFuncIns02(std::shared_ptr<NodeStatus> obj)
{
    for (int i = 0; i < 100; ++i) {
        auto node = std::make_unique<NodeInfo>();
        node->instanceInfo.staticInfo.id = i;
        obj->AddNode(std::move(node));
        std::cout << "FuncIns02, i: " << (i + 1) << std::endl;
    }
}
void ThreadFuncGet02(std::shared_ptr<NodeStatus> obj)
{
    for (int i = 0; i < 100; ++i) {
        auto tempMap = obj->GetAllNodes();
        std::cout << "FuncGet, i: " << (i + 1) << "\tsize: " << tempMap.size() << std::endl;
    }
}

/*
测试描述: 测试NodeStatus.cpp里定义的函数，只测试源码中使用次数超过11的函数。正例。
测试步骤:
    1. 构造两个节点，以及NodeStatus实例
    2. 测试AddNodes()
    3. 测试AddNode()
    4. 测试GetNode()
    5. 测试UpdateNode()
    6. 测试GetAllNodes()
    7. 测试RemoveNode()
    8. 测试UpdateRoleState()
    9. 测试UpdateRoleStateAndPeers()
    10. 测试GetIpForAllTypeNodes()
    11. 测试DetectNodeChanges()
    12. 测试IsPostRoleNeeded()
    13. 测试多线程交叉读写，
预期结果:
    1. 得到的vector只有一个成员，并且查看其内容，与修改的一致
    2. 得到的vector只有一个成员，并且查看其内容，是新传入的vector内的节点
*/
TEST_F(TestControllerClusterStatus, TestNodeStatusTC01)
{
    ControllerConfig::GetInstance()->Init();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    auto nodeInfo2 = std::make_unique<NodeInfo>();
    SetNodeInfo(nodeInfo1, nodeInfo2);
    // 2
    std::cout << "2:" << std::endl;
    std::vector<std::unique_ptr<NodeInfo>> nodeVec01;
    nodeVec01.push_back(std::move(nodeInfo1));
    nodeStatus->AddNodes(nodeVec01);
    // 3
    std::cout << "3:" << std::endl;
    nodeStatus->AddNode(std::move(nodeInfo2));
    // 4
    std::cout << "4:" << std::endl;
    {
        // std::move之后，就变成nullPtr了
        if (nodeInfo1 == nullptr) {
            std::cout << "nodeInfo1 == nullptr" << std::endl;
        } else {
            std::cout << "nodeInfo1 != nullptr" << std::endl;
        }
    }
    nodeInfo1 = nodeStatus->GetNode(16781740);  // id = 16781740
    if (nodeInfo1) {
        printf("nodeInfo1->roleState:%s\n", nodeInfo1->roleState.c_str());
    }
    // 5
    std::cout << "5:" << std::endl;
    nodeInfo1->roleState = "initializing";
    nodeInfo1->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    nodeInfo1->isHealthy = false;
    auto node = GetNodeInfoInstance(nodeInfo1); // 注意这里不需要move
    printf("node.roleState:%s\n", node.roleState.c_str());
    nodeStatus->UpdateNode(16781740, node);
    nodeInfo1 = nodeStatus->GetNode(16781740);
    if (nodeInfo1 != nullptr) {
        std::cout << "GetNode successfully!" << std::endl;
        EXPECT_EQ(nodeInfo1->isHealthy, false);
        EXPECT_EQ(nodeInfo1->roleState, "initializing");
        EXPECT_EQ(nodeInfo1->instanceInfo.staticInfo.role, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
        std::cout << "verify done!" << std::endl;
    } else {
        std::cout << "GetNode Failed!" << std::endl;
    }
    // 6
    std::cout << "6:" << std::endl;
    auto tempMap = nodeStatus->GetAllNodes();
    EXPECT_EQ(tempMap.size(), 2);
    for (auto& node : std::as_const(tempMap)) {
        std::cout << "id: " << node.first << std::endl;
    }
    // 7
    std::cout << "7:" << std::endl;
    nodeStatus->RemoveNode(123456);
    nodeStatus->RemoveNode(33558956);
    tempMap = nodeStatus->GetAllNodes();
    EXPECT_EQ(tempMap.size(), 1);
    // 8
    std::cout << "8:" << std::endl;
    std::string tempStr = "running";
    nodeStatus->UpdateRoleState(16781740, tempStr, true, false);
    nodeInfo1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(nodeInfo1->roleState, tempStr);
    EXPECT_EQ(nodeInfo1->isHealthy, true);
    EXPECT_EQ(nodeInfo1->isInitialized, false);
    // 9
    std::cout << "9:" << std::endl;
    tempStr = "new roleState";
    std::vector<uint64_t> tempVec = { 456123 };
    uint64_t unkonwGroupId = 0; // groupId = 0
    // 16781740 peers id
    nodeStatus->UpdateRoleStateAndPeers(unkonwGroupId, 16781740, tempStr, tempVec);
    nodeInfo1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(nodeInfo1->roleState, tempStr);
    EXPECT_EQ(nodeInfo1->peers, tempVec);
    // 10
    std::cout << "10:" << std::endl;
    tempStr = nodeStatus->GetIpForAllTypeNodes(16781740);  // 节点id = 16781740
    EXPECT_EQ(tempStr, "172.17.0.1");
    // 11
    std::cout << "11:" << std::endl;
    tempMap = nodeStatus->GetAllNodes();
    std::vector<std::unique_ptr<NodeInfo>> tempVec02;
    for (auto& node : tempMap) {
        tempVec02.push_back(std::move(node.second));
    }
    auto changes = nodeStatus->DetectNodeChanges(tempVec02);
    std::cout << "newIDs:" << std::endl;
    for (auto& id : changes.newIDs) {
        std::cout << id << std::endl;
    }
    std::cout << "removedIDs:" << std::endl;
    for (auto& id : changes.removedIDs) {
        std::cout << id << std::endl;
    }
    std::cout << "reappearIDs:" << std::endl;
    for (auto& id : changes.reappearIDs) {
        std::cout << id << std::endl;
    }
    // 12
    std::cout << "12:" << std::endl;
    bool ret = nodeStatus->IsPostRoleNeeded(16781740);
    std::cout << std::boolalpha;
    std::cout << "IsPostRoleNeeded:" << ret << std::endl;
    std::cout << std::noboolalpha;
    // 13
    std::cout << "13:" << std::endl;
    std::thread threadIns(ThreadFuncIns02, std::ref(nodeStatus));
    std::thread threadGet(ThreadFuncGet02, std::ref(nodeStatus));

    threadIns.join();
    threadGet.join();
}

/*
测试描述: 测试NodeStatus.cpp里定义的函数，只测试源码中使用次数超过11的函数。正例。
测试步骤:
    1. 构造2个节点，以及NodeStatus实例
    2. 节点1为Unknown并且有role，测试IsPostRoleNeeded()，预期结果1
预期结果:
    1. 节点不支持发身份
*/
TEST_F(TestControllerClusterStatus, TestIsPostRoleNeeded)
{
    ControllerConfig::GetInstance()->Init();

    auto nodeStatus = std::make_shared<NodeStatus>();
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    auto nodeInfo2 = std::make_unique<NodeInfo>();
    SetNodeInfo(nodeInfo1, nodeInfo2);
    nodeStatus->AddNode(std::move(nodeInfo1));
    nodeStatus->AddNode(std::move(nodeInfo2));
    MINDIE::MS::DIGSInstanceDynamicInfo info;
    std::vector<uint64_t> peers;
    std::string str = ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN);
    nodeStatus->UpdateNodeDynamicStatus(16781740, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE, // 节点id 16781740
                                        str, info, peers);
    EXPECT_TRUE(nodeStatus->IsPostRoleNeeded(16781740)); // 节点id 16781740
}

/*
测试描述: ClusterStatusWriter.cpp里定义的函数。正例。
测试步骤:
    1. 构造NodeStatus实例，CoordinatorStore实例，填入数据
    2. 测试ClusterStatusWriter::GetInstance()->Init，会执行cpp里定义的所有函数，预期结果1
预期结果:
    1. 返回值=0
*/
TEST_F(TestControllerClusterStatus, TestClusterStatusWriterTC01)
{
    ControllerConfig::GetInstance()->Init();

    auto nodeStatus = std::make_shared<NodeStatus>();
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    auto nodeInfo2 = std::make_unique<NodeInfo>();
    SetNodeInfo(nodeInfo1, nodeInfo2);
    nodeStatus->AddNode(std::move(nodeInfo1));
    nodeStatus->AddNode(std::move(nodeInfo2));

    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto node1 = std::make_unique<Coordinator>();
    auto node2 = std::make_unique<Coordinator>();
    std::string ip01 = "127.0.0.1";
    std::string ip02 = "127.0.0.2";
    node1->ip = ip01;
    node2->ip = ip02;
    node1->port = "1031";
    node2->port = "1032";
    node1->isHealthy = false;
    node2->isHealthy = false;
    std::vector<std::unique_ptr<Coordinator>> coordinatorVec1;
    coordinatorVec1.push_back(std::move(node1));
    coordinatorVec1.push_back(std::move(node2));
    coordinatorStore->UpdateCoordinators(coordinatorVec1);

    // 2
    std::cout << "2:" << std::endl;
    auto ret = ClusterStatusWriter::GetInstance()->Init(nodeStatus, coordinatorStore);
    EXPECT_EQ(ret, 0);

    std::this_thread::sleep_for(std::chrono::seconds(2));

    ClusterStatusWriter::GetInstance()->Stop();
}

/*
测试描述: IsIgnoredInPDSeparate，P节点为Switching状态
测试步骤:
    1. 构造NodeStatus实例，填入数据
    2. 节点1为P节点，设置为切换中，判断是否需要被过滤，预期结果0
预期结果:
    1. 返回值为false
*/
TEST_F(TestControllerClusterStatus, TestPIgnored)
{
    ControllerConfig::GetInstance()->Init();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    auto nodeInfo2 = std::make_unique<NodeInfo>();
    SetNodeInfo(nodeInfo1, nodeInfo2);
    nodeInfo1->currentRole = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    nodeInfo1->roleState = "RoleSwitching";
    nodeStatus->AddNode(std::move(nodeInfo1));
    nodeStatus->AddNode(std::move(nodeInfo2));
    const uint64_t nodeId = 16781740; // 节点1的id
    EXPECT_FALSE(nodeStatus->IsIgnoredInPDSeparate(nodeId)); // 16781740表示节点1的id
}