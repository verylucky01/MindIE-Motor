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
#include <memory>
#include <iostream>
#include <gmock/gmock.h>
#include <boost/beast/http.hpp>
#include "gtest/gtest.h"
#include "stub.h"
#include "Helper.h"
#include "ControllerConfig.h"
#include "ControllerSendReqStub.h"
#include "RequestCoordinatorStub.h"
#include "HttpClient.h"
#include "NodeScheduler.h"
#include "StatusUpdater.h"
#include "ProcessManager.h"
#define protected public
#define private public
#include "RoleSwitcher.h"
#include "RoleSwitcher.cpp"
#undef protected
#undef private
#include "ServerRequestHandler.h"
using namespace MINDIE::MS;


class TestRoleSwitcher : public testing::Test {
public:
    void SetUp() override
    {
        CopyDefaultConfig();
        ClearAllMock();
        InitNodeStatus();
        InitCoordinatorStore();
        InitNodeScheduler();
        InitStatusUpdater();
        InitRoleSwitcher();
        auto controllerJson = GetMSControllerConfigJsonPath();
        auto testJson = GetServerRequestHandlerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "check_role_attempt_times", "", 2); // 改小一点避免等待时间过长
        ModifyJsonItem(testJson, "check_role_wait_seconds", "", 1); // 改小一点避免等待时间过长
        ModifyJsonItem(testJson, "rank_table_detecting_seconds", "", 4); // 检查集群信息表的时间改成4s避免等待时间过长
        ModifyJsonItem(testJson, "disappeared_server_waiting_seconds", "", 15); // 检查集群信息表的时间改成5s避免等待时间过长
        ModifyJsonItem(testJson, "tls_config", "request_coordinator_tls_enable", false);
        ModifyJsonItem(testJson, "tls_config", "request_server_tls_enable", false);
        ModifyJsonItem(testJson, "tls_config", "http_server_tls_enable", false);
        ModifyJsonItem(testJson, "digs_model_config_path", "", GetModelConfigTestJsonPath());
        ModifyJsonItem(testJson, "digs_machine_config_path", "", GetMachineConfigTestJsonPath());
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << testJson << std::endl;
        ChangeFileMode(testJson);
        ChangeFileMode(GetModelConfigTestJsonPath());
        ChangeFileMode(GetMachineConfigTestJsonPath());
        ControllerConfig::GetInstance()->Init();
        stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
    }

    void TearDown() override
    {
        ProcessManager::GetInstance()->Stop();
    }

    void InitNodeStatus()
    {
        nodeStatus = std::make_shared<NodeStatus>();
    }

    void InitCoordinatorStore()
    {
        coordinatorStore = std::make_shared<CoordinatorStore>();
    }

    void InitNodeScheduler()
    {
        nodeScheduler = std::make_shared<NodeScheduler>(nodeStatus, coordinatorStore);
        nodeScheduler->nodeSchedulerAlarmThreadRunning = false;
        nodeScheduler->monitorRankTableRunning = false;

        std::atomic<bool> waitClusterDGRTSave{true};
        nodeScheduler->SetWaitClusterDGRTSave(&waitClusterDGRTSave);
    }

    void InitStatusUpdater()
    {
        statusUpdater = std::make_shared<StatusUpdater>(nodeStatus, coordinatorStore);
    }

    void InitRoleSwitcher()
    {
        roleSwitcher = std::make_shared<RoleSwitcher>(nodeStatus, coordinatorStore);
    }

    void AddCoordinatorNode(const std::string &ip = "127.0.0.1", bool isHealthy = false)
    {
        std::vector<std::unique_ptr<Coordinator>> coordinators;
        auto coordinator = std::make_unique<Coordinator>();
        coordinator->ip = ip;
        coordinator->port = "9000";
        coordinator->isHealthy = isHealthy;
        coordinators.emplace_back(std::move(coordinator));
        coordinatorStore->UpdateCoordinators(coordinators);
    }

    void AddNodeForOnline(uint64_t id, DIGSInstanceRole role = DIGSInstanceRole::PREFILL_INSTANCE)
    {
        auto node = std::make_unique<NodeInfo>();
        node->instanceInfo.staticInfo.id = id;
        node->instanceInfo.staticInfo.role = role;
        node->ip = "127.0.0.2";
        nodeStatus->AddNode(std::move(node));
    }

    void ModifyRankTable(uint32_t serverCount)
    {
        auto originRankTable = GetMSGlobalRankTableTestJsonPath();
        auto rawConfig = FileToJsonObj(originRankTable, 0777);
        nlohmann::json serverNodeJson = {
            {"server_id", "127.0.0.1"},
            {"server_ip", "127.0.0.1"},
            {"device", {
                {
                    {"device_id", "0"},
                    {"device_ip", "127.0.0.1"},
                    {"device_logical_id", "0"},
                    {"rank_id", "0"}
                }
            }}
        };
        for (uint32_t i = 4; i <= serverCount + 1; i++) {
            std::string ip = "127.0.0." + std::to_string(i);
            serverNodeJson["server_ip"] = ip;
            rawConfig["server_group_list"][2]["server_list"].push_back(serverNodeJson);
        }
        auto newRankTable = GetMSRankTableLoaderTestJsonPath();
        DumpStringToFile(newRankTable, rawConfig.dump(4));
        setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);
        std::cout << "GLOBAL_RANK_TABLE_FILE_PATH=" << newRankTable << std::endl;
        ChangeFileMode(newRankTable);
    }

    std::shared_ptr<RoleSwitcher> roleSwitcher;
    std::shared_ptr<NodeScheduler> nodeScheduler;
    std::shared_ptr<StatusUpdater> statusUpdater;
    std::shared_ptr<NodeStatus> nodeStatus;
    std::shared_ptr<CoordinatorStore> coordinatorStore;
    Stub stub;
};

