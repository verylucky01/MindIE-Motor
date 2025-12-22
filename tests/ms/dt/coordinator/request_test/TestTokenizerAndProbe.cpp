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

class TestTokenizerAndProbe : public testing::Test {
protected:

    void SetUp()
    {
        CopyDefaultConfig();
        for (auto i = 0; i < numMindIEServer; i++) {
            predictIPList.emplace_back("127.0.0.1");
        }
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
};

std::string CreateTokenizerRequest(std::string textInput)
{
    nlohmann::json request;
    request["inputs"] = textInput;
    return request.dump();
}
/*
测试描述: 默认调度器使用PD分离 Tokenizer协议
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestTokenizerAndProbe, TestTokenizerAndProbeTC01)
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
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto TokenizerBody = CreateTokenizerRequest("My name is Olivier and I");
    auto TokenizerBodyShort = CreateTokenizerRequest("M");

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, statusPort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/tokenizer", TokenizerBody, response);
    EXPECT_EQ(response, "{\"token\":[\"My name is Olivier and I\"],\"token_number\":24}"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/tokenizer", TokenizerBodyShort, response);
    EXPECT_EQ(response, "{\"token\":[\"M\"],\"token_number\":1}"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    // /v2/health/ready
    code = GetInferRequest(manageIP, statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(code, 200);

    // /v1/coordinator_info
    code = GetInferRequest(manageIP, managePort, tlsItems, "/v1/coordinator_info", response);
    EXPECT_EQ(code, 200);

    // /v1/instances/offline
    code = GetInferRequest(manageIP, managePort, tlsItems, "/v1/instances/offline", response);
    EXPECT_EQ(code, 400);

    // /v1/instances/tasks
    code = GetInferRequest(manageIP, managePort, tlsItems, "/v1/instances/tasks", response);
    EXPECT_EQ(code, 200);

    // /v1/startup
    code = GetInferRequest(manageIP, statusPort, tlsItems, "/v1/startup", response);
    EXPECT_EQ(code, 200);

    // /v1/health
    code = GetInferRequest(manageIP, statusPort, tlsItems, "/v1/health", response);
    EXPECT_EQ(code, 200);

    // /v1/readiness
    code = GetInferRequest(manageIP, statusPort, tlsItems, "/v1/readiness", response);
    EXPECT_EQ(code, 200);

    // /v2/health/live
    code = GetInferRequest(manageIP, statusPort, tlsItems, "/v2/health/live", response);
    EXPECT_EQ(code, 200);

    // /v2/models/llama_65b/ready
    code = GetInferRequest(manageIP, statusPort, tlsItems, "/v2/models/llama_65b/ready", response);
    EXPECT_EQ(code, 200);

    // /v2/models/gpt-3.5-turbo/ready, 模型不存在
    code = GetInferRequest(manageIP, statusPort, tlsItems, "/v2/models/gpt-3.5-turbo/ready", response);
    EXPECT_EQ(code, 503);

    Stub stub;
    stub.set(ADDR(Configure, IsMaster), &IsMasterMock);
    // 打桩LinkWithDNode
    Stub stub2;
    stub2.set(ADDR(RequestRepeater, LinkWithDNode), &LinkWithDNodeMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/tokenizer", TokenizerBody, response);
    EXPECT_EQ(response, "{\"token\":[\"My name is Olivier and I\"],\"token_number\":24}"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);
    stub2.reset(ADDR(RequestRepeater, LinkWithDNode));

    // 打桩LinkWithDNodeInMaxRetry
    Stub stub3;
    stub3.set(ADDR(RequestListener, LinkWithDNodeInMaxRetry), &LinkWithDNodeInMaxRetryMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/tokenizer", TokenizerBody, response);
    EXPECT_EQ(response, "{\"token\":[\"My name is Olivier and I\"],\"token_number\":24}"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);
    stub3.reset(ADDR(RequestListener, LinkWithDNodeInMaxRetry));
    stub.reset(ADDR(Configure, IsMaster));

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}
