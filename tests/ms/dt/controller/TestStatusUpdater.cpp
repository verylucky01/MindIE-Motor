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
#include "gtest/gtest.h"
#include "stub.h"
#include "StatusUpdater.h"
#include "ControllerConfig.h"
#include "Helper.h"
#include "RequestServerStub.h"
#include "CoordinatorRequestHandler.h"
#include "JsonFileManager.h"

using namespace MINDIE::MS;

class TestStatusUpdater : public testing::Test {
public:
    void SetUp() override
    {
        InitHttpClient();
        InitNodeStatus();
        InitCoordinatorStore();

        CopyDefaultConfig();
        auto controllerJson = GetMSControllerTestJsonPath();
        JsonFileManager manager(controllerJson);
        manager.Load();
        manager.SetList({"http_server", "port"}, 2028);
        manager.SetList({"tls_config", "request_coordinator_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "http_server_tls_enable"}, false);
        manager.Save();
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
        ControllerConfig::GetInstance()->Init();
        CoordinatorRequestHandler::GetInstance()->SetRunStatus(true);
    }

    void InitNodeStatus()
    {
        nodeStatus = std::make_shared<NodeStatus>();
        auto nodeInfo1 = std::make_unique<NodeInfo>();
        nodeInfo1->ip = host;
        nodeInfo1->port = port;
        nodeInfo1->isHealthy = true;
        nodeInfo1->isInitialized = true;
        nodeInfo1->roleState = "running";
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
        nodeInfo1->instanceInfo.dynamicInfo.availSlotsNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.availBlockNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.waitingRequestNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.runningRequestNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.swappedRequestNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.freeNpuBlockNums = 0;
        nodeInfo1->instanceInfo.dynamicInfo.freeCpuBlockNums = 0;
        nodeInfo1->instanceInfo.dynamicInfo.totalNpuBlockNums = 0;
        nodeInfo1->instanceInfo.dynamicInfo.totalCpuBlockNums = 0;
        nodeStatus->AddNode(std::move(nodeInfo1));
    }

    void InitHttpClient()
    {
        client = std::make_shared<HttpClient>();
        tlsItems.tlsEnable = false;
        host = "127.0.0.1";
        port = "1026";
        client->Init(host, port, tlsItems, false);
    }

    void InitCoordinatorStore()
    {
        coordinatorStore = std::make_shared<CoordinatorStore>();
        auto coordinatorInfo = std::make_unique<Coordinator>();
        coordinatorInfo->ip = coordinatorHost;
        coordinatorInfo->port = coordinatorPort;
        std::vector<std::unique_ptr<Coordinator>> coordinatorVec;
        coordinatorVec.push_back(std::move(coordinatorInfo));
        coordinatorStore->UpdateCoordinators(coordinatorVec);
    }

    TlsItems tlsItems;
    std::shared_ptr<NodeStatus> nodeStatus;
    std::shared_ptr<HttpClient> client;
    std::shared_ptr<CoordinatorStore> coordinatorStore;
    std::string host;
    std::string port;
    std::string coordinatorHost = "127.0.0.2";
    std::string coordinatorPort = "8080";
};

/*
测试描述: 测试StatusUpdater，请求成功
测试步骤:
    1. 打桩HttpClient接收到的响应为成功。
    2. StatusUpdater调用UpdateNodePeriod更新节点状态。
预期结果:
    1. UpdateNodeStatus方法返回0表示成功。
    2. 节点的信息更新成功:
        - 节点身份为P节点
        - 节点状态为RoleReady
        - 可用Slots为200
        - 可用Block为1024
*/
TEST_F(TestStatusUpdater, TestUpdateNodePeriodSuccess)
{
    SetCode(200);
    SetRet(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    StatusUpdater statusUpdater(nodeStatus, coordinatorStore);
    statusUpdater.Init(DeployMode::SINGLE_NODE);

    auto coordinators = coordinatorStore->GetCoordinators();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(coordinators.size(), 1);

    auto startTime = std::chrono::steady_clock::now();
    auto maxWaitTime = std::chrono::seconds(3);
    bool isHealthy = false;

    while (!isHealthy && std::chrono::steady_clock::now() - startTime < maxWaitTime) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 每100毫秒检查一次状态
        auto temp = coordinatorStore->GetCoordinators();
        isHealthy = temp[0]->isHealthy;
    }

    EXPECT_EQ(isHealthy, true);  // 请求成功，isHealthy应为true
}

/*
测试描述: 测试StatusUpdater，请求失败
测试步骤:
    1. 打桩HttpClient接收到的响应为失败。
    2. StatusUpdater调用UpdateNodePeriod更新节点状态。
预期结果:
    1. 请求失败时，coordinator的isHealthy字段为false。
*/
TEST_F(TestStatusUpdater, TestUpdateNodePeriodRequestFailure)
{
    SetCode(-1);  // 模拟请求失败
    SetRet(200);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    StatusUpdater statusUpdater(nodeStatus, coordinatorStore);
    statusUpdater.Init(DeployMode::SINGLE_NODE);

    auto coordinators = coordinatorStore->GetCoordinators();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(coordinators.size(), 1);

    auto startTime = std::chrono::steady_clock::now();
    auto maxWaitTime = std::chrono::seconds(1);
    bool isHealthy = false;

    while(!isHealthy && std::chrono::steady_clock::now() - startTime < maxWaitTime) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 每100毫秒检查一次状态
        auto temp = coordinatorStore->GetCoordinators();
        isHealthy = temp[0]->isHealthy;
    }

    EXPECT_EQ(coordinators[0]->isHealthy, false);  // 请求失败，isHealthy应为false
}

/*
测试描述: 测试StatusUpdater，响应为成功但非200
测试步骤:
    1. 打桩HttpClient接收到的响应为成功但非200。
    2. StatusUpdater调用UpdateNodePeriod更新节点状态。
预期结果:
    1. 响应非200时，coordinator的isHealthy字段为false。
*/
TEST_F(TestStatusUpdater, TestUpdateNodePeriodNon200Response)
{
    SetCode(500);  // 模拟返回500响应
    SetRet(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    StatusUpdater statusUpdater(nodeStatus, coordinatorStore);
    statusUpdater.Init(DeployMode::SINGLE_NODE);

    auto coordinators = coordinatorStore->GetCoordinators();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_EQ(coordinators.size(), 1);

    auto startTime = std::chrono::steady_clock::now();
    auto maxWaitTime = std::chrono::seconds(1);
    bool isHealthy = false;

    while(!isHealthy && std::chrono::steady_clock::now() - startTime < maxWaitTime) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 每100毫秒检查一次状态
        auto temp = coordinatorStore->GetCoordinators();
        isHealthy = temp[0]->isHealthy;
    }

    EXPECT_EQ(coordinators[0]->isHealthy, false);  // 请求失败，isHealthy应为false
}