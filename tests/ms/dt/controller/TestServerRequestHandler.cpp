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
#include "ServerRequestHandler.h"
#include "ControllerConfig.h"
#include "Helper.h"

#include "RequestServerStub.h"

using namespace MINDIE::MS;

class TestServerRequestHandler : public testing::Test {
public:
    void SetUp() override
    {
        InitHttpClient();
        InitNodeStatusV2();

        CopyDefaultConfig();
        auto controllerJson = GetMSControllerTestJsonPath();
        auto testJson = GetServerRequestHandlerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "check_role_attempt_times", "", 2); // 改小一点避免等待时间过长
        ModifyJsonItem(testJson, "check_role_wait_seconds", "", 1); // 改小一点避免等待时间过长
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << testJson << std::endl;
        ControllerConfig::GetInstance()->Init();
        ServerRequestHandler::GetInstance()->Init(nullptr, nullptr);
    }

    void SetStaticInfo(std::unique_ptr<NodeInfo> &nodeInfo, uint64_t id, MINDIE::MS::DIGSInstanceLabel instanceLabel,
                       MINDIE::MS::DIGSInstanceRole instanceRole)
    {
        nodeInfo->isHealthy = true;
        nodeInfo->isInitialized = true;
        nodeInfo->modelName = "model1";
        nodeInfo->roleState = "running";
        nodeInfo->instanceInfo.staticInfo.id = id;
        nodeInfo->instanceInfo.staticInfo.groupId = 1;
        nodeInfo->instanceInfo.staticInfo.maxSeqLen = 2048; // 2048: maxSeqLen
        nodeInfo->instanceInfo.staticInfo.maxOutputLen = 512; // 512: maxOutputLen
        nodeInfo->instanceInfo.staticInfo.totalSlotsNum = 200; // 200: totalSlotsNum
        nodeInfo->instanceInfo.staticInfo.totalBlockNum = 1024; // 1024: totalBlockNum
        nodeInfo->instanceInfo.staticInfo.blockSize = 128; // 128: blockSize
        nodeInfo->instanceInfo.staticInfo.label = instanceLabel;
        nodeInfo->instanceInfo.staticInfo.role = instanceRole;
        nodeInfo->instanceInfo.dynamicInfo.availSlotsNum = 0;
        nodeInfo->instanceInfo.dynamicInfo.availBlockNum = 0;
    }

    void CreateDeviceInfo(size_t deviceNum, ServerInfo &serverInfo, std::string ipPrefix, std::string superPodId)
    {
        std::vector<DeviceInfo> deviceInfosList;
        for (size_t i = 0; i < deviceNum; ++i) {
            DeviceInfo deviceInfo;
            deviceInfo.id = std::to_string(i);
            deviceInfo.ip = ipPrefix + std::to_string(i);
            deviceInfo.logicalId = std::to_string(i);
            deviceInfo.rankId = i;
            deviceInfo.superDeviceId = std::make_optional<std::string>(std::to_string(2000 * i)); // 2000: superDeviceId
            serverInfo.deviceInfos.emplace_back(deviceInfo);
            deviceInfosList.emplace_back(deviceInfo);
        }
        serverInfo.superPodId = std::make_optional<std::string>(superPodId);
        serverInfo.dpGroupInfos[0] = deviceInfosList;
    }

    void InitNodeStatusV2()
    {
        nodeStatus = std::make_shared<NodeStatus>();
        auto nodeInfo1 = std::make_unique<NodeInfo>();
        nodeInfo1->ip = host;
        nodeInfo1->port = port;
        nodeInfo1->peers = { 50336172, 33558956 };
        nodeInfo1->activePeers = { 50336172, 33558956 };
        SetStaticInfo(nodeInfo1, 16781740, // 16781740: nodeId
            MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
        ServerInfo serverInfo1;
        serverInfo1.hostId = "71.14.88.104";
        serverInfo1.ip = "192.168.88.1";
        std::string serverIp1 = "10.11.131.";
        CreateDeviceInfo(7, serverInfo1, serverIp1, "0");

        ServerInfo serverInfo2;
        serverInfo2.hostId = "71.14.88.105";
        serverInfo2.ip = "192.168.88.2";
        std::string serverIp2 = "10.11.132.";
        CreateDeviceInfo(7, serverInfo2, serverIp2, "0");

        nodeInfo1->serverInfoList.emplace_back(serverInfo1);
        nodeInfo1->serverInfoList.emplace_back(serverInfo2);
        nodeStatus->AddNode(std::move(nodeInfo1));
        auto nodeInfo2 = std::make_unique<NodeInfo>();
        nodeInfo2->ip = "127.0.0.2";
        nodeInfo2->port = port;
        nodeInfo2->peers = { 50336172 };
        nodeInfo2->activePeers = {};
        SetStaticInfo(nodeInfo2, 33558956, // 33558956: nodeId
            MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER, MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
        nodeInfo2->serverInfoList.emplace_back(serverInfo1);
        nodeInfo2->serverInfoList.emplace_back(serverInfo2);
        nodeStatus->AddNode(std::move(nodeInfo2));
        auto nodeInfo3 = std::make_unique<NodeInfo>();
        nodeInfo3->ip = "127.0.0.3";
        nodeInfo3->port = port;
        nodeInfo3->peers = { 16781740, 33558956 };
        nodeInfo3->activePeers = { 16781740, 33558956 };
        SetStaticInfo(nodeInfo3, 50336172, // 50336172: node Id
            MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER, MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
        nodeInfo3->serverInfoList.emplace_back(serverInfo1);
        nodeInfo3->serverInfoList.emplace_back(serverInfo2);
        nodeStatus->AddNode(std::move(nodeInfo3));
    }

    void InitHttpClient()
    {
        client = std::make_shared<HttpClient>();
        tlsItems.tlsEnable = false;
        host = "127.0.0.1";
        port = "1026";
        client->Init(host, port, tlsItems, false);
    }

    TlsItems tlsItems;
    std::shared_ptr<NodeStatus> nodeStatus;
    std::shared_ptr<HttpClient> client;
    std::string host;
    std::string port;
};

/*
测试描述: 查询server状态测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - 节点身份为P节点
        - 节点状态为ready
        - 可用Slots为200
        - 可用Block为1024
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. UpdateNodeStatus方法返回0表示成功。
    2. 节点的信息更新成功
        - 节点身份为P节点
        - 节点状态为ready
        - 可用Slots为200
        - 可用Block为1024
*/
TEST_F(TestServerRequestHandler, TestServerStatusSuccess)
{
    SetResponse(RoleStatusEnum::RoleReady, CurrentRoleEnum::prefill, 200, 1024);
    SetCode(200);
    SetRet(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    bool isReady = false;
    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "running");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);

    auto ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, 0);

    auto node2 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node2->roleState, "RoleReady");
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availSlotsNum, 200);
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availBlockNum, 1024);
}

