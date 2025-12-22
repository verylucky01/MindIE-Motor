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

#include "ControllerConfig.h"
#include "stub.h"
#include "gtest/gtest.h"
#include "Helper.h"
#include "RequestCCAEStub.h"
#include "CCAERequestHandler.h"
#include <gmock/gmock.h>
#include "NodeStatus.h"

using namespace MINDIE::MS;

class TestCCAERequestHandler : public testing::Test {
public:
    void SetUp() override
    {
        InitHttpClient();
        CopyDefaultConfig();
        auto controllerJson = GetMSControllerTestJsonPath();
        auto testJson = GetProbeServerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "tls_config", "request_server_tls_enable", false);
        ModifyJsonItem(testJson, "tls_config", "request_coordinator_tls_enable", false);
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << testJson << std::endl;
        ControllerConfig::GetInstance()->Init();
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
    std::shared_ptr<HttpClient> client;
    std::string host;
    std::string port;
    std::string CCAEHost = "127.0.0.2";
    std::string CCAEPort = "8080";
};

/*
测试描述: 测试CCAERequestHandler，请求成功
测试步骤:
    1. 打桩HttpClient接收到的响应为成功。
    2. 向CCAE同步可用的实例信息。
    3. 检查CCAE接收到的可用实例，为空。
预期结果:
    1. SendNodeStatus方法执行成功。
    2. 查询CCAE接收到的节点信息:
        - 节点信息为空
*/
TEST_F(TestCCAERequestHandler, TestSendRegisterSuccess)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::SUCCESS);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    EXPECT_EQ(ret, 0);
}

TEST_F(TestCCAERequestHandler, TestRegisterResponseInvalidTC01)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::INVALIDRESPONSETYPE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] Invalid CCAE register response") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestRegisterResponseInvalidTC02)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::INVALIDRETCODETYPE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] Invalid CCAE register response ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("return code or message data type") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestRegisterResponseInvalidTC03)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::INVALIDRETMSGTYPE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] Invalid CCAE register response ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("return code or message data type") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestRegisterResponseInvalidTC04)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::ERRORCODE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] CCAE register returns error code 1 ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("and error message:") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestRegisterResponseInvalidTC05)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::INVALIDREQLISTTYPE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] CCAE register dose not ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("return with valid request list.") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestRegisterResponseInvalidTC06)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::INVALIDMODELID);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] CCAE register dose not ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("return with valid modelID.") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestRegisterResponseInvalidTC07)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::INVALIDMETRICPERIOD);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] CCAE register dose not ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("return with valid metrics.") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestRegisterResponseInvalidTC08)
{
    SetRequestRegisterMockCode(200); // Set code is 200;
    SetRequestRegisterMockRet(0);
    SetRequestRegisterMockCode(RegisterCode::INVALIDFORCEUPDATE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestRegisterMock);

    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendRegister2UpdateStatus(*client, *mCCAEStatus);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] CCAE register dose not ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("return with valid inventories.") != std::string::npos);
}

ServerInfo buildServer1()
{
    ServerInfo serverInfo1;
    serverInfo1.hostId = "192.168.88.1";
    serverInfo1.ip = "192.168.88.2";
    DeviceInfo deviceInfo1;
    deviceInfo1.id = "0";
    deviceInfo1.ip = "192.168.88.3";
    deviceInfo1.logicalId = "0";
    deviceInfo1.rankId = 0;
    serverInfo1.deviceInfos.emplace_back(deviceInfo1);
    DeviceInfo deviceInfo2;
    deviceInfo2.id = "0";
    deviceInfo2.ip = "192.168.88.4";
    deviceInfo2.logicalId = "0";
    deviceInfo2.rankId = 1;
    serverInfo1.deviceInfos.emplace_back(deviceInfo2);
    std::vector<DeviceInfo> deviceInfosList {deviceInfo1, deviceInfo2};
    serverInfo1.dpGroupInfos[0] = deviceInfosList;
    return serverInfo1;
}

