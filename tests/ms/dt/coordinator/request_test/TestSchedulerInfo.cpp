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

class TestSchedulerInfo : public testing::Test {
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
        manager.SetList({"http_config", "status_port"}, statusPort);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Set("string_token_rate", 4.02);
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
    std::string statusPort = "2040";
};

/*
测试描述: 默认调度器使用PD分离 openai协议, 测试/v1/coordinator_info接口
测试步骤:
    1. 配置digs_scheduler，使用load_balance算法
    2. 发送请求，预期结果1
    3. 发送/v1/coordinator_info， 返回调度信息
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestSchedulerInfo, TestSchedulerInfoTC01)
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

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);

    std::string response;
    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, statusPort);
    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);
    sleep(1);
    code = GetInferRequest(manageIP, managePort, tlsItems, "/v1/coordinator_info", response);
    EXPECT_EQ(code, 200);
    try {
        auto mJson = nlohmann::json::parse(response);
        auto lengthInfo = mJson.at("request_length_info");
        auto inputLen = lengthInfo.at("input_len").template get<uint32_t>();
        auto outputLen = lengthInfo.at("output_len").template get<uint32_t>();
        ASSERT_EQ(inputLen, 21);
        ASSERT_EQ(outputLen, 0);
    } catch (const std::exception &e) {
        ASSERT_TRUE(false);
    }
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);
    code = GetInferRequest(manageIP, managePort, tlsItems, "/v1/coordinator_info", response);
    EXPECT_EQ(code, 200);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);
    sleep(1);
    code = GetInferRequest(manageIP, managePort, tlsItems, "/v1/coordinator_info", response);
    EXPECT_EQ(code, 200);
    try {
        auto mJson = nlohmann::json::parse(response);
        auto lengthInfo = mJson.at("request_length_info");
        auto inputLen = lengthInfo.at("input_len").template get<uint32_t>();
        auto outputLen = lengthInfo.at("output_len").template get<uint32_t>();
        ASSERT_EQ(inputLen, 26);
        ASSERT_EQ(outputLen, 48);
    } catch (const std::exception &e) {
        ASSERT_TRUE(false);
    }
    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}