/*
测试描述: 查询server状态失败测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为失败。
        - 方法返回值为-1
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. UpdateNodeStatus方法返回值为-1表示失败。
    2. 节点的信息不更新:
        - 节点身份为P节点
        - 节点状态为running
        - 可用Slots保持为0
        - 可用Block保持为0
    3. 节点的isHealthy字段更新为false
*/
TEST_F(TestServerRequestHandler, TestServerStatusFailure)
{
    SetRet(-1);     // 模拟发送请求失败
    SetCode(500);   // 设置HTTP响应码为500

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    bool isReady = false;
    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "running");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);
    EXPECT_EQ(node1->isHealthy, true);

    auto ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    auto node2 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node2->roleState, "RoleUnknown");  // 角色状态变更为RoleUnknown
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availSlotsNum, 0);  // 可用Slots保持为0
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availBlockNum, 0);  // 可用Block保持为0
    EXPECT_EQ(node2->isHealthy, false);  // isHealthy更新为false
}

/*
测试描述: 查询server状态请求成功但响应非200的测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为非200。
        - HTTP响应码为500
        - 方法返回值为0（表示请求成功但响应码错误）
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. UpdateNodeStatus方法返回0表示请求成功，但响应码错误。
    2. 节点的信息不更新:
        - 节点身份为P节点
        - 节点状态为running
        - 可用Slots保持为0
        - 可用Block保持为0
    3. 节点的isHealthy字段更新为false
*/
TEST_F(TestServerRequestHandler, TestServerStatusNon200Response)
{
    SetResponse(RoleStatusEnum::RoleReady, CurrentRoleEnum::prefill, 200, 1024);
    SetCode(500);   // 设置HTTP响应码为非200
    SetRet(0);      // 模拟发送请求成功

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    bool isReady = false;
    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "running");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);
    EXPECT_EQ(node1->isHealthy, true);

    auto ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    auto node2 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node2->roleState, "RoleUnknown");  // 角色状态变更为：RoleUnknown
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availSlotsNum, 0);  // 可用Slots保持为0
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availBlockNum, 0);  // 可用Block保持为0
    EXPECT_EQ(node2->isHealthy, false);  // isHealthy更新为false
}

