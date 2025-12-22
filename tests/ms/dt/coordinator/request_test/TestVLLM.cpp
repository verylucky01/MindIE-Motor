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
#include "stub.h"


using namespace MINDIE::MS;

class TestVLLM : public testing::Test {
protected:

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
};

std::string CreateVLLMRequest(std::string prompt, bool stream)
{
    nlohmann::json request;
    if (prompt == "error") {
        request["prompt"] = 123;
    } else {
        request["prompt"] = prompt;
    }

    request["max_tritons"] = 20;
    request["repetition_penalty"] = 1.03;
    request["presence_penalty"] = 1.2;
    request["frequency_penalty"] = 1.2;
    request["temperature"] = 0.5;
    request["top_k"] = 10;
    request["top_p"] = 0.95;
    request["stream"] = stream;

    return request.dump();
}

/*
测试描述: 默认调度器使用PD分离 VLLM协议
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestVLLM, TestVLLMTC01)
{
    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto predictIP = predictIPList[i];
        auto manageIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        1, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto nonStreamBody = CreateVLLMRequest("My name is Olivier and I", false);
    auto streamBody = CreateVLLMRequest("My name is Olivier and I", true);
    auto singlestreamBody = CreateVLLMRequest("M", true);
    auto singleNonstreamBody = CreateVLLMRequest("M", false);
    auto errorPromptBody = CreateVLLMRequest("error", false);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", singleNonstreamBody, response);
    EXPECT_EQ(response, "M"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    // 打桩AddReq
    Stub stub;
    stub.set(ADDR(ReqManage, AddReq), &AddReqMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", nonStreamBody, response);
    EXPECT_EQ(response, "Duplicate request id\r\n"); // mock实现为将所有的msg返回
    stub.reset(ADDR(ReqManage, AddReq));

    // 打桩GetReqNum
    Stub stub2;
    stub2.set(ADDR(ReqManage, GetReqNum), &GetReqNumMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", nonStreamBody, response);
    EXPECT_EQ(response, "Too many requests\r\n"); // mock实现为将所有的msg返回
    stub2.reset(ADDR(ReqManage, GetReqNum));

    // 打桩GetReqNum
    Stub stub3;
    stub3.set(ADDR(ClusterNodes, IsAvailable), &IsAvailableMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", nonStreamBody, response);
    EXPECT_EQ(response, "MindIE-MS Coordinator is not ready\r\n"); // mock实现为将所有的msg返回
    stub3.reset(ADDR(ClusterNodes, IsAvailable));

    Stub stub4;
    stub4.set(ADDR(Configure, IsMaster), &IsMasterMock);
    // 打桩LinkWithDNode
    Stub stub5;
    stub5.set(ADDR(RequestRepeater, LinkWithDNode), &LinkWithDNodeMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);
    stub5.reset(ADDR(RequestRepeater, LinkWithDNode));

    // 打桩LinkWithDNodeInMaxRetry
    Stub stub6;
    stub6.set(ADDR(RequestListener, LinkWithDNodeInMaxRetry), &LinkWithDNodeInMaxRetryMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);
    stub6.reset(ADDR(RequestListener, LinkWithDNodeInMaxRetry));
    stub4.reset(ADDR(Configure, IsMaster));

    // 输入错误类型prompt预期报错
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", errorPromptBody, response);
    EXPECT_EQ(response, "Request format is invalid\r\n"); // mock实现为将所有的msg返回

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}
