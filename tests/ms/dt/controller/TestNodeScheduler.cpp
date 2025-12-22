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
#include <vector>
#include <memory>
#include <iostream>
#include <mutex>
#include "gtest/gtest.h"
#include "stub.h"
#include "gmock/gmock.h"
#include "Helper.h"
#include "ControllerSendReqStub.h"
#include "ControllerConfig.h"
#include "NodeScheduler.h"
#include "StatusUpdater.h"
#include "ProcessManager.h"
#include "FaultManager.h"
#include "ServerRequestHandler.h"

using namespace MINDIE::MS;

constexpr uint32_t TIMEOUT = 30; // seconds
class TestNodeScheduler : public testing::Test {
public:
    static int32_t HttpClientInitFail(HttpClient* obj, const std::string& host, const std::string& port,
        const MINDIE::MS::TlsItems& tls_items, bool isHttps = false)
    {
        return -1; // Simulate initialization failure
    }

    void SetUp() override
    {
        CopyDefaultConfig();
        ClearAllMock();
        InitNodeStatus();
        InitCoordinatorStore();
        InitNodeScheduler();
        InitStatusUpdater();
        InitFaultManager();
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
        std::cout << "Start init controller config." << testJson << std::endl;
        ControllerConfig::GetInstance()->Init();
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
        FaultManager::GetInstance()->isNeedWaitNpuProcessExit = false;
    }

    void InitFaultManager()
    {
        auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
        FaultManager::GetInstance()->Init(nodeStatus, deployMode);
        FaultManager::GetInstance()->isNeedWaitNpuProcessExit = false;
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
    std::shared_ptr<NodeScheduler> nodeScheduler;
    std::shared_ptr<StatusUpdater> statusUpdater;
    std::shared_ptr<NodeStatus> nodeStatus;
    std::shared_ptr<CoordinatorStore> coordinatorStore;
};

/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，1P3D
测试步骤:
    1. 修改rankTable
    2. 初始化NodeScheduler
    3. NodeScheduler完成PD身份初始化，预期结果1
预期结果:
    1. coordinator的实例总量为4，group0包含1P3D
*/
TEST_F(TestNodeScheduler, Test1P3DInit)
{
    // 修改rankTable
    ModifyRankTable(4);

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));

    // 执行函数
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 检查预期结果
    std::map<uint64_t, std::vector<uint64_t>> target;
    target[GetIdFormIp("127.0.0.2")] = {};
    target[GetIdFormIp("127.0.0.3")] = {GetIdFormIp("127.0.0.2")};
    target[GetIdFormIp("127.0.0.4")] = {GetIdFormIp("127.0.0.2")};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());

    nodeScheduler->Stop();
    thread->join();
    statusUpdater->Stop();
}

/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，2P2D，D节点全部失败
测试步骤:
    1. 修改controller.json
    2. 修改rankTable
    3. Server打桩，设置27.0.0.4、127.0.0.5为unknown状态
    4. NodeScheduler完成PD身份初始化，预期结果1
预期结果:
    1. coordinator的实例总量为0，4台节点都为不可用节点
*/
TEST_F(TestNodeScheduler, Test2P2DAllDInitFailed)
{
    // 修改controller.json配置
    auto testJson = GetServerRequestHandlerTestJsonPath();
    ModifyJsonItem(testJson, "default_p_rate", "", 2);
    ModifyJsonItem(testJson, "default_d_rate", "", 2);
    ControllerConfig::GetInstance()->Init();

    // 修改rankTable
    ModifyRankTable(4);

    // Server打桩
    std::vector<std::string> unknownInstance {"127.0.0.4", "127.0.0.5"};
    ServerMock unknownServerMock;
    unknownServerMock.roleState = RoleState::UNKNOWN;
    for (auto &ip : unknownInstance) {
        AddServerMock(ip, unknownServerMock);
    }

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));

    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 检查预期结果
    EXPECT_TRUE(CheckOnlineInstanceEmpty(30));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(0, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    nodeScheduler->Stop();
    thread->join();
    statusUpdater->Stop();
}


/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，2P2D，P节点全部失败
测试步骤:
    1. 修改controller.json
    2. 修改rankTable
    3. Server打桩，设置127.0.0.2、127.0.0.3为unknown状态
    4. NodeScheduler完成PD身份初始化，预期结果1