/*
测试描述: 查询server状态请求成功且响应码为200的测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - HTTP响应码为200
        - 方法返回值为0（表示请求成功且响应码正确）
        - 返回内容包含更新动态数据的信息
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. UpdateNodeStatus方法返回0表示成功。
    2. 节点的信息更新成功:
        - 节点身份为P节点
        - 节点状态为RoleReady
        - 可用Slots更新为200
        - 可用Block更新为1024
    3. 节点的isHealthy字段更新为true
*/
TEST_F(TestServerRequestHandler, TestServerStatusResponse200)
{
    SetResponse(RoleStatusEnum::RoleReady, CurrentRoleEnum::prefill, 200, 1024);
    SetCode(200);   // 设置HTTP响应码为200
    SetRet(0);      // 模拟发送请求成功

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    bool isReady = false;
    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "running");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);
    EXPECT_EQ(node1->isHealthy, true);

    auto ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, 0);  // 预期返回0表示成功

    auto node2 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node2->roleState, "RoleReady");  // 角色状态更新为RoleReady
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availSlotsNum, 200);  // 可用Slots更新为200
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availBlockNum, 1024);  // 可用Block更新为1024
    EXPECT_EQ(node2->isHealthy, true);  // isHealthy更新为true
}

/*
测试描述: 查询QueryInstanceInfo状态请求成功且响应码为200的测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - HTTP响应码为200
        - 方法返回值为0（表示请求成功且响应码正确）
        - 返回内容包含更新动态数据的信息
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. QueryInstanceInfo方法返回0表示成功。
    2. 节点的信息更新成功:
        - node节点的modelName变更
        - 节点状态为RoleReady
        - 可用Slots更新为200
        - 可用Block更新为1024
*/
TEST_F(TestServerRequestHandler, TestQueryInstanceInfoResponse200)
{
    SetResponseInstanceInfo("llama_65b", 2560, 1024, 128);
    SetCode(200);   // 设置HTTP响应码为200
    SetRet(0);      // 模拟发送请求成功

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    auto ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, 0);  // 预期返回0表示成功

    EXPECT_EQ(node1->modelName, "llama_65b");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2560);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 1024);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);
}

/*
测试描述: 查询QueryInstanceInfo状态请求成功且响应码为200，但是参数取值非法
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - HTTP响应码为200
        - 方法返回值为0（表示请求成功且响应码正确）
        - 返回内容包含非法的数据
    2. ServerRequestHandler不更新节点的状态，预期结果1。
预期结果:
    1. QueryInstanceInfo方法返回-1表示失败。
    2. 节点的信息更新失败:
        - node节点的modelName变更
        - 节点状态为RoleReady
        - 可用Slots更新为200
        - 可用Block更新为1024
*/
TEST_F(TestServerRequestHandler, TestInvalidConfig)
{
    SetResponseInstanceInfo("llama_65b", 4294967296, 1024, 256);
    SetCode(200);   // 设置HTTP响应码为200
    SetRet(0);      // 模拟发送请求成功

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    auto ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回0表示成功

    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    SetResponseInstanceInfo("llama_65b", -1, 1024, 256);
    ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    SetResponseInstanceInfo("llama_65b", 2560, 0, 256);
    ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    SetResponseInstanceInfo("llama_65b", 2560, 4294967295, 256);
    ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    SetResponseInstanceInfo("llama_65b", 2560, 1024, 0);
    ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    SetResponseInstanceInfo("llama_65b", 2560, 1024, 129);
    ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回0表示成功

    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);
}

