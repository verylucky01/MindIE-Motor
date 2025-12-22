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
#include "RequestCoordinatorStub.h"
#include "CoordinatorRequestHandler.h"
using namespace MINDIE::MS;

class TestCoordinatorRequestHandler : public testing::Test {
public:
    void SetUp() override
    {
        InitHttpClient();
        InitNodeStatus();
        InitCoordinatorStore();
        auto controllerJson = GetMSControllerTestJsonPath();
        auto testJson = GetProbeServerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "tls_config", "request_server_tls_enable", false);
        ModifyJsonItem(testJson, "tls_config", "request_coordinator_tls_enable", false);
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << testJson << std::endl;
        ControllerConfig::GetInstance()->Init();
        CoordinatorRequestHandler::GetInstance()->SetRunStatus(true);
    }

    void InitNodeStatus()
    {
        nodeStatus = std::make_shared<NodeStatus>();
        auto nodeInfo1 = std::make_unique<NodeInfo>();
        nodeInfo1->ip = host;
        nodeInfo1->port = port;
        nodeInfo1->interCommPort = "1027";
        nodeInfo1->isHealthy = true;
        nodeInfo1->isInitialized = true;
        nodeInfo1->currentRole = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
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
        auto selectedCoordinator = std::make_unique<Coordinator>();
        selectedCoordinator->ip = coordinatorHost;
        selectedCoordinator->port = coordinatorPort;
        selectedCoordinators.push_back(std::move(selectedCoordinator));
    }

    TlsItems tlsItems;
    std::shared_ptr<NodeStatus> nodeStatus;
    std::shared_ptr<HttpClient> client;
    std::shared_ptr<CoordinatorStore> coordinatorStore;
    std::vector<std::unique_ptr<Coordinator>> selectedCoordinators;
    std::string host;
    std::string port;
    std::string coordinatorHost = "127.0.0.2";
    std::string coordinatorPort = "8080";
};

/*
测试描述: 测试CoordinatorRequestHandler，请求成功
测试步骤:
    1. 打桩HttpClient接收到的响应为成功。
    2. 向Coordinator同步可用的实例信息。
    3. 检查Coordinator接收到的可用实例，为空。
预期结果:
    1. SendNodeStatus方法执行成功。
    2. 查询Coordinator接收到的节点信息:
        - 节点信息为空
*/
TEST_F(TestCoordinatorRequestHandler, TestSendPWithoutAvailableD)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestCoordinatorMock);

    CoordinatorRequestHandler::GetInstance()->SendNodeStatus(client, nodeStatus, coordinatorStore);

    auto coordinators = coordinatorStore->GetCoordinators();
    EXPECT_EQ(coordinators.size(), 1);
    EXPECT_TRUE(coordinators[0]->isHealthy);

    auto instanceStr = GetCoordinatorCurrentInstances();
    auto instancesJson = nlohmann::json::parse(instanceStr);
    EXPECT_TRUE(instancesJson["ids"].empty());
    EXPECT_TRUE(instancesJson["instances"].empty());
}

/*
测试描述: 测试CoordinatorRequestHandler，请求失败
测试步骤:
    1. 打桩HttpClient接收到的响应为失败。
    2. 向Coordinator同步可用的实例信息。
预期结果:
    1. 请求失败时，coordinator的isHealthy字段为false。
*/
TEST_F(TestCoordinatorRequestHandler, TestSendNodeStatusFailed)
{
    SetRequestCoordinatorMockCode(404);  // 模拟请求失败
    SetRequestCoordinatorMockRet(-1);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestCoordinatorMock);

    CoordinatorRequestHandler::GetInstance()->SendNodeStatus(client, nodeStatus, coordinatorStore);

    auto coordinators = coordinatorStore->GetCoordinators();
    EXPECT_EQ(coordinators.size(), 1);
    EXPECT_FALSE(coordinators[0]->isHealthy);
}

/*
测试描述: 测试CoordinatorRequestHandler，响应为成功但非200
测试步骤:
    1. 打桩HttpClient接收到的响应为成功但非200。
    2. 向Coordinator同步可用的实例信息。
预期结果:
    1. 响应非200时，coordinator的isHealthy字段为false。
*/
TEST_F(TestCoordinatorRequestHandler, TestSendNodeStatusNon200Response)
{
    SetRequestCoordinatorMockCode(500);  // 模拟返回500响应
    SetRequestCoordinatorMockRet(0);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestCoordinatorMock);

    CoordinatorRequestHandler::GetInstance()->SendNodeStatus(client, nodeStatus, coordinatorStore);

    auto coordinators = coordinatorStore->GetCoordinators();
    EXPECT_EQ(coordinators.size(), 1);
    EXPECT_FALSE(coordinators[0]->isHealthy);
}