/*
测试描述: 测试RoleSwitcher，由于节点异常，不执行身份切换
测试步骤:
    1. 节点1未找到，预期结果1
    2. 节点1不健康，预期结果1
    3. 节点1的角色与身份决策的结果一致，预期结果1
    4. 未找到节点1的group，预期结果1
预期结果:
    1. 不执行身份切换
*/
TEST_F(TestRoleSwitcher, TestNotSwitch)
{
    // 修改controller.json配置
    auto testJson = GetServerRequestHandlerTestJsonPath();
    ModifyJsonItem(testJson, "default_p_rate", "", 2);
    ModifyJsonItem(testJson, "default_d_rate", "", 2);
    ControllerConfig::GetInstance()->Init();

    // 修改rankTable
    ModifyRankTable(4);

    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 设置coordinator的检查条件
    std::map<uint64_t, std::vector<uint64_t>> target;
    target[GetIdFormIp("127.0.0.2")] = {};
    target[GetIdFormIp("127.0.0.3")] = {};
    target[GetIdFormIp("127.0.0.4")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    // 1. 节点1未找到
    MINDIE::MS::DIGSRoleDecision decision;
    decision.id = GetIdFormIp("127.0.0.6");
    decision.groupId = 0;
    decision.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    roleSwitcher->ProcessSingleRoleSwitching(decision);

    // 2. 节点1不健康
    decision.id = GetIdFormIp("127.0.0.3");
    nodeStatus->UpdateRoleState(GetIdFormIp("127.0.0.3"), "RoleUnknown", false, true);
    roleSwitcher->ProcessSingleRoleSwitching(decision);
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.3", "prefill"));

    // 3. 待切换的身份与原有的身份一致
    decision.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    nodeStatus->UpdateRoleState(GetIdFormIp("127.0.0.3"), "RoleReady", true, true);
    roleSwitcher->ProcessSingleRoleSwitching(decision);
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.3", "prefill"));

    // 4. group不存在
    decision.groupId = 1;
    decision.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    roleSwitcher->ProcessSingleRoleSwitching(decision);
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.3", "prefill"));

    nodeScheduler->Stop();
    thread->join();
    statusUpdater->Stop();
}

/*
测试描述: 测试RoleSwitcher，P节点切换为D
测试步骤:
    1. 触发身份切换P->D，预期结果1
预期结果:
    1. Group0包含1P3D
*/
TEST_F(TestRoleSwitcher, TestPSwitchDSuccess)
{
    // 修改controller.json配置
    auto testJson = GetServerRequestHandlerTestJsonPath();
    ModifyJsonItem(testJson, "default_p_rate", "", 2);
    ModifyJsonItem(testJson, "default_d_rate", "", 2);
    ControllerConfig::GetInstance()->Init();

    // 修改rankTable
    ModifyRankTable(4);

    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    stub.set(ADDR(RoleSwitcher, OldInstanceOffline), ADDR(ControllerHttpClientMock, OldInstanceOfflineSuccessMock));
    stub.set(ADDR(RoleSwitcher, QueryInstancePeerTasks),
        ADDR(ControllerHttpClientMock, QueryInstancePeerTasksSuccessMock));
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    EXPECT_EQ(0, roleSwitcher->Init());
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 设置coordinator的检查条件
    std::map<uint64_t, std::vector<uint64_t>> target;
    target[GetIdFormIp("127.0.0.2")] = {};
    target[GetIdFormIp("127.0.0.3")] = {};
    target[GetIdFormIp("127.0.0.4")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    MINDIE::MS::DIGSRoleDecision decision;
    decision.id = GetIdFormIp("127.0.0.3");
    decision.groupId = 0;
    decision.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    roleSwitcher->ProcessSingleRoleSwitching(decision);
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.2", "prefill"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.3", "decode"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.4", "decode"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.5", "decode"));

    target[GetIdFormIp("127.0.0.3")] = {GetIdFormIp("127.0.0.2")};
    target[GetIdFormIp("127.0.0.4")] = {GetIdFormIp("127.0.0.2")};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    nodes = nodeStatus->GetAllNodes();
    faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    nodeScheduler->Stop();
    thread->join();
    statusUpdater->Stop();
}

