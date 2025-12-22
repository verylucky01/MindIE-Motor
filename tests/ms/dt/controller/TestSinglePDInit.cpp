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
#include <thread>
#include <mutex>
#include "gtest/gtest.h"
#include "stub.h"
#include "gmock/gmock.h"
#include "Helper.h"
#include "SinglePDStub.h"
#include "ControllerConfig.h"
#include "NodeScheduler.h"
#include "StatusUpdater.h"
#include "ProcessManager.h"
using namespace MINDIE::MS;

class TestSinglePDPostRole : public testing::Test {
public:
    void SetUp() override
    {
        CopyDefaultConfig();
        SinglePDMock::ClearAllMock();
        InitNodeStatus();
        InitCoordinatorStore();
        InitNodeScheduler();
        InitStatusUpdater();
        auto controllerJson = GetMSControllerConfigJsonPath();
        auto testJson = GetServerRequestHandlerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "is_single_container", "", true); // 配置为单机PD分离模式
        ModifyJsonItem(testJson, "check_role_attempt_times", "", 2); // 改小一点避免等待时间过长
        ModifyJsonItem(testJson, "check_role_wait_seconds", "", 1); // 改小一点避免等待时间过长
        ModifyJsonItem(testJson, "rank_table_detecting_seconds", "", 4); // 检查集群信息表的时间改成4s避免等待时间过长
        ModifyJsonItem(testJson, "disappeared_server_waiting_seconds", "", 5); // 检查集群信息表的时间改成5s避免等待时间过长
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

        std::atomic<bool> waitClusterDGRTSave{true};
        nodeScheduler->SetWaitClusterDGRTSave(&waitClusterDGRTSave);
    }

    void InitStatusUpdater()
    {
        statusUpdater = std::make_shared<StatusUpdater>(nodeStatus, coordinatorStore);
    }

    void ModifyRankTable(uint32_t serverCount)
    {
        auto originRankTable = GetMSGlobalRankTableTestJsonPath();
        auto rawConfig = FileToJsonObj(originRankTable, 0777);
        nlohmann::json serverNodeJson = {
            {"server_id", "127.0.0.1"},
            {"server_ip", "127.0.0.2"},
            {"predict_port", "1170"},
            {"mgmt_port", "1171"},
            {"metric_port", "1172"},
            {"inter_comm_port", "1173"},
            {"device", {
                {
                    {"device_id", "0"},
                    {"device_ip", "127.0.0.1"},
                    {"device_logical_id", "0"},
                    {"rank_id", "0"}
                }
            }}
        };
        nlohmann::json serverList = nlohmann::json::array();
        for (uint32_t i = 0; i < serverCount; i++) {
            // 只需要管理面端口不重复即可，不需要实际进行http通信，会对发请求行为打桩
            serverNodeJson["mgmt_port"] = std::to_string(3024 + i);
            serverList.push_back(serverNodeJson);
        }
        rawConfig["server_group_list"][2]["server_list"] = serverList;
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
测试描述: 测试单机PD分离场景初始化集群，1P3D均初始化成功
测试步骤:
    1. 修改rankTable，添加4个Server实例信息
    2. 初始化NodeScheduler、statusUpdater等对象
    3. 初始化集群身份，预期结果1
预期结果:
    1. controller同步至coordinator的集群可用实例总量为4（group 0，包含1P3D且PD间链接关系正确）
*/
TEST_F(TestSinglePDPostRole, Post1P3DSuccess)
{
    // 修改rankTable
    setenv("HSECEASY_PATH", GetHSECEASYPATH().c_str(), 1);
    ModifyRankTable(4);
    // 打桩身份下发请求发送成功
    SinglePDMock::SetPostRoleHttpStatus(true);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SinglePDMock::SinglePDSendRequestMock);

    // 执行函数
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });

    // 检查预期结果
    bool checkResult = false;
    uint64_t id2 = GetIdFormIp("127.0.0.2");
    uint64_t id3 = GetIdFormIp("127.0.0.3");
    uint64_t id4 = GetIdFormIp("127.0.0.4");
    uint64_t id5 = GetIdFormIp("127.0.0.5");
    std::vector<std::map<uint64_t, std::vector<uint64_t>>> validTargets = { // 定义所有可能的合法情况
        {{id2, {}}, {id3, {id2}}, {id4, {id2}}, {id5, {id2}}},
        {{id2, {id3}}, {id3, {}}, {id4, {id3}}, {id5, {id3}}},
        {{id2, {id4}}, {id3, {id4}}, {id4, {}}, {id5, {id4}}},
        {{id2, {id5}}, {id3, {id5}}, {id4, {id5}}, {id5, {}}}
    };
    for (const auto& target : validTargets) {
        if (SinglePDMock::CheckOnlineInstanceWithAttempt(30, 0, target)) { // 30: times
            checkResult = true;
            break;
        }
    }
    EXPECT_TRUE(checkResult);
    EXPECT_EQ(0, SinglePDMock::GetInvalidInstanceCount());

    nodeScheduler->Stop();
    thread->join();
    statusUpdater->Stop();
}