/*
测试描述: 测试PD分离场景下，CoordinatorRequestHandler同步的集群信息中携带inter_comm_port字段
测试步骤:
    1. 打桩HttpClient接收到的响应为成功。
    2. 向Coordinator同步实例信息。
    3. 检查集群节点信息同步请求体，预期结果1。
预期结果:
    1. 请求体中包含符合预期的inter_comm_port字段。
*/
TEST_F(TestCoordinatorRequestHandler, TestRequestBodyHasInterCommPort)
{
    SetRequestCoordinatorMockCode(200);  // 模拟返回200状态
    SetRequestCoordinatorMockRet(0);

    // 添加一个D实例信息，保证集群中为1P1D，集群可用
    auto nodeInfo = std::make_unique<NodeInfo>();
    nodeInfo->ip = "127.0.0.3";
    nodeInfo->port = port;
    nodeInfo->interCommPort = "1027"; // PD的集群间通信端口设为相同方便检查
    nodeInfo->isHealthy = true;
    nodeInfo->isInitialized = true;
    nodeInfo->currentRole = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    nodeInfo->roleState = "RoleReady";
    nodeInfo->modelName = "model1";
    nodeInfo->peers = { 16781740 }; // 原有的P实例
    nodeInfo->activePeers = { 16781740 };
    nodeInfo->instanceInfo.staticInfo.id = 33558956;
    nodeInfo->instanceInfo.staticInfo.groupId = 1;
    nodeInfo->instanceInfo.staticInfo.maxSeqLen = 2048;
    nodeInfo->instanceInfo.staticInfo.maxOutputLen = 512;
    nodeInfo->instanceInfo.staticInfo.totalSlotsNum = 200;
    nodeInfo->instanceInfo.staticInfo.totalBlockNum = 1024;
    nodeInfo->instanceInfo.staticInfo.blockSize = 128;
    nodeInfo->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER;
    nodeInfo->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
    nodeInfo->instanceInfo.dynamicInfo.availSlotsNum = 0;
    nodeInfo->instanceInfo.dynamicInfo.availBlockNum = 0;
    nodeInfo->instanceInfo.dynamicInfo.waitingRequestNum = 0;
    nodeInfo->instanceInfo.dynamicInfo.runningRequestNum = 0;
    nodeInfo->instanceInfo.dynamicInfo.swappedRequestNum = 0;
    nodeInfo->instanceInfo.dynamicInfo.freeNpuBlockNums = 0;
    nodeInfo->instanceInfo.dynamicInfo.freeCpuBlockNums = 0;
    nodeInfo->instanceInfo.dynamicInfo.totalNpuBlockNums = 0;
    nodeInfo->instanceInfo.dynamicInfo.totalCpuBlockNums = 0;
    nodeStatus->AddNode(std::move(nodeInfo));

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestCoordinatorMock);

    CoordinatorRequestHandler::GetInstance()->SendNodeStatus(client, nodeStatus, coordinatorStore);

    auto instanceStr = GetCoordinatorCurrentInstances();
    auto instancesJson = nlohmann::json::parse(instanceStr);
    EXPECT_EQ(instancesJson["ids"].size(), 2);
    EXPECT_EQ(instancesJson["instances"].size(), 2);
    EXPECT_TRUE(instancesJson["instances"][0].contains("inter_comm_port"));
    EXPECT_EQ(instancesJson["instances"][0].at("inter_comm_port").get<std::string>(), "1027");
}

/*
测试描述: 测试CoordinatorRequestHandler，请求成功
测试步骤:
    1. 打桩HttpClient接收到的响应为成功。
    2. 向Coordinator同步可用的实例信息。
    3. 检查Coordinator接收到的可用实例，为空。
预期结果:
    1. SendNodeStatus方法执行成功。
    2. 查询Coordinator接收到的节点信息:
        - 节点信息为空
*/
TEST_F(TestCoordinatorRequestHandler, TestSendMetricsSuccess)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::SUCCESS);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] Metric information is invalid.") == std::string::npos);
}

TEST_F(TestCoordinatorRequestHandler, TestSendMetricsEmptyResponse)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::EMPTY);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] Response is empty") != std::string::npos);
}

TEST_F(TestCoordinatorRequestHandler, TestSendMetricsValueIsNegativeTC01)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::INVALIDCOUTERVALUE);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] The value of counter metric") != std::string::npos);
    EXPECT_TRUE(logOutput.find("which is negative") != std::string::npos);
}

TEST_F(TestCoordinatorRequestHandler, TestSendMetricsValueIsNegativeTC02)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::INVALIDHISTOGRAMVALUE);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] The value of histogram bucket") != std::string::npos);
    EXPECT_TRUE(logOutput.find("which is negative") != std::string::npos);
}

TEST_F(TestCoordinatorRequestHandler, TestSendMetricsValueIsNegativeTC03)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::INVALIDGAUGEVALUE);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] The value of gauge metric") != std::string::npos);
    EXPECT_TRUE(logOutput.find("which is negative") != std::string::npos);
}

TEST_F(TestCoordinatorRequestHandler, TestSendMetricsFormatIsInvalidTC01)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::INVALIDHELPFORMAT);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] The format of # HELP line") != std::string::npos);
    EXPECT_TRUE(logOutput.find("is invalid.") != std::string::npos);
}

TEST_F(TestCoordinatorRequestHandler, TestSendMetricsFormatIsInvalidTC02)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::INVALIDTYPE);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] The metric type of line") != std::string::npos);
    EXPECT_TRUE(logOutput.find("is invalid.") != std::string::npos);
}

TEST_F(TestCoordinatorRequestHandler, TestSendMetricsFormatIsInvalidTC03)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::INVALIDTYPEFORMAT);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] The format of # TYPE line") != std::string::npos);
    EXPECT_TRUE(logOutput.find("is invalid.") != std::string::npos);
}

TEST_F(TestCoordinatorRequestHandler, TestSendMetricsFormatIsInvalidTC04)
{
    SetRequestCoordinatorMockCode(200);
    SetRequestCoordinatorMockRet(0);
    SetRequestMetricsMockCode(MetricsCode::INVALIDDATAFORMAT);

    std::string response;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendMetricRequestCoordinatorMock);

    testing::internal::CaptureStdout();
    CoordinatorRequestHandler::GetInstance()->SendMetricsRequest(
        client, coordinatorStore, selectedCoordinators, response);

    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(logOutput.find("[CoordinatorRequestHandler] Line") != std::string::npos);
    EXPECT_TRUE(logOutput.find("contains metrics data before # HELP or # TYPE line.") != std::string::npos);
}