ServerInfo buildServer2()
{
    ServerInfo serverInfo2;
    serverInfo2.hostId = "192.168.88.5";
    serverInfo2.ip = "192.168.88.6";
    DeviceInfo deviceInfo3;
    deviceInfo3.id = "0";
    deviceInfo3.ip = "192.168.88.7";
    deviceInfo3.logicalId = "0";
    deviceInfo3.rankId = 0;
    serverInfo2.deviceInfos.emplace_back(deviceInfo3);
    DeviceInfo deviceInfo4;
    deviceInfo4.id = "0";
    deviceInfo4.ip = "192.168.88.8";
    deviceInfo4.logicalId = "0";
    deviceInfo4.rankId = 1;
    serverInfo2.deviceInfos.emplace_back(deviceInfo4);
    std::vector<DeviceInfo> deviceInfosList2 {deviceInfo3, deviceInfo4};
    serverInfo2.dpGroupInfos[0] = deviceInfosList2;
    return serverInfo2;
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
    nodeInfo1->instanceInfo.staticInfo.virtualId = 16781740; // 16781740: virtualId
    nodeInfo1->instanceInfo.staticInfo.groupId = 1;
    nodeInfo1->instanceInfo.staticInfo.maxSeqLen = 2048;
    nodeInfo1->instanceInfo.staticInfo.maxOutputLen = 512;
    nodeInfo1->instanceInfo.staticInfo.totalSlotsNum = 200;
    nodeInfo1->instanceInfo.staticInfo.totalBlockNum = 1024;
    nodeInfo1->instanceInfo.staticInfo.blockSize = 128;
    nodeInfo1->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER;
    nodeInfo1->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;

    nodeInfo1->serverInfoList.emplace_back(buildServer1());
    nodeInfo1->serverInfoList.emplace_back(buildServer2());

    nodeInfo2->ip = "172.17.0.2";
    nodeInfo2->port = "8081";
    nodeInfo2->isHealthy = true;
    nodeInfo2->isInitialized = false;
    nodeInfo2->roleState = "RoleReady";
    nodeInfo2->modelName = "model2";
    nodeInfo2->peers = { 16781740 };
    nodeInfo2->activePeers = { 16781740 };
    nodeInfo2->instanceInfo.staticInfo.id = 33558956;
    nodeInfo2->instanceInfo.staticInfo.virtualId = 33558956; // 33558956: virtualId
    nodeInfo2->instanceInfo.staticInfo.groupId = 2;
    nodeInfo2->instanceInfo.staticInfo.maxSeqLen = 2048;
    nodeInfo2->instanceInfo.staticInfo.maxOutputLen = 512;
    nodeInfo2->instanceInfo.staticInfo.totalSlotsNum = 300;
    nodeInfo2->instanceInfo.staticInfo.totalBlockNum = 2048;
    nodeInfo2->instanceInfo.staticInfo.blockSize = 256;
    nodeInfo2->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER;
    nodeInfo2->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
}

