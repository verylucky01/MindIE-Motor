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
#include "ControllerListener.h"
#include "HttpClient.h"
#include "Configure.h"
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "ThreadSafeVector.h"

using namespace MINDIE::MS;
class TestAllZeroIpListening : public testing::Test {
protected:
    void SetUp() override
    {
        CopyDefaultConfig();
        // 为coordinator分配未使用的端口
        predictPort = std::to_string(GetUnBindPort());
        managePort = std::to_string(GetUnBindPort());
        // 准备server进程，记录它们的ip和端口信息
        for (auto i = 0; i < numMindIEServer; i++) {
            predictIPList.emplace_back("127.0.0.1");
            auto pPort = std::to_string(GetUnBindPort());
            auto mPort = std::to_string(GetUnBindPort());
            auto iPort = std::to_string(GetUnBindPort());
            predictPortList.emplace_back(pPort);
            managePortList.emplace_back(mPort);
            interCommonPortLists.emplace_back(iPort);
            auto serverPIp = predictIPList[i];
            auto serverMIp = predictIPList[i];

            auto thread = std::make_unique<std::thread>(CreateMindIEServer,
            std::move(serverPIp), std::move(pPort),
            std::move(serverMIp), std::move(mPort),
            4, i, ""); // 默认4线程启动服务
            testThreadList.emplace_back(std::move(thread));
        }
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;

        // 调整coordinator的初始化配置
        JsonFileManager manager(coordinatorJson);
        manager.Load();
         // 放行全零ip监听
        manager.SetList({"http_config", "allow_all_zero_ip_listening"}, true);
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        // 关闭安全证书以便测试
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Save();

        // 启动coordinator进程
        char *argsCoordinator[1] = {"ms_coordinator"};
        auto threadObj = std::make_unique<std::thread>(__main_coordinator__, 1, argsCoordinator);
        testThreadList.emplace_back(std::move(threadObj));
    }

    void TearDown() override
    {
        for (size_t i = 0; i < testThreadList.size(); ++i) {
            EXPECT_TRUE(testThreadList[i]->joinable());
            testThreadList[i]->detach();
        }
    }

    uint8_t numMindIEServer = 4;
    ThreadSafeVector<std::string> predictIPList;
    ThreadSafeVector<std::string> predictPortList;
    ThreadSafeVector<std::string> managePortList;
    ThreadSafeVector<std::string> interCommonPortLists;
    std::string manageIP = "0.0.0.0";
    std::string managePort = "";
    std::string predictIP = "0.0.0.0";
    std::string predictPort = "";
    std::vector<std::unique_ptr<std::thread>> testThreadList;
};

/*
测试描述: 测试2P2D的正常场景下，开启全零监听后可以正常接受推理请求，返回预期的推理结果。
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 向127.0.0.1:Coordinator的管理面端口发送包含2P2D的集群状态信息，预期结果1。
    3. 向127.0.0.1:Coordinator的数据面端口发送推理请求，预期结果2。
预期结果:
    1. 发送集群状态信息成功，返回值为0。
    2. 非流式推理请求发送成功，返回的推理结果为prompt的拼接。
*/
TEST_F(TestAllZeroIpListening, ZeroIpListeningInferSuccess)
{
    TlsItems tlsItems;
    std::string response;
    std::string localHost = "127.0.0.1";

    // 同步2P2D集群信息至coordinator，等待coordinator就绪
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, localHost, managePort);
    auto timeOutFlag = WaitCoordinatorReady(localHost, managePort);
    EXPECT_EQ(timeOutFlag, 0); // Coordinator获取集群信息成功，就绪，可进行推理

    // 创建非流式OpenAI格式推理请求
    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    // 发送推理请求
    auto code = SendInferRequest(localHost, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock的server会返回prompt的拼接
    EXPECT_EQ(code, 0);
}