/*
测试描述: 查询QueryInstanceInfo状态请求成功请求成功但响应非200的测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为成功但是HTTP响应码为500。
        - HTTP响应码为500
        - 方法返回值为0（表示请求成功且响应码正确）
        - 返回内容包含更新动态数据的信息
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. QueryInstanceInfo方法返回0，但节点不更新。
    2. 节点的信息更新成功:
        - node节点的modelName不变更
        - 节点状态为RoleReady
        - 可用Slots不变更
        - 可用Block不变更
*/
TEST_F(TestServerRequestHandler, TestQueryInstanceInfoResponse500)
{
    SetResponseInstanceInfo("llama_65b", 2560, 1024, 256);
    SetCode(500);   // 设置HTTP响应码为500
    SetRet(0);      // 模拟发送请求成功

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    auto ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);
}

/*
测试描述: 查询QueryInstanceInfo状态请求成功请求失败
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - HTTP响应码为200
        - 方法返回值为-1（表示请求失败）
        - 返回内容包含更新动态数据的信息
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. QueryInstanceInfo方法返回0表示成功。
    2. 节点的信息更新失败:
        - node节点的modelName不变更
        - 节点状态为RoleReady
        - 可用Slots不变更
        - 可用Block不变更
*/
TEST_F(TestServerRequestHandler, TestQueryInstanceInfoResponseFailed)
{
    SetResponseInstanceInfo("llama_65b", 2560, 1024, 256);
    SetCode(200);   // 设置HTTP响应码为200
    SetRet(-1);      // 模拟发送请求失败

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);

    auto ret = ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    EXPECT_EQ(node1->modelName, "model1");
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(node1->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(node1->instanceInfo.staticInfo.blockSize, 128);
}

/*
测试描述: 查询PostSingleRole状态请求成功且响应码为200的测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - HTTP响应码为200
        - 方法返回值为0（表示请求成功且响应码正确）
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. PostSingleRole 方法返回0表示成功。
    2. 节点的信息更新成功:
        - 节点状态为RoleUnknown
        - 节点状态isHealthy为true
        - 节点状态为isHealthy为true
*/
TEST_F(TestServerRequestHandler, TestPostSingleRoleResponse200)
{
    SetCode(200);   // 设置HTTP响应码为200
    SetRet(0);      // 模拟发送请求成功

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    nodeStatus->UpdateRoleState(16781740,
                                ControllerConstant::GetInstance()->GetRoleState(RoleState::READY), false, false);
    nodeStatus->UpdateRoleState(33558956,
                                ControllerConstant::GetInstance()->GetRoleState(RoleState::READY), false, false);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "RoleReady");
    EXPECT_EQ(node1->isHealthy, false);
    EXPECT_EQ(node1->isInitialized, false);

    auto ret = ServerRequestHandler::GetInstance()->PostSingleRole(*client, *nodeStatus, *node1);
    EXPECT_EQ(ret, 0);  // 预期返回0表示成功

    auto node2 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node2->roleState, "RoleSwitching");
    EXPECT_EQ(node2->isHealthy, true);
    EXPECT_EQ(node2->isInitialized, true);
}