TEST_F(TestCCAERequestHandler, TestSendInventoriesSuccess)
{
    SetRequestInventoriesMockCode(201); // Set code is 201
    SetRequestInventoriesMockRet(0);
    SetRequestInventoriesMockCode(InventoriesCode::SUCCESS);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestInventoriesMock);

    ControllerConfig::GetInstance()->Init();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    auto nodeInfo2 = std::make_unique<NodeInfo>();
    SetNodeInfo(nodeInfo1, nodeInfo2);

    std::vector<std::unique_ptr<NodeInfo>> nodeVec01;
    nodeVec01.push_back(std::move(nodeInfo1));
    nodeStatus->AddNodes(nodeVec01);
    nodeStatus->AddNode(std::move(nodeInfo2));
    auto tempMap = nodeStatus->GetAllNodes();
    EXPECT_EQ(tempMap.size(), 2);

    std::vector<std::string> mModelIDStack {"1000"};
    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();
    std::shared_ptr<NodeStatus> mNodeStatus = std::make_shared<NodeStatus>();
    std::string response = R"(
        # HELP prompt_tokens_total Number of prefill tokens processed.
        # TYPE prompt_tokens_total counter
        prompt_tokens_total{model_name="llama2-7b"} 9564
        # HELP avg_prompt_throughput_toks_per_s Average prefill throughput in tokens/s.
        # TYPE avg_prompt_throughput_toks_per_s gauge
        avg_prompt_throughput_toks_per_s{model_name="llama2-7b"} 0.586739718914032
        # HELP time_to_first_token_seconds Histogram of time to first token in seconds.
        # TYPE time_to_first_token_seconds histogram
        time_to_first_token_seconds_count{model_name="llama2-7b"} 2523
        time_to_first_token_seconds_sum{model_name="llama2-7b"} 9740.00200343132
        time_to_first_token_seconds_bucket{model_name="llama2-7b", le="0.001"} 0
    )";
    testing::internal::CaptureStdout();
    std::string jsonString =
        CCAERequestHandler::GetInstance()->FillInventoryRequest(mModelIDStack, mCCAEStatus,
            mNodeStatus, response);
    int32_t ret = CCAERequestHandler::GetInstance()->SendInventories(*client, jsonString);
    std::string logOutput = testing::internal::GetCapturedStdout();
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(logOutput.find("CCAE inventories return success") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestInventoriesResponseInvalidTC01)
{
    SetRequestInventoriesMockCode(201); // Set code is 201
    SetRequestInventoriesMockRet(0);
    SetRequestInventoriesMockCode(InventoriesCode::INVALIDRESPONSETYPE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestInventoriesMock);

    std::vector<std::string> mModelIDStack {};
    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();
    std::shared_ptr<NodeStatus> mNodeStatus = std::make_shared<NodeStatus>();
    std::string response = R"(
        # HELP prompt_tokens_total Number of prefill tokens processed.
        # TYPE prompt_tokens_total counter
        prompt_tokens_total{model_name="llama2-7b"} 9564
        # HELP avg_prompt_throughput_toks_per_s Average prefill throughput in tokens/s.
        # TYPE avg_prompt_throughput_toks_per_s gauge
        avg_prompt_throughput_toks_per_s{model_name="llama2-7b"} 0.586739718914032
        # HELP time_to_first_token_seconds Histogram of time to first token in seconds.
        # TYPE time_to_first_token_seconds histogram
        time_to_first_token_seconds_count{model_name="llama2-7b"} 2523
        time_to_first_token_seconds_sum{model_name="llama2-7b"} 9740.00200343132
        time_to_first_token_seconds_bucket{model_name="llama2-7b", le="0.001"} 0
    )";
    std::string jsonString =
        CCAERequestHandler::GetInstance()->FillInventoryRequest(mModelIDStack, mCCAEStatus,
            mNodeStatus, response);

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendInventories(*client, jsonString);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] Invalid CCAE inventories response") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestInventoriesResponseInvalidTC02)
{
    SetRequestInventoriesMockCode(201); // Set code is 201
    SetRequestInventoriesMockRet(0);
    SetRequestInventoriesMockCode(InventoriesCode::INVALIDRETCODETYPE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestInventoriesMock);

    std::vector<std::string> mModelIDStack {};
    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();
    std::shared_ptr<NodeStatus> mNodeStatus = std::make_shared<NodeStatus>();
    
    std::string response = R"(
        # HELP prompt_tokens_total Number of prefill tokens processed.
        # TYPE prompt_tokens_total counter
        prompt_tokens_total{model_name="llama2-7b"} 9564
        # HELP avg_prompt_throughput_toks_per_s Average prefill throughput in tokens/s.
        # TYPE avg_prompt_throughput_toks_per_s gauge
        avg_prompt_throughput_toks_per_s{model_name="llama2-7b"} 0.586739718914032
        # HELP time_to_first_token_seconds Histogram of time to first token in seconds.
        # TYPE time_to_first_token_seconds histogram
        time_to_first_token_seconds_count{model_name="llama2-7b"} 2523
        time_to_first_token_seconds_sum{model_name="llama2-7b"} 9740.00200343132
        time_to_first_token_seconds_bucket{model_name="llama2-7b", le="0.001"} 0
    )";
    std::string jsonString =
        CCAERequestHandler::GetInstance()->FillInventoryRequest(mModelIDStack, mCCAEStatus,
            mNodeStatus, response);

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendInventories(*client, jsonString);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] Invalid CCAE inventories response ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("return code or message data type") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestInventoriesResponseInvalidTC03)
{
    SetRequestInventoriesMockCode(201); // Set code is 201
    SetRequestInventoriesMockRet(0);
    SetRequestInventoriesMockCode(InventoriesCode::INVALIDRETMSGTYPE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestInventoriesMock);

    std::vector<std::string> mModelIDStack {};
    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();
    std::shared_ptr<NodeStatus> mNodeStatus = std::make_shared<NodeStatus>();
    std::string response = R"(
        # HELP prompt_tokens_total Number of prefill tokens processed.
        # TYPE prompt_tokens_total counter
        prompt_tokens_total{model_name="llama2-7b"} 9564
        # HELP avg_prompt_throughput_toks_per_s Average prefill throughput in tokens/s.
        # TYPE avg_prompt_throughput_toks_per_s gauge
        avg_prompt_throughput_toks_per_s{model_name="llama2-7b"} 0.586739718914032
        # HELP time_to_first_token_seconds Histogram of time to first token in seconds.
        # TYPE time_to_first_token_seconds histogram
        time_to_first_token_seconds_count{model_name="llama2-7b"} 2523
        time_to_first_token_seconds_sum{model_name="llama2-7b"} 9740.00200343132
        time_to_first_token_seconds_bucket{model_name="llama2-7b", le="0.001"} 0
    )";
    std::string jsonString =
        CCAERequestHandler::GetInstance()->FillInventoryRequest(mModelIDStack, mCCAEStatus,
            mNodeStatus, response);

    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendInventories(*client, jsonString);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] Invalid CCAE inventories response ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("return code or message data type") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestInventoriesResponseInvalidTC04)
{
    SetRequestInventoriesMockCode(201); // Set code is 201
    SetRequestInventoriesMockRet(0);
    SetRequestInventoriesMockCode(InventoriesCode::ERRORCODE);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestInventoriesMock);

    std::vector<std::string> mModelIDStack {};
    std::shared_ptr<CCAEStatus> mCCAEStatus = std::make_shared<CCAEStatus>();
    std::shared_ptr<NodeStatus> mNodeStatus = std::make_shared<NodeStatus>();
    std::string response = R"(
        # HELP prompt_tokens_total Number of prefill tokens processed.
        # TYPE prompt_tokens_total counter
        prompt_tokens_total{model_name="llama2-7b"} 9564
        # HELP avg_prompt_throughput_toks_per_s Average prefill throughput in tokens/s.
        # TYPE avg_prompt_throughput_toks_per_s gauge
        avg_prompt_throughput_toks_per_s{model_name="llama2-7b"} 0.586739718914032
        # HELP time_to_first_token_seconds Histogram of time to first token in seconds.
        # TYPE time_to_first_token_seconds histogram
        time_to_first_token_seconds_count{model_name="llama2-7b"} 2523
        time_to_first_token_seconds_sum{model_name="llama2-7b"} 9740.00200343132
        time_to_first_token_seconds_bucket{model_name="llama2-7b", le="0.001"} 0
    )";
    std::string jsonString =
        CCAERequestHandler::GetInstance()->FillInventoryRequest(mModelIDStack, mCCAEStatus,
            mNodeStatus, response);
    
    testing::internal::CaptureStdout();
    int32_t ret = CCAERequestHandler::GetInstance()->SendInventories(*client, jsonString);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(ret, -1);
    EXPECT_TRUE(logOutput.find("[CCAERequestHandler] CCAE inventories return error code 1 ") != std::string::npos);
    EXPECT_TRUE(logOutput.find("and error message: failed") != std::string::npos);
}

