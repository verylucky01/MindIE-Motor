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
#include <sys/stat.h>
#include "gtest/gtest.h"
#define main __main_coordinator__
#include "coordinator/main.cpp"
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "stub.h"


using namespace MINDIE::MS;

class TestSSLRequest : public testing::Test {
protected:

    void SetUp()
    {
        CopyDefaultConfig();
        auto hseceasyPath = GetHSECEASYPATH();
        setenv("HSECEASY_PATH", hseceasyPath.c_str(), 1);
        std::cout << "HSECEASY_PATH=" << hseceasyPath << std::endl;

        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;

        std::string exePath = GetExecutablePath();
        std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
        std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_mangment_tls_enable"}, false);

        manager.SetList({"tls_config", "request_server_tls_enable"}, true);
        manager.SetList({"tls_config", "request_server_tls_items", "ca_cert"},
            GetAbsolutePath(certDir, "coordinator_to_user/ca.pem"));
        manager.SetList({"tls_config", "request_server_tls_items", "tls_cert"},
            GetAbsolutePath(certDir, "coordinator_to_user/cert.pem"));
        manager.SetList({"tls_config", "request_server_tls_items", "tls_key"},
            GetAbsolutePath(certDir, "coordinator_to_user/cert.key.pem"));
        manager.SetList({"tls_config", "request_server_tls_items", "tls_passwd"},
            GetAbsolutePath(certDir, "coordinator_to_user/cert_passwd.txt"));

        manager.SetList({"tls_config", "mindie_client_tls_enable"}, true);
        manager.SetList({"tls_config", "mindie_client_tls_items", "ca_cert"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/ca.pem"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "tls_cert"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert.pem"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "tls_key"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert.key.pem"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "tls_passwd"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert_passwd.txt"));

        manager.Save();

        // change the permission of cert files
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_user/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_user/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_user/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_user/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_to_coordinator/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert_passwd.txt"), S_IRUSR);
    }
    uint8_t numMindIEServer = 4;
    ThreadSafeVector<std::string> predictPortList;
    ThreadSafeVector<std::string> managePortList;
    ThreadSafeVector<std::string> predictIPList;
    ThreadSafeVector<std::string> interCommonPortLists;
    std::string manageIP = "127.0.0.1";
    std::string managePort = "1026";
    std::string predictIP = "127.0.0.1";
    std::string predictPort = "1025";
};

std::string CreateOpenAIRequest(std::vector<std::string> contents, bool stream)
{
    // OpenAI request
    nlohmann::json request;
    request["model"] = "llama2_7b";
    request["messages"] = nlohmann::json::array({});
    for (const auto &content : contents) {
        nlohmann::json message;
        message["role"] = "user";
        message["content"] = content;
        request["messages"].push_back(message);
    }
    request["max_tokens"] = 20;
    request["presence_penalty"] = 1.03;
    request["frequency_penalty"] = 1.0;
    request["seed"] = nullptr;
    request["temperature"] = 0.5;
    request["top_p"] = 0.95;
    request["stream"] = stream;
    return request.dump();
}

/*
测试描述: 默认调度器使用PD分离 openai协议
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestSSLRequest, TestSSLRequestTC01)
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));

    std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");

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

        threads.push_back(std::thread(CreateSSLServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, "")); // 默认4线程
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);

    TlsItems tlsItems;
    tlsItems.caCert = GetAbsolutePath(certDir, "user_to_coordinator/ca.pem");
    tlsItems.tlsCert = GetAbsolutePath(certDir, "user_to_coordinator/cert.pem");
    tlsItems.tlsKey = GetAbsolutePath(certDir, "user_to_coordinator/cert.key.pem");
    tlsItems.tlsPasswd = GetAbsolutePath(certDir, "user_to_coordinator/cert_passwd.txt");

    std::string response;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendSSLInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);

    code = SendSSLInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}