/*
测试描述: 查询PostSingleRole状态请求成功但响应非200的测试用例
测试步骤:
    1. 打桩httpclient接收到的HTTP响应码为500。
        - HTTP响应码为500
        - 方法返回值为0（表示请求成功且响应码正确）
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. PostSingleRole 方法返回0表示成功。
    2. 节点的信息更新失败:
        - 节点状态为RoleUnknown
        - 节点状态isHealthy为false
        - 节点状态为isHealthy为false
*/
TEST_F(TestServerRequestHandler, TestPostSingleRoleResponse500)
{
    SetCode(500);   // 设置HTTP响应码为500
    SetRet(0);      // 模拟发送请求成功

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    nodeStatus->UpdateRoleState(16781740,
                                ControllerConstant::GetInstance()->GetRoleState(RoleState::READY), true, true);
    nodeStatus->UpdateRoleState(33558956,
                                ControllerConstant::GetInstance()->GetRoleState(RoleState::READY), true, true);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "RoleReady");
    EXPECT_EQ(node1->isHealthy, true);
    EXPECT_EQ(node1->isInitialized, true);

    auto ret = ServerRequestHandler::GetInstance()->PostSingleRole(*client, *nodeStatus, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    auto node2 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node2->roleState, "RoleUnknown");
    EXPECT_EQ(node2->isHealthy, false);
    EXPECT_EQ(node2->isInitialized, false);
}

/*
测试描述: 查询PostSingleRole状态请求失败的测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - HTTP响应码为200
        - 方法返回值为-1（表示请求失败）
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. PostSingleRole 方法返回返回-1表示失败。
    2. 节点的信息更新失败:
        - 节点状态为RoleUnknown
        - 节点状态isHealthy为false
        - 节点状态为isHealthy为false
*/
TEST_F(TestServerRequestHandler, TestPostSingleRoleResponseFailed)
{
    SetCode(200);   // 设置HTTP响应码为200
    SetRet(-1);      // 模拟发送请求成功

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    nodeStatus->UpdateRoleState(16781740,
                                ControllerConstant::GetInstance()->GetRoleState(RoleState::READY), true, true);
    nodeStatus->UpdateRoleState(33558956,
                                ControllerConstant::GetInstance()->GetRoleState(RoleState::READY), true, true);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "RoleReady");
    EXPECT_EQ(node1->isHealthy, true);
    EXPECT_EQ(node1->isInitialized, true);

    auto ret = ServerRequestHandler::GetInstance()->PostSingleRole(*client, *nodeStatus, *node1);
    EXPECT_EQ(ret, -1);  // 预期返回-1表示失败

    auto node2 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node2->roleState, "RoleUnknown");
    EXPECT_EQ(node2->isHealthy, false);
    EXPECT_EQ(node2->isInitialized, false);
}

/*
测试描述: 查询BatchPostRole批量检查，请求前一个成功且响应码为200，后一个状态请求失败的测试用例
测试步骤:
    1. 打桩httpclient接收到的响应前一个为成功, 后一个节点的失败。
        - HTTP响应码为200
        - 方法返回值为0（表示请求成功且响应码正确）
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. PostSingleRole 方法返回0表示成功。
    2. 前一个节点16781740的信息更新成功:
        - 节点状态为RoleUnknown
        - 节点状态isHealthy为true
        - 节点状态为isHealthy为true
    3. 后一个节点33558956的信息更新失败:
        - 节点状态为RoleUnknown
        - 节点状态isHealthy为false
        - 节点状态为isHealthy为false
    4. success中的值只有16781740
*/
TEST_F(TestServerRequestHandler, TestBatchPostRole)
{
    SetCode(200);   // 设置HTTP响应码为200
    SetRet(0);      // 模拟发送请求成功, 但是SendRequestMockMulti的第二次调用返回-1
    SetCount(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMockMulti);

    nodeStatus->UpdateRoleState(16781740,
                                ControllerConstant::GetInstance()->GetRoleState(RoleState::READY), false, false);
    nodeStatus->UpdateRoleState(33558956,
                                ControllerConstant::GetInstance()->GetRoleState(RoleState::READY), true, true);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "RoleReady");
    EXPECT_EQ(node1->isHealthy, false);
    EXPECT_EQ(node1->isInitialized, false);

    auto node2 = nodeStatus->GetNode(33558956);
    EXPECT_EQ(node2->roleState, "RoleReady");
    EXPECT_EQ(node2->isHealthy, true);
    EXPECT_EQ(node2->isInitialized, true);

    std::vector<uint64_t> nodesId;
    std::vector<uint64_t> success;

    nodesId.push_back(16781740);
    nodesId.push_back(33558956);

    ServerRequestHandler::GetInstance()->BatchPostRole(*client, *nodeStatus, nodesId, success);

    auto node3 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node3->roleState, "RoleSwitching");
    EXPECT_EQ(node3->isHealthy, true);
    EXPECT_EQ(node3->isInitialized, true);

    auto node4 = nodeStatus->GetNode(33558956);
    EXPECT_EQ(node4->roleState, "RoleUnknown");
    EXPECT_EQ(node4->isHealthy, false);
    EXPECT_EQ(node4->isInitialized, false);

    for (uint64_t id : success) {
        EXPECT_EQ(id, 16781740);
    }
}