预期结果:
    1. coordinator的实例总量为0，4台节点都为不可用节点
*/
TEST_F(TestNodeScheduler, Test2P2DAllPInitFailed)
{
    // 修改controller.json配置
    auto testJson = GetServerRequestHandlerTestJsonPath();
    ModifyJsonItem(testJson, "default_p_rate", "", 2);
    ModifyJsonItem(testJson, "default_d_rate", "", 2);
    ControllerConfig::GetInstance()->Init();

    // 修改rankTable
    ModifyRankTable(4);

    // Server打桩
    std::vector<std::string> unknownInstance {"127.0.0.2", "127.0.0.3"};
    ServerMock unknownServerMock;
    unknownServerMock.roleState = RoleState::UNKNOWN;
    for (auto &ip : unknownInstance) {
        AddServerMock(ip, unknownServerMock);
    }

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));

    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 检查预期结果
    EXPECT_TRUE(CheckOnlineInstanceEmpty(30));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(0, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    nodeScheduler->Stop();
    statusUpdater->Stop();
    thread->join();
    ProcessManager::GetInstance()->Stop();
}

/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，2P2D，D节点全部为切换状态后恢复
测试步骤:
    1. 修改controller.json
    2. 修改rankTable
    3. Server打桩，设置127.0.0.4、127.0.0.5为switching状态
    4. NodeScheduler完成PD身份初始化，预期结果1
预期结果:
    1. coordinator的实例总量为4，4台节点都可用
*/
TEST_F(TestNodeScheduler, Test2P2DAllDSwitchingSuccess)
{
    // 修改controller.json配置
    auto testJson = GetServerRequestHandlerTestJsonPath();
    ModifyJsonItem(testJson, "default_p_rate", "", 2);
    ModifyJsonItem(testJson, "default_d_rate", "", 2);
    ControllerConfig::GetInstance()->Init();

    // 修改rankTable
    ModifyRankTable(4);

    // Server打桩
    std::vector<std::string> unknownInstance {"127.0.0.4", "127.0.0.5"};
    ServerMock unknownServerMock;
    unknownServerMock.roleState = RoleState::SWITCHING;
    for (auto &ip : unknownInstance) {
        AddServerMock(ip, unknownServerMock);
    }

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
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
    target[GetIdFormIp("127.0.0.4")] = {GetIdFormIp("127.0.0.2")};
    target[GetIdFormIp("127.0.0.5")] = {GetIdFormIp("127.0.0.2")};

    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    EXPECT_EQ(4, nodes.size()); // 4是可用节点数量

    nodeScheduler->Stop();
    statusUpdater->Stop();
    thread->join();
}

/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，1P1D，删除1D
测试步骤:
    1. NodeScheduler完成PD身份初始化，预期结果1
    2. 删除集群信息表中的D节点，预期结果2
预期结果:
    1. coordinator的实例总量为2，group0包含1P1D
    2. coordinator的实例总量为0，group0包含1P