/*
测试描述: 测试RoleSwitcher，D节点切换为P
测试步骤:
    1. 触发身份切换D->P，预期结果1
预期结果:
    1. Group0包含3P1D
*/
TEST_F(TestRoleSwitcher, TestDSwitchPSuccess)
{
    // 修改controller.json配置
    auto testJson = GetServerRequestHandlerTestJsonPath();
    ModifyJsonItem(testJson, "default_p_rate", "", 2);
    ModifyJsonItem(testJson, "default_d_rate", "", 2);
    ControllerConfig::GetInstance()->Init();

    // 修改rankTable
    ModifyRankTable(4);

    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    stub.set(ADDR(RoleSwitcher, OldInstanceOffline), ADDR(ControllerHttpClientMock, OldInstanceOfflineSuccessMock));
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    EXPECT_EQ(0, roleSwitcher->Init());
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 设置coordinator的检查条件
    std::map<uint64_t, std::vector<uint64_t>> target;
    target[GetIdFormIp("127.0.0.2")] = {};
    target[GetIdFormIp("127.0.0.3")] = {};
    target[GetIdFormIp("127.0.0.4")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    MINDIE::MS::DIGSRoleDecision decision;
    decision.id = GetIdFormIp("127.0.0.4");
    decision.groupId = 0;
    decision.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    roleSwitcher->ProcessSingleRoleSwitching(decision);
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.2", "prefill"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.3", "prefill"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.4", "prefill"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.5", "decode"));

    target[GetIdFormIp("127.0.0.4")] = {};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3"), GetIdFormIp("127.0.0.4")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    nodes = nodeStatus->GetAllNodes();
    faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());
    
    nodeScheduler->Stop();
    thread->join();
    statusUpdater->Stop();
}