/*
测试描述: 测试单机PD分离场景初始化集群，1P3D均初始化时，下发身份请求失败，集群不可用
测试步骤:
    1. 修改rankTable，添加4个Server实例信息
    2. 初始化NodeScheduler、statusUpdater等对象
    3. 初始化集群身份，打桩身份下发请求发送状态，预期结果1
预期结果:
    1. controller维护的集群状态nodeStatus中无可用节点，失败节点数量为4，同步至cooridinator的集群信息中无可用实例
*/
TEST_F(TestSinglePDPostRole, SendRoleRequestFailed)
{
    // 修改rankTable
    ModifyRankTable(4);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SinglePDMock::SinglePDSendRequestMock);

    // 执行函数
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));

    // 打桩身份下发请求发送失败
    SinglePDMock::SetPostRoleHttpStatus(false);
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });
    // 等待身份下发请求发送/重试完成，保证节点信息更新完毕
    std::this_thread::sleep_for(std::chrono::seconds(11));      // 增加延时到11秒
    nodeScheduler->Stop();
    thread->join();

    // 检查预期结果
    EXPECT_TRUE(SinglePDMock::CheckOnlineInstanceEmpty(30));
    EXPECT_EQ(0, SinglePDMock::GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(0, nodes.size());
    EXPECT_EQ(4, faultyNodes.size());

    statusUpdater->Stop();
}

/*
测试描述: 测试单机PD分离场景初始化集群，1P3D均初始化时，1D身份状态初始化失败，集群不可用
测试步骤:
    1. 修改rankTable，添加4个Server实例信息
    2. 初始化NodeScheduler、statusUpdater等对象
    3. 打桩Server节点，将其中1D（选择mgmt_port为3025的节点）的身份状态设为UNKNOWN
    4. 初始化集群身份
预期结果:
    1. controller维护的集群状态nodeStatus中无可用节点，失败节点数量为4，同步至cooridinator的集群信息中无可用实例
*/
TEST_F(TestSinglePDPostRole, RoleStateUnknown)
{
    // 修改rankTable
    ModifyRankTable(4);
    // 打桩身份下发请求发送成功
    SinglePDMock::SetPostRoleHttpStatus(true);

    // Server身份状态打桩
    SinglePDMock::ServerMock unknownRoleServer;
    unknownRoleServer.roleState = RoleState::UNKNOWN;
    SinglePDMock::AddServerMock("3025", unknownRoleServer);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SinglePDMock::SinglePDSendRequestMock);

    // 执行函数
    auto deployMode = ControllerConfig::GetInstance()->GetDeployMode();
    EXPECT_EQ(0, nodeScheduler->Init(deployMode));
    EXPECT_EQ(0, statusUpdater->Init(deployMode));
    auto thread = std::make_unique<std::thread>([this]() {
        this->nodeScheduler->Run();
    });
    // 等待身份状态检查完成，保证节点信息更新完毕
    std::this_thread::sleep_for(std::chrono::seconds(11));      // 增加延时到11秒
    nodeScheduler->Stop();
    thread->join();

    // 检查预期结果
    EXPECT_FALSE(SinglePDMock::CheckOnlineInstanceEmpty(30));
    EXPECT_EQ(0, SinglePDMock::GetInvalidInstanceCount());
    auto nodes = nodeStatus->GetAllNodes();
    auto faultyNodes = nodeStatus->GetAllFaultyNodes();
    EXPECT_EQ(4, nodes.size());
    EXPECT_EQ(0, faultyNodes.size());

    statusUpdater->Stop();
}