/*
测试描述: 查询CheckStatus批量检查，前一个请求成功且响应码为200，后一个状态请求失败的测试用例
测试步骤:
    1. 打桩httpclient接收到的响应前一个为成功, 后一个节点的失败。
        - HTTP响应码为200
        - 方法返回值为0（表示请求成功且响应码正确）
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. PostSingleRole 方法返回0表示成功。
    2. 前一个节点16781740的信息更新成功:
        - 节点身份为P节点
        - 节点状态为ready
        - 可用Slots为200
        - 可用Block为1024
    3. 后一个节点33558956的信息更新失败:
        - 节点身份为P节点
        - 节点状态为running
        - 可用Slots保持为0
        - 可用Block保持为0
    4. success中的值只有16781740
*/
TEST_F(TestServerRequestHandler, TestCheckStatus)
{
    SetCode(200);   // 设置HTTP响应码为200
    SetRet(0);      // 模拟发送请求成功, 但是SendRequestMockMulti的除第一次外调用返回-1
    SetCount(0);
    SetResponse(RoleStatusEnum::RoleReady, CurrentRoleEnum::prefill, 200, 1024);
    auto controllerJson = GetMSControllerTestJsonPath();

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMockMulti);

    nodeStatus->UpdateRoleState(16781740,
        ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN), false, true);
    nodeStatus->UpdateRoleState(33558956,
        ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN), true, true);

    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "RoleUnknown");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);
    EXPECT_EQ(node1->isHealthy, false);

    auto node2 = nodeStatus->GetNode(33558956);
    EXPECT_EQ(node2->roleState, "RoleUnknown");
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availBlockNum, 0);
    EXPECT_EQ(node2->isHealthy, true);

    std::vector<uint64_t> nodesId;
    std::vector<uint64_t> ret;

    nodesId.push_back(16781740);
    nodesId.push_back(33558956);

    ret = ServerRequestHandler::GetInstance()->CheckStatus(*client, *nodeStatus, nodesId, false);

    auto node3 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node3->roleState, "RoleUnknown");  // 角色状态更新为RoleReady
    EXPECT_EQ(node3->instanceInfo.dynamicInfo.availSlotsNum, 200);  // 可用Slots更新为200
    EXPECT_EQ(node3->instanceInfo.dynamicInfo.availBlockNum, 1024);  // 可用Block更新为1024
    EXPECT_EQ(node3->isHealthy, false);  // isHealthy更新为true

    auto node4 = nodeStatus->GetNode(33558956);
    EXPECT_EQ(node4->roleState, "RoleUnknown");  // 角色状态变更为：RoleUnknown
    EXPECT_EQ(node4->instanceInfo.dynamicInfo.availSlotsNum, 0);  // 可用Slots保持为0
    EXPECT_EQ(node4->instanceInfo.dynamicInfo.availBlockNum, 0);  // 可用Block保持为0
    EXPECT_EQ(node4->isHealthy, false);  // isHealthy更新为false

    for (uint64_t id : ret) {
        EXPECT_EQ(id, 16781740);
    }
}

