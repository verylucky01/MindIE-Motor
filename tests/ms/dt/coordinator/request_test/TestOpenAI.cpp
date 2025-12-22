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
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include "gtest/gtest.h"
#define main __main_coordinator__
#include "coordinator/main.cpp"
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "ThreadSafeVector.h"
#include "stub.h"


using namespace MINDIE::MS;

class TestOpenAI : public testing::Test {
    void SetUp()
    {
        CopyDefaultConfig();
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        manager.SetList({"http_config", "status_port"}, statusPort);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Save();
    }
    uint8_t numMindIEServer = 4;
    ThreadSafeVector<std::string> predictIPList;
    ThreadSafeVector<std::string> predictPortList;
    ThreadSafeVector<std::string> managePortList;
    ThreadSafeVector<std::string> interCommonPortLists;
    std::string manageIP = "127.0.0.1";
    std::string managePort = "1026";
    std::string predictIP = "127.0.0.1";
    std::string predictPort = "1025";
    std::string statusPort = "2020";

    std::vector<std::thread> StartServices()
    {
        std::vector<std::thread> threads;
        for (size_t i = 0; i < numMindIEServer; i++) {
            predictIPList.emplace_back("127.0.0.1");
            auto pPort = std::to_string(GetUnBindPort());
            auto mPort = std::to_string(GetUnBindPort());
            auto iPort = std::to_string(GetUnBindPort());
            auto manageIP = predictIPList[i];
            auto predictIP = predictIPList[i];
            predictPortList.push_back(pPort);
            managePortList.push_back(mPort);
            interCommonPortLists.push_back(iPort);
            std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                            ", manage port = " << mPort << std::endl;

            threads.push_back(std::thread(CreateMindIEServer,
            std::move(manageIP), std::move(pPort),
            std::move(predictIP), std::move(mPort),
            4, i, "")); // 默认4线程启动服务
        }

        return threads;
    }
};