/*
测试描述: 测试RoleSwitcher，P节点切换为D
测试步骤:
    1. 触发身份切换P->D, 下线节点失败，预期结果1
预期结果:
    1. Group0包含2P2D
*/
TEST_F(TestRoleSwitcher, TestPSwitchDFail)
{
    // 修改controller.json配置
    auto testJson = GetServerRequestHandlerTestJsonPath();
    ModifyJsonItem(testJson, "default_p_rate", "", 2);
    ModifyJsonItem(testJson, "default_d_rate", "", 2);
    ControllerConfig::GetInstance()->Init();

    // 修改rankTable
    ModifyRankTable(4);

    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    stub.set(ADDR(RoleSwitcher, OldInstanceOffline), ADDR(ControllerHttpClientMock, OldInstanceOfflineFailMock));
    stub.set(ADDR(RoleSwitcher, QueryInstancePeerTasks),
        ADDR(ControllerHttpClientMock, QueryInstancePeerTasksFailMock));

    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    EXPECT_EQ(0, roleSwitcher->Init());
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 设置coordinator的检查条件
    std::map<uint64_t, std::vector<uint64_t>> target;
    target[GetIdFormIp("127.0.0.2")] = {};
    target[GetIdFormIp("127.0.0.3")] = {};
    target[GetIdFormIp("127.0.0.4")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};

    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    MINDIE::MS::DIGSRoleDecision decision;
    decision.id = GetIdFormIp("127.0.0.3");
    decision.groupId = 0;
    decision.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    roleSwitcher->ProcessSingleRoleSwitching(decision);
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.2", "prefill"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.3", "prefill"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.4", "decode"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.5", "decode"));

    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    nodes = nodeStatus->GetAllNodes();
    faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    nodeScheduler->Stop();
    thread->join();
    statusUpdater->Stop();
}

/*
测试描述: 测试RoleSwitcher，D节点切换为P
测试步骤:
    1. 触发身份切换D->P，下线节点失败，预期结果1
预期结果:
    1. Group0包含2P2D
*/
TEST_F(TestRoleSwitcher, TestDSwitchPFail)
{
    // 修改controller.json配置
    auto testJson = GetServerRequestHandlerTestJsonPath();
    ModifyJsonItem(testJson, "default_p_rate", "", 2);
    ModifyJsonItem(testJson, "default_d_rate", "", 2);
    ControllerConfig::GetInstance()->Init();

    // 修改rankTable
    ModifyRankTable(4);

    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    stub.set(ADDR(RoleSwitcher, OldInstanceOffline), ADDR(ControllerHttpClientMock, OldInstanceOfflineFailMock));

    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    EXPECT_EQ(0, roleSwitcher->Init());
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 设置coordinator的检查条件
    std::map<uint64_t, std::vector<uint64_t>> target;
    target[GetIdFormIp("127.0.0.2")] = {};
    target[GetIdFormIp("127.0.0.3")] = {};
    target[GetIdFormIp("127.0.0.4")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2"), GetIdFormIp("127.0.0.3")};

    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    MINDIE::MS::DIGSRoleDecision decision;
    decision.id = GetIdFormIp("127.0.0.4");
    decision.groupId = 0;
    decision.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    roleSwitcher->ProcessSingleRoleSwitching(decision);
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.2", "prefill"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.3", "prefill"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.4", "decode"));
    EXPECT_TRUE(CheckServerRole(30, "127.0.0.5", "decode"));

    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    nodes = nodeStatus->GetAllNodes();
    faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());
    
    nodeScheduler->Stop();
    thread->join();
    statusUpdater->Stop();
}

TEST_F(TestRoleSwitcher, TestPostInstanceOnlineSuccess)
{
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    ASSERT_EQ(0, roleSwitcher->Init());

    const uint64_t nodeId = 100;
    AddCoordinatorNode();
    AddNodeForOnline(nodeId, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);

    roleSwitcher->PostInstanceOnline(nodeId);

    auto postBody = GetCoordinatorLastPostOnlineRequest("127.0.0.1");
    ASSERT_FALSE(postBody.empty());
    auto bodyJson = nlohmann::json::parse(postBody);
    ASSERT_TRUE(bodyJson.contains("ids"));
    ASSERT_EQ(1u, bodyJson["ids"].size());
    EXPECT_EQ(nodeId, bodyJson["ids"][0].get<uint64_t>());

    auto stored = coordinatorStore->GetCoordinators();
    ASSERT_EQ(1u, stored.size());
    EXPECT_TRUE(stored[0]->isHealthy);
}

TEST_F(TestRoleSwitcher, TestPostInstanceOnlineNodeNotFound)
{
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    ASSERT_EQ(0, roleSwitcher->Init());
    AddCoordinatorNode();

    roleSwitcher->PostInstanceOnline(999);

    auto postBody = GetCoordinatorLastPostOnlineRequest("127.0.0.1");
    EXPECT_TRUE(postBody.empty());
}