TEST_F(TestCCAERequestHandler, TestGetInstanceInfo)
{
    std::string pdFlag = "p";
    std::string serverId = "192.168.88.1";
    auto result1 = CCAERequestHandler::GetInstance()->GetInstanceInfo(pdFlag, serverId);
    EXPECT_EQ(result1["ID"], "p_192.168.88.1");
    
    nlohmann::json instanceInfo;
    instanceInfo["ID"] = "p_192.168.88.1";
    instanceInfo["Name"] = "p_192.168.88.1";
    CCAERequestHandler::GetInstance()->mInstanceMap[pdFlag][serverId] = instanceInfo;
    auto result2 = CCAERequestHandler::GetInstance()->GetInstanceInfo(pdFlag, serverId);
    EXPECT_EQ(result2["ID"], "p_192.168.88.1");
    
    CCAERequestHandler::GetInstance()->mInstanceMap.clear();
}

TEST_F(TestCCAERequestHandler, TestGetInstances)
{
    auto result1 = CCAERequestHandler::GetInstance()->GetInstances("nonexistent");
    EXPECT_TRUE(result1.empty());

    nlohmann::json instance1, instance2;
    instance1["ID"] = "p_server1";
    instance1["Name"] = "instance1";
    instance2["ID"] = "p_server2";
    instance2["Name"] = "instance2";
    
    CCAERequestHandler::GetInstance()->mInstanceMap["p"]["server1"] = instance1;
    CCAERequestHandler::GetInstance()->mInstanceMap["p"]["server2"] = instance2;
    
    auto result2 = CCAERequestHandler::GetInstance()->GetInstances("p");
    EXPECT_EQ(result2.size(), 2);
    
    bool found1 = false, found2 = false;
    for (const auto& instance : result2) {
        if (instance["Name"] == "instance1") found1 = true;
        if (instance["Name"] == "instance2") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
    
    CCAERequestHandler::GetInstance()->mInstanceMap.clear();
    CCAERequestHandler::GetInstance()->mInstanceMap.clear();
}

TEST_F(TestCCAERequestHandler, TestBuildCentralizedInstances)
{
    NodeInfo nodeInfo;
    nodeInfo.hostId = "192.168.88.1";
    
    ServerInfo serverInfo1;
    serverInfo1.hostId = "192.168.88.1";
    serverInfo1.ip = "192.168.88.2";
    
    DeviceInfo deviceInfo1;
    deviceInfo1.id = "0";
    deviceInfo1.ip = "192.168.88.3";
    deviceInfo1.logicalId = "0";
    deviceInfo1.rankId = 0;
    serverInfo1.deviceInfos.push_back(deviceInfo1);
    
    nodeInfo.serverInfoList.push_back(serverInfo1);

    nlohmann::json instanceInfo;
    instanceInfo["ID"] = "p_192.168.88.1";
    instanceInfo["Name"] = "p_192.168.88.1";
    instanceInfo["serverList"] = nlohmann::json::array();
    instanceInfo["serverIPList"] = nlohmann::json::array();
    instanceInfo["podInfoList"] = nlohmann::json::array();
    
    std::string serverId = "192.168.88.1";
    CCAERequestHandler::GetInstance()->BuildCentralizedInstances(instanceInfo, "p", serverId, nodeInfo);

    EXPECT_TRUE(CCAERequestHandler::GetInstance()->mInstanceMap.find("p") !=
            CCAERequestHandler::GetInstance()->mInstanceMap.end());
    EXPECT_TRUE(CCAERequestHandler::GetInstance()->mInstanceMap["p"].find(serverId) !=
            CCAERequestHandler::GetInstance()->mInstanceMap["p"].end());
    EXPECT_TRUE(CCAERequestHandler::GetInstance()->mPodToInstanceMap.find("192.168.88.2") !=
            CCAERequestHandler::GetInstance()->mPodToInstanceMap.end());
    EXPECT_EQ(CCAERequestHandler::GetInstance()->mPodToInstanceMap["192.168.88.2"], "p_192.168.88.1");

    CCAERequestHandler::GetInstance()->mInstanceMap.clear();
    CCAERequestHandler::GetInstance()->mPodToInstanceMap.clear();
}

TEST_F(TestCCAERequestHandler, TestBuildDistributeInstance)
{
    std::string serverIP = "192.168.88.1";
    std::string logicId = "1";
    CCAERequestHandler::GetInstance()->mServerIPToInstLogicIDMap[serverIP] = logicId;

    NodeInfo nodeInfo;
    nodeInfo.hostId = serverIP;
    
    ServerInfo serverInfo1;
    serverInfo1.hostId = serverIP;
    serverInfo1.ip = "192.168.88.2";
    
    DeviceInfo deviceInfo1;
    deviceInfo1.id = "0";
    deviceInfo1.ip = "192.168.88.3";
    deviceInfo1.logicalId = "0";
    deviceInfo1.rankId = 0;
    serverInfo1.deviceInfos.push_back(deviceInfo1);
    nodeInfo.serverInfoList.push_back(serverInfo1);

    nlohmann::json instanceInfo;
    instanceInfo["ID"] = "d_1";
    instanceInfo["Name"] = "d_1";
    instanceInfo["serverList"] = nlohmann::json::array();
    instanceInfo["serverIPList"] = nlohmann::json::array();
    instanceInfo["podInfoList"] = nlohmann::json::array();

    CCAERequestHandler::GetInstance()->BuildDistributeInstance(instanceInfo, "d", serverIP, nodeInfo);
    
    EXPECT_TRUE(CCAERequestHandler::GetInstance()->mInstanceMap.find("d") !=
            CCAERequestHandler::GetInstance()->mInstanceMap.end());
    EXPECT_TRUE(CCAERequestHandler::GetInstance()->mInstanceMap["d"].find(logicId) !=
            CCAERequestHandler::GetInstance()->mInstanceMap["d"].end());
    EXPECT_TRUE(CCAERequestHandler::GetInstance()->mPodToInstanceMap.find("192.168.88.2") !=
            CCAERequestHandler::GetInstance()->mPodToInstanceMap.end());
    EXPECT_EQ(CCAERequestHandler::GetInstance()->mPodToInstanceMap["192.168.88.2"], "d_1");

    CCAERequestHandler::GetInstance()->mInstanceMap.clear();
    CCAERequestHandler::GetInstance()->mPodToInstanceMap.clear();
    CCAERequestHandler::GetInstance()->mPodMap.clear();
    CCAERequestHandler::GetInstance()->mServerIPToInstLogicIDMap.clear();
}