*/
TEST_F(TestNodeScheduler, Test1P1DScaleInD)
{
    // 修改rankTable
    ModifyRankTable(2);

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
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
    target[GetIdFormIp("127.0.0.3")] = {GetIdFormIp("127.0.0.2")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(2, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    // 删除节点2
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取文件权限777
    rawConfig["server_group_list"][2]["server_list"].erase(rawConfig["server_group_list"][2]["server_list"].begin()
        + 1);
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ChangeFileMode(newRankTable);
    EXPECT_TRUE(CheckOnlineInstanceEmpty(30));

    nodeScheduler->Stop();
    statusUpdater->Stop();
    thread->join();
}

/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，1P1D，删除后重启1D
测试步骤:
    1. NodeScheduler完成PD身份初始化，预期结果1
    2. 删除集群信息表中的D节点，预期结果2
    3. 重启集群信息表中的D节点，预期结果3
预期结果:
    1. coordinator的实例总量为2，group0包含1P1D
    2. coordinator的实例总量为0，group0包含1P
    3. coordinator的实例总量为2，group0包含1P1D
*/
TEST_F(TestNodeScheduler, Test1P1DScaleOutD)
{
    // 修改rankTable
    ModifyRankTable(2);

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
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
    target[GetIdFormIp("127.0.0.3")] = {GetIdFormIp("127.0.0.2")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(2, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    // 删除节点2
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取文件权限777
    rawConfig["server_group_list"][2]["server_list"].erase(rawConfig["server_group_list"][2]["server_list"].begin()
        + 1);
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ChangeFileMode(newRankTable);
    EXPECT_TRUE(CheckOnlineInstanceEmpty(30));
    std::vector<std::string> peers = {"127.0.0.3"};
    std::string ip = "127.0.0.2";
    // 缩容会删除peers, 故该处的检查作为缩容时正确删除peers的检查
    EXPECT_FALSE(CheckServerPeers(TIMEOUT, ip, peers));

    // 重启节点2
    CopyFile(originRankTable, newRankTable);
    EXPECT_FALSE(CheckOnlineInstanceWithAttempt(TIMEOUT, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    nodes = nodeStatus->GetAllNodes();
    faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(1, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    nodeScheduler->Stop();
    statusUpdater->Stop();
    thread->join();
}

/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，1P1D，删除1P
测试步骤:
    1. NodeScheduler完成PD身份初始化，预期结果1
    2. 删除集群信息表中的D节点，预期结果2
预期结果:
    1. coordinator的实例总量为2，group0包含1P1D
    2. coordinator的实例总量为0，group0包含1D
*/
TEST_F(TestNodeScheduler, Test1P1DScaleInP)
{
    // 修改rankTable
    ModifyRankTable(2);

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
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
    target[GetIdFormIp("127.0.0.3")] = {GetIdFormIp("127.0.0.2")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(2, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    // 删除节点1
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取文件权限777
    rawConfig["server_group_list"][2]["server_list"].erase(rawConfig["server_group_list"][2]["server_list"].begin());
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ChangeFileMode(newRankTable);
    EXPECT_TRUE(CheckOnlineInstanceEmpty(30));

    nodeScheduler->Stop();
    statusUpdater->Stop();
    thread->join();
}

/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，1P1D，删除后重启1P
测试步骤:
    1. NodeScheduler完成PD身份初始化，预期结果1
    2. 删除集群信息表中的D节点，预期结果2
    2. 重启集群信息表中的D节点，预期结果3
预期结果:
    1. coordinator的实例总量为2，group0包含1P1D
    2. coordinator的实例总量为0，group0包含1D
    2. coordinator的实例总量为2，group0包含1P1D
*/
TEST_F(TestNodeScheduler, Test1P1DScaleOutP)
{
    // 修改rankTable
    ModifyRankTable(2);

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
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
    target[GetIdFormIp("127.0.0.3")] = {GetIdFormIp("127.0.0.2")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(2, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    // 删除节点1
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取文件权限777
    rawConfig["server_group_list"][2]["server_list"].erase(rawConfig["server_group_list"][2]["server_list"].begin());
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ChangeFileMode(newRankTable);
    EXPECT_TRUE(CheckOnlineInstanceEmpty(30));
    std::vector<std::string> peers = {"127.0.0.2"};
    std::string ip = "127.0.0.3";
    // 缩容会删除peers, 故该处的检查作为缩容时正确删除peers的检查
    EXPECT_FALSE(CheckServerPeers(TIMEOUT, ip, peers));

    // 重启节点1
    CopyFile(originRankTable, newRankTable);
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    nodes = nodeStatus->GetAllNodes();
    faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(2, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    nodeScheduler->Stop();
    statusUpdater->Stop();
    thread->join();
}

TEST_F(TestNodeScheduler, Test1P1DScaleInDFail)
{
    // 修改rankTable
    ModifyRankTable(2);

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
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
    target[GetIdFormIp("127.0.0.3")] = {GetIdFormIp("127.0.0.2")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(2, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    // 删除节点2
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取文件权限777
    rawConfig["server_group_list"][2]["server_list"].erase(rawConfig["server_group_list"][2]["server_list"].begin()
        + 1);
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ChangeFileMode(newRankTable);

    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerToServerHttpClientFail, SetHostAndPort));
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerToServerHttpClientFail, ControllerSendRequestMock));
    EXPECT_TRUE(CheckOnlineInstanceEmpty(30));
    std::string ip = "127.0.0.2";
    EXPECT_TRUE(!nodeStatus->GetNode(GetIdFormIp(ip))->isHealthy);

    nodeScheduler->Stop();
    statusUpdater->Stop();
    thread->join();
}

/*
测试描述: 测试NodeScheduler，PD分离模式节点初始化，1P1D，删除1P
测试步骤:
    1. NodeScheduler完成PD身份初始化，预期结果1
    2. 删除集群信息表中的D节点，预期结果2
预期结果:
    1. coordinator的实例总量为2，group0包含1P1D
    2. coordinator的实例总量为0，group0包含1D
*/
TEST_F(TestNodeScheduler, Test1P1DScaleInPFail)
{
    // 修改rankTable
    ModifyRankTable(2);

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
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
    target[GetIdFormIp("127.0.0.3")] = {GetIdFormIp("127.0.0.2")};
    EXPECT_TRUE(CheckOnlineInstanceWithAttempt(30, 0, target));
    EXPECT_EQ(0, GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(2, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    // 删除节点1
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取文件权限777
    rawConfig["server_group_list"][2]["server_list"].erase(rawConfig["server_group_list"][2]["server_list"].begin());
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ChangeFileMode(newRankTable);

    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerToServerHttpClientFail, SetHostAndPort));
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerToServerHttpClientFail, ControllerSendRequestMock));
    EXPECT_TRUE(CheckOnlineInstanceEmpty(30));
    std::string ip = "127.0.0.3";
    EXPECT_TRUE(!nodeStatus->GetNode(GetIdFormIp(ip))->isHealthy);

    nodeScheduler->Stop();
    statusUpdater->Stop();
    thread->join();
}

/*
测试描述: 测试NodeScheduler，解析调度器返回的响应
测试步骤:
    1. request_length_info中的数值字段非法，预期结果1
    2. schedule_info中的数值字段非法，预期结果1
    3. 响应体为空字符串，预期结果1
    4. request_length_info或者schedule_info缺失，预期结果1
    5. schedule_info中的实例个数超过96，预期结果1
预期结果:
    1. 解析消息体失败
*/
TEST_F(TestNodeScheduler, TestInvalidCoordinatorInfoResp)
{
    nlohmann::json respJson = {
        {"schedule_info", {{
            {"id", 0},
            {"allocated_slots", 0},
            {"allocated_blocks", 0}
        }}},
        {"request_length_info", {
            {"input_len", 0},
            {"output_len", 0},
        }}
    };
    auto str = respJson.dump();
    EXPECT_EQ(0, nodeScheduler->ParseCoordinatorInfoResp(str));

    nlohmann::json invalidRespJson;
    std::cout << "case 1" << std::endl;
    std::vector<std::string> keys {"input_len", "output_len"};
    for (auto &key : std::as_const(keys)) {
        invalidRespJson = respJson;
        invalidRespJson["request_length_info"][key] = 4294967296;
        str = invalidRespJson.dump();
        EXPECT_EQ(-1, nodeScheduler->ParseCoordinatorInfoResp(str));
    }
    for (auto &key : std::as_const(keys)) {
        invalidRespJson = respJson;
        invalidRespJson["request_length_info"][key] = -1;
        str = invalidRespJson.dump();
        EXPECT_EQ(-1, nodeScheduler->ParseCoordinatorInfoResp(str));
    }

    std::cout << "case 2" << std::endl;
    std::vector<std::string> scheduleInfoKeys {"id", "allocated_slots", "allocated_blocks"};
    for (auto &key : std::as_const(scheduleInfoKeys)) {
        invalidRespJson = respJson;
        invalidRespJson["schedule_info"][0][key] = -1;
        str = invalidRespJson.dump();
        EXPECT_EQ(-1, nodeScheduler->ParseCoordinatorInfoResp(str));
    }
    invalidRespJson = respJson;
    invalidRespJson["schedule_info"][0]["allocated_slots"] = 5001;
    str = invalidRespJson.dump();
    EXPECT_EQ(-1, nodeScheduler->ParseCoordinatorInfoResp(str));
    invalidRespJson = respJson;
    invalidRespJson["schedule_info"][0]["allocated_blocks"] = 4294967296;
    str = invalidRespJson.dump();
    EXPECT_EQ(-1, nodeScheduler->ParseCoordinatorInfoResp(str));

    std::cout << "case 3" << std::endl;
    str = "";
    EXPECT_EQ(-1, nodeScheduler->ParseCoordinatorInfoResp(str));

    std::cout << "case 4" << std::endl;
    std::vector<std::string> objectKeys {"schedule_info", "request_length_info"};
    for (auto &key : std::as_const(objectKeys)) {
        invalidRespJson = respJson;
        invalidRespJson = invalidRespJson.erase(key);
        str = invalidRespJson.dump();
        EXPECT_EQ(-1, nodeScheduler->ParseCoordinatorInfoResp(str));
    }

    std::cout << "case 5" << std::endl;
    invalidRespJson = respJson;
    nlohmann::json serverNodeJson = {
        {"id", 0},
        {"allocated_slots", 0},
        {"allocated_blocks", 0}
    };
    for (int32_t i = 0; i <= 6 * 16 * 6 -1; i ++) {
        invalidRespJson["schedule_info"].push_back(serverNodeJson);
    }
    str = invalidRespJson.dump();
    EXPECT_EQ(-1, nodeScheduler->ParseCoordinatorInfoResp(str));
}

TEST_F(TestNodeScheduler, TestProcessRoleUnknownForD)
{
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    
    auto node = std::make_unique<NodeInfo>();
    node->ip = "127.0.0.2";
    node->instanceInfo.staticInfo.id = GetIdFormIp("127.0.0.2");
    node->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    node->instanceInfo.staticInfo.groupId = 0;
    node->roleState = ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN);
    nodeStatus->AddNode(std::move(node));
    
    std::vector<uint64_t> dIds = {GetIdFormIp("127.0.0.2")};

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));
    
    nodeScheduler->ProcessRoleUnknownForD(dIds);

    auto retrievedNode = nodeStatus->GetNode(GetIdFormIp("127.0.0.2"));
    EXPECT_NE(retrievedNode, nullptr);
    EXPECT_GE(retrievedNode->initRetryTimes, 0);
}

TEST_F(TestNodeScheduler, TestProcessRoleUnknownForP)
{
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    
    auto pNode = std::make_unique<NodeInfo>();
    pNode->ip = "127.0.0.2";
    pNode->instanceInfo.staticInfo.id = GetIdFormIp("127.0.0.2");
    pNode->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    pNode->instanceInfo.staticInfo.groupId = 0;
    pNode->roleState = ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN);
    pNode->peers = {GetIdFormIp("127.0.0.3")};
    
    auto dNode = std::make_unique<NodeInfo>();
    dNode->ip = "127.0.0.3";
    dNode->instanceInfo.staticInfo.id = GetIdFormIp("127.0.0.3");
    dNode->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    dNode->instanceInfo.staticInfo.groupId = 0;
    dNode->roleState = ControllerConstant::GetInstance()->GetRoleState(RoleState::READY);

    nodeStatus->AddNode(std::move(pNode));
    nodeStatus->AddNode(std::move(dNode));
    
    std::vector<uint64_t> pIds = {GetIdFormIp("127.0.0.2")};

    Stub stub;
    stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
    stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));

    nodeScheduler->ProcessRoleUnknownForP(pIds);

    auto retrievedNode = nodeStatus->GetNode(GetIdFormIp("127.0.0.2"));
    EXPECT_NE(retrievedNode, nullptr);
    EXPECT_GE(retrievedNode->initRetryTimes, 0);
}

TEST_F(TestNodeScheduler, TestStopUnavailableNodes)
{
    // Test case 1: NPU recovery is disabled
    {
        // Modify config to disable NPU recovery
        auto testJson = GetServerRequestHandlerTestJsonPath();
        ModifyJsonItem(testJson, "npu_recovery_enable", "", false);
        ControllerConfig::GetInstance()->Init();

        std::vector<std::unique_ptr<NodeInfo>> availableNodes;
        auto node = std::make_unique<NodeInfo>();
        node->ip = "127.0.0.2";
        node->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(node));

        EXPECT_NO_THROW(nodeScheduler->StopUnavailableNodes(availableNodes));
    }

    // Test case 2: No UNAVAILABLE nodes
    {
        // Enable NPU recovery
        auto testJson = GetServerRequestHandlerTestJsonPath();
        ModifyJsonItem(testJson, "npu_recovery_enable", "", true);
        ControllerConfig::GetInstance()->Init();

        std::vector<std::unique_ptr<NodeInfo>> availableNodes;
        // Add a node that is not UNAVAILABLE
        auto node = std::make_unique<NodeInfo>();
        node->ip = "127.0.0.2";
        node->inferenceType = InferenceType::AVAILABLE; // Not UNAVAILABLE
        availableNodes.push_back(std::move(node));

        EXPECT_NO_THROW(nodeScheduler->StopUnavailableNodes(availableNodes));
    }

    // Test case 3: No nodes with IP addresses
    {
        std::vector<std::unique_ptr<NodeInfo>> availableNodes;
        // Add a node with empty IP
        auto node = std::make_unique<NodeInfo>();
        node->ip = ""; // Empty IP
        node->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(node));

        EXPECT_NO_THROW(nodeScheduler->StopUnavailableNodes(availableNodes));
    }

    // Test case 4: Valid UNAVAILABLE nodes with successful STOP_ENGINE command
    {
        std::vector<std::unique_ptr<NodeInfo>> availableNodes;
        // Add multiple nodes with same IP to test deduplication
        auto node1 = std::make_unique<NodeInfo>();
        node1->ip = "127.0.0.2";
        node1->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(node1));

        auto node2 = std::make_unique<NodeInfo>();
        node2->ip = "127.0.0.3";
        node2->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(node2));

        auto node3 = std::make_unique<NodeInfo>();
        node3->ip = "127.0.0.2"; // Duplicate IP
        node3->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(node3));

        Stub stub;
        stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
        stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));

        EXPECT_NO_THROW(nodeScheduler->StopUnavailableNodes(availableNodes));
    }

    // Test case 5: Valid UNAVAILABLE nodes with failed STOP_ENGINE command
    {
        std::vector<std::unique_ptr<NodeInfo>> availableNodes;
        auto node = std::make_unique<NodeInfo>();
        node->ip = "127.0.0.4";
        node->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(node));

        Stub stub;
        stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerToServerHttpClientFail, SetHostAndPort));
        stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerToServerHttpClientFail, ControllerSendRequestMock));

        EXPECT_NO_THROW(nodeScheduler->StopUnavailableNodes(availableNodes));
    }

    // Test case 6: HttpClient initialization failure
    {
        std::vector<std::unique_ptr<NodeInfo>> availableNodes;
        auto node = std::make_unique<NodeInfo>();
        node->ip = "127.0.0.5";
        node->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(node));

        Stub stub;
        // Use a mock function that returns -1 to simulate initialization failure
        stub.set(ADDR(HttpClient, Init), TestNodeScheduler::HttpClientInitFail);

        EXPECT_NO_THROW(nodeScheduler->StopUnavailableNodes(availableNodes));
    }

    // Test case 7: Mixed node types (some UNAVAILABLE, some not)
    {
        std::vector<std::unique_ptr<NodeInfo>> availableNodes;
        
        // Add UNAVAILABLE node
        auto unavailableNode = std::make_unique<NodeInfo>();
        unavailableNode->ip = "127.0.0.6";
        unavailableNode->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(unavailableNode));
        
        // Add AVAILABLE node (should be ignored)
        auto availableNode = std::make_unique<NodeInfo>();
        availableNode->ip = "127.0.0.7";
        availableNode->inferenceType = InferenceType::AVAILABLE;
        availableNodes.push_back(std::move(availableNode));
        
        // Add another UNAVAILABLE node
        auto anotherUnavailableNode = std::make_unique<NodeInfo>();
        anotherUnavailableNode->ip = "127.0.0.8";
        anotherUnavailableNode->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(anotherUnavailableNode));

        Stub stub;
        stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
        stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));

        EXPECT_NO_THROW(nodeScheduler->StopUnavailableNodes(availableNodes));
    }

    // Test case 8: Node with null pointer (should be skipped)
    {
        std::vector<std::unique_ptr<NodeInfo>> availableNodes;
        
        // Add a null pointer (this simulates a potential edge case)
        availableNodes.push_back(nullptr);
        
        // Add a valid UNAVAILABLE node
        auto node = std::make_unique<NodeInfo>();
        node->ip = "127.0.0.9";
        node->inferenceType = InferenceType::UNAVAILABLE;
        availableNodes.push_back(std::move(node));

        Stub stub;
        stub.set(ADDR(HttpClient, SetHostAndPort), ADDR(ControllerHttpClientMock, SetHostAndPort));
        stub.set(ADDR(HttpClient, SendRequest), ADDR(ControllerHttpClientMock, ControllerSendRequestMock));

        EXPECT_NO_THROW(nodeScheduler->StopUnavailableNodes(availableNodes));
    }
}