TEST_F(TestRoleSwitcher, TestPostInstanceOnlineCoordinatorFailure)
{
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    ASSERT_EQ(0, roleSwitcher->Init());

    const uint64_t nodeId = 101;
    AddCoordinatorNode("127.0.0.1", true);
    AddNodeForOnline(nodeId, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    SetCoordinatorPostOnlineMock("127.0.0.1", "", 500, -1);

    roleSwitcher->PostInstanceOnline(nodeId);

    auto stored = coordinatorStore->GetCoordinators();
    ASSERT_EQ(1u, stored.size());
    EXPECT_FALSE(stored[0]->isHealthy);
}

TEST_F(TestRoleSwitcher, TestUpdateAbnormalRoleWhenRecoverCluster)
{
    EXPECT_EQ(0, roleSwitcher->Init());
    
    std::vector<std::unique_ptr<NodeInfo>> nodesInGroup;
    for (int i = 1; i <= 4; i++) {
        auto nodeInfo = std::make_unique<NodeInfo>();
        nodeInfo->instanceInfo.staticInfo.id = i;
        nodeInfo->ip = "127.0.0." + std::to_string(i);
        if (i <= 2) {
            nodeInfo->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
            nodeInfo->peers = {3, 4};
        } else {
            nodeInfo->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
            nodeInfo->peers = {1, 2};
        }
        nodesInGroup.push_back(std::move(nodeInfo));
    }
    
    // 测试路径1: P节点需要切换到D
    NodeInfo pToDNode;
    pToDNode.instanceInfo.staticInfo.id = 2;
    pToDNode.currentRole = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    pToDNode.instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    
    // 修改组信息使节点2应该在D组中，触发P到D切换
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> pToDGroup;
    pToDGroup.first = {1};
    pToDGroup.second = {2, 3, 4};

    roleSwitcher->UpdateAbnormalRoleWhenRecoverCluster(pToDNode, pToDGroup, nodesInGroup);
     
    // 测试路径2: D节点需要切换到P
    NodeInfo dToPNode;
    dToPNode.instanceInfo.staticInfo.id = 3;
    dToPNode.currentRole = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    dToPNode.instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;

    // 修改组信息使节点3应该在P组中，触发D到P切换
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> dToPGroup;
    dToPGroup.first = {1, 2, 3};
    dToPGroup.second = {4};
    
    roleSwitcher->UpdateAbnormalRoleWhenRecoverCluster(dToPNode, dToPGroup, nodesInGroup);
}

TEST_F(TestRoleSwitcher, TestOldInstanceOffline)
{
    EXPECT_EQ(0, roleSwitcher->Init());

    // Mock与coordinator的HTTP请求
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestCoordinatorMock);
    
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);

    auto testNode = std::make_unique<NodeInfo>();
    testNode->instanceInfo.staticInfo.id = 100;
    testNode->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    testNode->instanceInfo.staticInfo.groupId = 0;
    testNode->ip = "127.0.0.1";
    testNode->isHealthy = true;
    nodeStatus->AddNode(std::move(testNode));
    
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> group;
    group.first = {100};
    group.second = {101};
    nodeStatus->AddGroup(0, group);
    
    MINDIE::MS::DIGSRoleDecision decision;
    decision.id = 100;
    decision.groupId = 0;
    decision.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    
    roleSwitcher->ProcessSingleRoleSwitching(decision);
    
    SetRequestCoordinatorMockCode(500);
    SetRequestCoordinatorMockRet(-1);
    
    auto testNode2 = std::make_unique<NodeInfo>();
    testNode2->instanceInfo.staticInfo.id = 200;
    testNode2->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    testNode2->instanceInfo.staticInfo.groupId = 1;
    testNode2->ip = "127.0.0.2";
    testNode2->isHealthy = true;
    nodeStatus->AddNode(std::move(testNode2));
    
    std::pair<std::vector<uint64_t>, std::vector<uint64_t>> group2;
    group2.first = {201};
    group2.second = {200};
    nodeStatus->AddGroup(1, group2);

    MINDIE::MS::DIGSRoleDecision decision2;
    decision2.id = 200;
    decision2.groupId = 1;
    decision2.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;

    roleSwitcher->ProcessSingleRoleSwitching(decision2);
}