/*
测试描述: 默认调度器使用PD分离 测试openai的v1/chat/completions接口
测试步骤:
    1. 启动ms_server线程，ms_coordinate线程
    2. 发送一个非流式推理请求到指定的IP和端口
    3. 发送一个流式推理请求
    4. 发送一个单流数据请求
    5. 发送一个单非流式数据请求
    6. 发送异常请求数据 exceptionBody 发送请求
    7. 模拟重复请求场景
    8. 模拟请求过多场景
    9. 模拟coordinate不可用场景
预期结果:
    1. response为"My name is Olivier and IOK"，返回值code应为0（表示成功）
    2. 将响应结果合并为完整内容为"My name is Olivier and IMy name is Olivier and I"，返回值code应为0（表示成功）
    3. 流式响应通过 ProcessStreamRespond 处理后，预期内容为 "M"
    4. 直接断言响应值 response 为 "M"
    5. 返回缺少关键字错误信息"Invalid request format: Missing both 'prompt' or 'messages'\r\n"
    6. 返回重复请求的错误信息"Duplicate request id\r\n"
    7. 返回请求过多的错误信息"Too many requests\r\n"
    8. 返回coordinate不可用信息"MindIE-MS Coordinator is not ready\r\n"
*/
TEST_F(TestOpenAI, TestOpenAITC01)
{
    std::vector<std::thread> threads = StartServices();

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);
    auto singlestreamBody = CreateOpenAIRequest({"M"}, true);
    auto singlenonstreamBody = CreateOpenAIRequest({"M"}, false);
    auto exceptionBody = CreateOpenAIExceptionRequest({"My name is Olivier and I", "OK"}, false);

    std::string response;
    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, statusPort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", exceptionBody, response);
    EXPECT_EQ(response, "Invalid request format: Missing both 'prompt' or 'messages'\r\n");

    // 打桩AddReq
    Stub stub;
    stub.set(ADDR(ReqManage, AddReq), &AddReqMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "Duplicate request id\r\n"); // mock实现为将所有的msg返回
    stub.reset(ADDR(ReqManage, AddReq));

    // 打桩GetReqNum
    Stub stub2;
    stub2.set(ADDR(ReqManage, GetReqNum), &GetReqNumMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "Too many requests\r\n"); // mock实现为将所有的msg返回
    stub2.reset(ADDR(ReqManage, GetReqNum));

    // 打桩GetReqNum
    Stub stub3;
    stub3.set(ADDR(ClusterNodes, IsAvailable), &IsAvailableMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "MindIE-MS Coordinator is not ready\r\n"); // mock实现为将所有的msg返回
    stub3.reset(ADDR(ClusterNodes, IsAvailable));

    Stub stub4;
    stub4.set(ADDR(Configure, IsMaster), &IsMasterMock);
    // 打桩LinkWithDNode
    Stub stub5;
    stub5.set(ADDR(RequestRepeater, LinkWithDNode), &LinkWithDNodeMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    stub5.reset(ADDR(RequestRepeater, LinkWithDNode));

    // 打桩LinkWithDNodeInMaxRetry
    Stub stub6;
    stub6.set(ADDR(RequestListener, LinkWithDNodeInMaxRetry), &LinkWithDNodeInMaxRetryMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    stub6.reset(ADDR(RequestListener, LinkWithDNodeInMaxRetry));
    stub4.reset(ADDR(Configure, IsMaster));

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}

/*
测试描述: 默认调度器使用PD分离 测试openai的v1/completions接口
测试步骤:
    1. 启动ms_server线程，ms_coordinate线程
    2. 发送一个非流式推理请求到指定的IP和端口
    3. 发送一个流式推理请求
    4. 发送一个单流数据请求
    5. 发送一个单非流式数据请求
    6. 发送异常请求数据 exceptionBody 发送请求
    7. 模拟重复请求场景
    8. 模拟请求过多场景
    9. 模拟coordinate不可用场景
预期结果:
    1. response为"My name is Olivier and I."，返回值code应为0（表示成功）
    2. 将响应结果合并为完整内容为"My name is Olivier and I."，返回值code应为0（表示成功）
    3. 流式响应通过 ProcessStreamRespond 处理后，预期内容为 "M"
    4. 直接断言响应值 response 为 "M"
    5. 返回缺少关键字错误信息"Invalid request format: Missing both 'prompt' or 'messages'\r\n"
    6. 返回重复请求的错误信息"Duplicate request id\r\n"
    7. 返回请求过多的错误信息"Too many requests\r\n"
    8. 返回coordinate不可用信息"MindIE-MS Coordinator is not ready\r\n"
*/
TEST_F(TestOpenAI, TestOpenAICompletionsTC01)
{
    std::vector<std::thread> threads = StartServices();

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto nonStreamBody = CreateOpenAICompletionsRequest("My name is Olivier and I.", false);
    auto streamBody = CreateOpenAICompletionsRequest("My name is Olivier and I.", true);
    auto singlestreamBody = CreateOpenAICompletionsRequest({"M"}, true);
    auto singlenonstreamBody = CreateOpenAICompletionsRequest({"M"}, false);
    auto exceptionBody = CreateOpenAICompletionsExceptionRequest("My name is Olivier and I.", false);

    std::string response;
    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, statusPort);

    testing::internal::CaptureStdout();
    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I.");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and I.");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", exceptionBody, response);
    EXPECT_EQ(response, "Invalid request format: Missing both 'prompt' or 'messages'\r\n");
    
    std::string streamLogOutput = testing::internal::GetCapturedStdout();
    EXPECT_TRUE(streamLogOutput.find("prefill success") != std::string::npos);
    EXPECT_TRUE(streamLogOutput.find("decode success") != std::string::npos);

    // 打桩AddReq
    Stub stub;
    stub.set(ADDR(ReqManage, AddReq), &AddReqMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", nonStreamBody, response);
    EXPECT_EQ(response, "Duplicate request id\r\n"); // mock实现为将所有的msg返回
    stub.reset(ADDR(ReqManage, AddReq));

    // 打桩GetReqNum
    Stub stub2;
    stub2.set(ADDR(ReqManage, GetReqNum), &GetReqNumMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", nonStreamBody, response);
    EXPECT_EQ(response, "Too many requests\r\n"); // mock实现为将所有的msg返回
    stub2.reset(ADDR(ReqManage, GetReqNum));

    // 打桩GetReqNum
    Stub stub3;
    stub3.set(ADDR(ClusterNodes, IsAvailable), &IsAvailableMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", nonStreamBody, response);
    EXPECT_EQ(response, "MindIE-MS Coordinator is not ready\r\n"); // mock实现为将所有的msg返回
    stub3.reset(ADDR(ClusterNodes, IsAvailable));

    Stub stub4;
    stub4.set(ADDR(Configure, IsMaster), &IsMasterMock);
    // 打桩LinkWithDNode
    Stub stub5;
    stub5.set(ADDR(RequestRepeater, LinkWithDNode), &LinkWithDNodeMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I.");
    stub5.reset(ADDR(RequestRepeater, LinkWithDNode));

    // 打桩LinkWithDNodeInMaxRetry
    Stub stub6;
    stub6.set(ADDR(RequestListener, LinkWithDNodeInMaxRetry), &LinkWithDNodeInMaxRetryMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I.");
    stub6.reset(ADDR(RequestListener, LinkWithDNodeInMaxRetry));
    stub4.reset(ADDR(Configure, IsMaster));

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}