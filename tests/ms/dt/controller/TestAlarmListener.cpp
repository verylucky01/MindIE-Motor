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
#include <nlohmann/json.hpp>

#include <gmock/gmock.h>
#include "gtest/gtest.h"
#include "ControllerConfig.h"
#include "ConfigParams.h"
#include "stub.h"
#include "Helper.h"
#include "AlarmListener.h"
#include "AlarmConfig.h"
#include "CCAERequestHandler.h"
#include "ServerRequestHandler.h"
#include "NodeStatus.h"


using namespace MINDIE::MS;

class TestAlarmListener : public testing::Test {
public:
    void SetUp() override
    {
        CopyDefaultConfig();
        auto controllerJson = GetMSControllerTestJsonPath();
        auto testJson = GetProbeServerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "tls_config", "request_server_tls_enable", false);
        ModifyJsonItem(testJson, "tls_config", "request_coordinator_tls_enable", false);
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << testJson << std::endl;
        ControllerConfig::GetInstance()->Init();
        
        alarmListener = std::make_unique<AlarmListener>();
    }

    void TearDown() override
    {
        alarmListener.reset();
    }

    std::unique_ptr<AlarmListener> alarmListener;
};

/*
测试描述: 测试UpdateCoordinatorStatus，传入空的alarmBody
测试步骤:
    1. 调用UpdateCoordinatorStatus方法，传入空字符串
    2. 检查返回值
预期结果:
    1. 函数返回false
    2. 日志中包含相应的错误信息
*/
TEST_F(TestAlarmListener, TestUpdateCoordinatorStatusWithEmptyBody)
{
    std::string emptyAlarmBody = "";
    
    testing::internal::CaptureStdout();
    bool result = alarmListener->UpdateCoordinatorStatus(emptyAlarmBody);
    std::string logOutput = testing::internal::GetCapturedStdout();

    EXPECT_EQ(result, false);
    EXPECT_TRUE(logOutput.find("Coordinator alarm request body is invalid") != std::string::npos);
}

/*
测试描述: 测试UpdateCoordinatorStatus，处理COORDINATOR_EXCEPTION告警的正常流程
测试步骤:
    1. 打桩AlarmConfig::GetAlarmIDString方法返回固定的告警ID
    2. 打桩CCAERequestHandler::SetCoordinatorServiceReady方法
    3. 构造包含COORDINATOR_EXCEPTION告警的JSON数据
    4. 调用UpdateCoordinatorStatus方法
    5. 检查返回值和相关状态更新
预期结果:
    1. 第一次调用时，由于首次过滤机制，函数返回false
    2. CCAERequestHandler::SetCoordinatorServiceReady被正确调用
    3. 日志输出正常
*/
TEST_F(TestAlarmListener, TestUpdateCoordinatorStatusWithCoordinatorException)
{
    // 构造测试用的JSON数据
    nlohmann::json testAlarm = nlohmann::json::array();
    nlohmann::json alarmRecord;
    alarmRecord["alarmId"] = "COORDINATOR_EXCEPTION_ALARM";
    alarmRecord["cleared"] = 0; // 0表示故障，1表示恢复
    testAlarm.push_back(alarmRecord);
    std::string alarmBody = testAlarm.dump();

    // 打桩AlarmConfig::GetAlarmIDString方法
    Stub stub1;
    stub1.set(
        ADDR(AlarmConfig, GetAlarmIDString),
        +[](void*, AlarmType) -> std::string {
            return "COORDINATOR_EXCEPTION_ALARM";
        });

    // 打桩CCAERequestHandler::SetCoordinatorServiceReady方法
    static bool coordinatorServiceReadySet = false;
    static bool coordinatorServiceReadyValue = true;
    Stub stub2;
    stub2.set(
        ADDR(CCAERequestHandler, SetCoordinatorServiceReady),
        +[](bool ready) -> void {
            coordinatorServiceReadySet = true;
            coordinatorServiceReadyValue = ready;
        });

    testing::internal::CaptureStdout();
    bool result = alarmListener->UpdateCoordinatorStatus(alarmBody);
    std::string logOutput = testing::internal::GetCapturedStdout();

    // 由于是首次COORDINATOR_EXCEPTION故障告警，应该被过滤掉
    EXPECT_EQ(result, false);
    
    // 验证CCAERequestHandler::SetCoordinatorServiceReady被调用，且参数为false（因为cleared=0）
    EXPECT_TRUE(coordinatorServiceReadySet);
    EXPECT_EQ(coordinatorServiceReadyValue, false);
    
    // 测试第二次调用同样的告警，此时应该不被过滤
    coordinatorServiceReadySet = false;
    result = alarmListener->UpdateCoordinatorStatus(alarmBody);
    
    // 第二次调用应该返回true（不被过滤）
    EXPECT_EQ(result, true);
    EXPECT_TRUE(coordinatorServiceReadySet);
    EXPECT_EQ(coordinatorServiceReadyValue, false);
}

TEST_F(TestAlarmListener, TestTerminateServiceHandler)
{
    Stub stub;
    stub.set(
        ADDR(ServerRequestHandler, TerminateService),
        +[](HttpClient &client, NodeInfo &nodeInfo) -> int32_t {
            LOG_I("[TestTerminateServiceHandler] Terminate service succeed, node address %s:%s",
                nodeInfo.ip.c_str(), nodeInfo.port.c_str());
            return 0;
        });

    auto nodeStatus = std::make_shared<NodeStatus>();
    // 添加一个节点到NodeStatus中，IP和端口与测试数据匹配
    auto nodeInfo = std::make_unique<NodeInfo>();
    nodeInfo->ip = "127.0.0.1";
    nodeInfo->port = "2021";
    nodeStatus->AddNode(std::move(nodeInfo));

    alarmListener->Init(nodeStatus);

    // 构造一个req
    boost::beast::http::request<boost::beast::http::string_body> req;
    req.body() = R"({"ip":"127.0.0.1","port":"2021"})";
    req.prepare_payload();

    testing::internal::CaptureStdout();

    std::pair<ErrorCode, Response> result = alarmListener->TerminateServiceHandler(req);
    std::string logOutput = testing::internal::GetCapturedStdout();
    EXPECT_EQ(result.first, ErrorCode::OK);
    EXPECT_TRUE(logOutput.find("Terminate service succeed, node address 127.0.0.1:2021") != std::string::npos);
}