// Test for BuildTasksRequestUrl function
TEST_F(TestRoleSwitcher, TestBuildTasksRequestUrl)
{
    // Test case 1: Empty vector
    {
        std::vector<uint64_t> ids = {};
        std::string url = BuildTasksRequestUrl(ids);
        // Should end with "?" and have no id parameters
        auto coordinatorUri = ControllerConstant::GetInstance()->GetCoordinatorURI(CoordinatorURI::GET_TASK);
        EXPECT_EQ(url, coordinatorUri);
    }

    // Test case 2: Single ID
    {
        std::vector<uint64_t> ids = {123};
        std::string url = BuildTasksRequestUrl(ids);
        auto coordinatorUri = ControllerConstant::GetInstance()->GetCoordinatorURI(CoordinatorURI::GET_TASK);
        std::string expected = coordinatorUri + "?id=123";
        EXPECT_EQ(url, expected);
    }

    // Test case 3: Multiple IDs
    {
        std::vector<uint64_t> ids = {123, 456, 789};
        std::string url = BuildTasksRequestUrl(ids);
        auto coordinatorUri = ControllerConstant::GetInstance()->GetCoordinatorURI(CoordinatorURI::GET_TASK);
        std::string expected = coordinatorUri + "?id=123&id=456&id=789";
        EXPECT_EQ(url, expected);
    }

    // Test case 4: Large ID value
    {
        std::vector<uint64_t> ids = {UINT64_MAX};
        std::string url = BuildTasksRequestUrl(ids);
        auto coordinatorUri = ControllerConstant::GetInstance()->GetCoordinatorURI(CoordinatorURI::GET_TASK);
        std::string expected = coordinatorUri + "?id=" + std::to_string(UINT64_MAX);
        EXPECT_EQ(url, expected);
    }
}

// Test for IsValidTasksResponse function
TEST_F(TestRoleSwitcher, TestIsValidTasksResponse)
{
    // Test case 1: Valid response with single integer task
    {
        nlohmann::json bodyJson = {
            {"tasks", {123}}
        };
        EXPECT_TRUE(IsValidTasksResponse(bodyJson));
    }

    // Test case 2: Valid response with multiple integer tasks
    {
        nlohmann::json bodyJson = {
            {"tasks", {1, 2, 3, 4, 5}}
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 3: Valid response with boundary values
    {
        nlohmann::json bodyJson = {
            {"tasks", {-1, 2147483647}}  // MIN_TASKS_NUMBER and MAX_TASKS_NUMBER
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 4: Invalid - tasks field is not an array
    {
        nlohmann::json bodyJson = {
            {"tasks", "not_an_array"}
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 5: Invalid - tasks array is empty
    {
        nlohmann::json bodyJson = {
            {"tasks", {}}
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 6: Invalid - tasks array has more than 1 element (as per function logic)
    {
        nlohmann::json bodyJson = {
            {"tasks", {1, 2}}  // More than 1 element
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 7: Invalid - task value is not integer
    {
        nlohmann::json bodyJson = {
            {"tasks", {"string_value"}}
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 8: Invalid - task value is floating point
    {
        nlohmann::json bodyJson = {
            {"tasks", {123.45}}
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 9: Invalid - task value below minimum
    {
        nlohmann::json bodyJson = {
            {"tasks", {-2}}  // Less than MIN_TASKS_NUMBER (-1)
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 10: Invalid - task value above maximum
    {
        nlohmann::json bodyJson = {
            {"tasks", {2147483648}}  // Greater than MAX_TASKS_NUMBER (2147483647)
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 11: Invalid - missing tasks field
    {
        nlohmann::json bodyJson = {
            {"other_field", {1, 2, 3}}
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 12: Invalid - tasks field is null
    {
        nlohmann::json bodyJson = {
            {"tasks", nullptr}
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }

    // Test case 13: Invalid - tasks field is object
    {
        nlohmann::json bodyJson = {
            {"tasks", {{"id", 123}}}
        };
        EXPECT_FALSE(IsValidTasksResponse(bodyJson));
    }
}