/*
测试描述: 查询server状态测试用例，返回的数值非法
测试步骤:
    1. 打桩httpclient接收到的响应为成功，但是body体中的键值非法
    2. ServerRequestHandler不更新节点的状态，预期结果1。
预期结果:
    1. UpdateNodeStatus方法返回0表示成功。
    2. 节点的信息更新失败
*/
TEST_F(TestServerRequestHandler, TestInvalidStatus)
{
    SetResponse(RoleStatusEnum::RoleReady, CurrentRoleEnum::decode, -1, 1024);
    SetCode(200);
    SetRet(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    bool isReady = false;
    auto node1 = nodeStatus->GetNode(50336172);
    EXPECT_FALSE(nodeStatus->IsPostRoleNeeded(50336172));
    EXPECT_EQ(node1->roleState, "running");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);

    auto ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, -1);

    node1 = nodeStatus->GetNode(50336172);
    EXPECT_FALSE(nodeStatus->IsPostRoleNeeded(50336172));
    EXPECT_EQ(node1->roleState, "RoleUnknown");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);

    SetResponse(RoleStatusEnum::RoleReady, CurrentRoleEnum::decode, 5001, 1024);
    ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, -1);

    node1 = nodeStatus->GetNode(50336172);
    EXPECT_FALSE(nodeStatus->IsPostRoleNeeded(50336172));
    EXPECT_EQ(node1->roleState, "RoleUnknown");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);

    SetResponse(RoleStatusEnum::RoleReady, CurrentRoleEnum::decode, 5000, -1);
    ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, -1);

    node1 = nodeStatus->GetNode(50336172);
    EXPECT_FALSE(nodeStatus->IsPostRoleNeeded(50336172));
    EXPECT_EQ(node1->roleState, "RoleUnknown");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);

    SetResponse(RoleStatusEnum::RoleReady, CurrentRoleEnum::decode, 5000, 4294967296);
    ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, -1);

    node1 = nodeStatus->GetNode(50336172);
    EXPECT_FALSE(nodeStatus->IsPostRoleNeeded(50336172));
    EXPECT_EQ(node1->roleState, "RoleUnknown");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);
}

/*
测试描述: 下发server身份请求体测试用例，请求体中包含server的宿主机ip和唯一身份标识id
测试步骤:
    1. 打桩httpclient接收到的响应为成功，打桩函数中会校验请求体内容是否合法。
    2. 通过ServerRequestHandler发送身份下发请求，预期结果1。
预期结果:
    1. PostSingleRoleById和PostSingleRoleByVec方法调用成功。
*/
TEST_F(TestServerRequestHandler, TestReqBodyCheck)
{
    SetCode(200);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMockCheckBody);

    auto ret = ServerRequestHandler::GetInstance()->PostSingleRoleById(*client, *nodeStatus, 50336172);
    EXPECT_EQ(ret, 0);

    auto node1 = nodeStatus->GetNode(50336172);
    auto node2 = nodeStatus->GetNode(16781740);
    std::vector<std::unique_ptr<NodeInfo>> nodeVec;
    nodeVec.push_back(std::move(node1));
    nodeVec.push_back(std::move(node2));
    ret = ServerRequestHandler::GetInstance()->PostSingleRoleByVec(*client, nodeVec, *nodeVec[1]);
    EXPECT_EQ(ret, 0);
}

/*
测试描述: 下发server v2版本身份请求体测试用例，请求体中包含server的宿主机ip和唯一身份标识id
测试步骤:
    1. 打桩httpclient接收到的响应为成功，打桩函数中会校验请求体内容是否合法。
    2. 通过ServerRequestHandler发送身份下发请求，预期结果1。
预期结果:
    1. PostSingleRoleById和PostSingleRoleByVec方法调用成功。
*/
TEST_F(TestServerRequestHandler, TestV2ReqBodyCheck)
{
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    ModifyJsonItem(testJson, "multi_node_infer_config", "multi_node_infer_enable", true); // 开启跨级
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();

    int32_t ret = ControllerConfig::GetInstance()->Init();
    SetCode(200);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendV2RequestMockCheckBody);

    ret = ServerRequestHandler::GetInstance()->PostSingleRoleById(*client, *nodeStatus, 50336172);
    EXPECT_EQ(ret, 0);

    auto node1 = nodeStatus->GetNode(50336172);
    auto node2 = nodeStatus->GetNode(16781740);
    std::vector<std::unique_ptr<NodeInfo>> nodeVec;
    nodeVec.push_back(std::move(node1));
    nodeVec.push_back(std::move(node2));
    ret = ServerRequestHandler::GetInstance()->PostSingleRoleByVec(*client, nodeVec, *nodeVec[1]);
    EXPECT_EQ(ret, 0);
}


/*
测试描述: 查询server状态测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - 节点身份为P节点
        - 节点状态为ready
        - 可用Slots为200
        - 可用Block为1024
        - 最大可用block为100
    2. ServerRequestHandler更新节点的状态，预期结果1。
预期结果:
    1. UpdateNodeStatus方法返回0表示成功。
    2. 节点的信息更新成功
        - 节点身份为P节点
        - 节点状态为ready
        - 可用Slots为200
        - 可用Block为1024
        - 最大可用block为100
*/
TEST_F(TestServerRequestHandler, TestServerV2StatusSuccess)
{
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    ModifyJsonItem(testJson, "multi_node_infer_config", "multi_node_infer_enable", true);
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();

    SetV2Response(RoleStatusEnum::RoleReady, CurrentRoleEnum::prefill, 200, 1024, 100);
    SetCode(200);
    SetRet(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    bool isReady = false;
    auto node1 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node1->roleState, "running");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.maxAvailBlockNum, 1024);

    auto ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, 0);

    auto node2 = nodeStatus->GetNode(16781740);
    EXPECT_EQ(node2->roleState, "RoleReady");
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availSlotsNum, 200);
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.availBlockNum, 1024);
    EXPECT_EQ(node2->instanceInfo.dynamicInfo.maxAvailBlockNum, 100);
}

/*
测试描述: 查询server状态测试用例
测试步骤:
    1. 打桩httpclient接收到的响应为成功。
        - 节点身份为P节点
        - 节点状态为ready
        - 可用Slots为200
        - 可用Block为非法字符
        - 最大可用block字段不返回
    2. ServerRequestHandler更新节点的状态，预期结果0。
预期结果:
    1. UpdateNodeStatus方法返回-1。
    2. 节点的信息更新失败。
*/
TEST_F(TestServerRequestHandler, TestServerV2StatusFailed)
{
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    ModifyJsonItem(testJson, "multi_node_infer_config", "multi_node_infer_enable", true);
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();

    SetInvalidV2Response(RoleStatusEnum::RoleReady, CurrentRoleEnum::prefill, 200); // 200: avail block
    SetCode(200);
    SetRet(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    bool isReady = false;
    auto node1 = nodeStatus->GetNode(16781740);

    auto ret = ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, *node1, false, isReady);
    EXPECT_EQ(ret, -1);

    EXPECT_EQ(node1->roleState, "running");
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availSlotsNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.availBlockNum, 0);
    EXPECT_EQ(node1->instanceInfo.dynamicInfo.maxAvailBlockNum, 1024);
}

/*
测试描述: A3场景下发server v2版本身份请求体测试用例，请求体中包含server的宿主机ip和唯一身份标识id
测试步骤:
    1. 打桩httpclient接收到的响应为成功，打桩函数中会校验请求体内容是否合法。
    2. 通过ServerRequestHandler发送身份下发请求，预期结果1。
预期结果:
    1. PostSingleRoleById和PostSingleRoleByVec方法调用成功。
*/
TEST_F(TestServerRequestHandler, TestV2ReqBodyCheckInA3)
{
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    ModifyJsonItem(testJson, "multi_node_infer_config", "multi_node_infer_enable", true); // 开启机器
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();

    int32_t ret = ControllerConfig::GetInstance()->Init();
    SetCode(200);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendV2RequestMockCheckBodyForA3);

    ret = ServerRequestHandler::GetInstance()->PostSingleRoleById(*client, *nodeStatus, 50336172);
    EXPECT_EQ(ret, 0);

    auto node1 = nodeStatus->GetNode(50336172);
    auto node2 = nodeStatus->GetNode(16781740);
    std::vector<std::unique_ptr<NodeInfo>> nodeVec;
    nodeVec.push_back(std::move(node1));
    nodeVec.push_back(std::move(node2));
    ret = ServerRequestHandler::GetInstance()->PostSingleRoleByVec(*client, nodeVec, *nodeVec[1]);
    EXPECT_EQ(ret, 0);
}