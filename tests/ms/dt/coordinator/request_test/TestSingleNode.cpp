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

class TestSingleNode : public testing::Test {
protected:

    void SetUp()
    {
        CopyFile(GetMSCoordinatorConfigJsonPath(), GetMSCoordinatorTestJsonPath());
        coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"log_info", "log_level"}, "DEBUG");
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        manager.SetList({"digs_scheduler_config", "deploy_mode"}, "single_node");
        manager.SetList({"digs_scheduler_config", "scheduler_type"}, "default_scheduler");
        manager.SetList({"digs_scheduler_config", "algorithm_type"}, "cache_affinity");
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Save();
    }

    void TearDown()
    {
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
    std::string coordinatorJson;
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

nlohmann::json CreateTritonRequest(bool separateApi)
{
    nlohmann::json request;
    if (separateApi) {
        request["id"] = "1";
        request["text_input"] = "My name is Olivier and I";

        // 设置 parameters 字段
        request["parameters"]["details"] = false;
        request["parameters"]["do_sample"] = false;
        request["parameters"]["max_new_tokens"] = 250;
        request["parameters"]["repetition_penalty"] = 1.1;
        request["parameters"]["seed"] = 123;
        request["parameters"]["temperature"] = 1;
        request["parameters"]["top_k"] = 10;
        request["parameters"]["top_p"] = 0.99;
        request["parameters"]["batch_size"] = 100;
        request["parameters"]["typical_p"] = 0.5;
        request["parameters"]["watermark"] = false;
        request["parameters"]["perf_stat"] = false;
    } else {
        request["id"] = "1";
        request["inputs"] = {
            {
                {"name", "input0"},
                {"shape", {1, 10}},
                {"datatype", "UINT32"},
                {"data", {396, 319}}
            }
        };
        request["outputs"] = {
            {
                {"name", "output0"}
            }
        };
        request["parameters"] = {
            {"temperature", 0.5},
            {"top_k", 10},
            {"top_p", 0.95},
            {"do_sample", true},
            {"seed", 'null'},
            {"repetition_penalty", 1.03},
            {"max_new_tokens", 20},
            {"watermark", true}
        };
    }

    return request;
}

std::string CreateTokenizerRequest(std::string textInput)
{
    nlohmann::json request;
    request["inputs"] = textInput;
    return request.dump();
}

/*
测试描述: default调度器使用单机部署 openai协议
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestSingleNode, TestDefaultPrefixCacheTC01)
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

        threads.push_back(std::thread(CreateSingleServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, "")); // 默认4线程
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));
    
    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetSingleRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK"); // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I"); // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    sleep(1);
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}

/*
测试描述: default调度器使用单机部署 tokenizer接口
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestSingleNode, TestDefaultPrefixCacheTC02)
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

        threads.push_back(std::thread(CreateSingleServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, "")); // 默认4线程
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));
    
    auto TokenizerBody = CreateTokenizerRequest("My name is Olivier and I");

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetSingleRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/tokenizer", TokenizerBody, response);
    EXPECT_EQ(response, "{\"token\":[\"My name is Olivier and I\"],\"token_number\":24}"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    sleep(1);
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}

/*
测试描述: default调度器使用单机部署 triton接口
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestSingleNode, TestDefaultPrefixCacheTC03)
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

        threads.push_back(std::thread(CreateSingleServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, "")); // 默认4线程
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));
    
    auto body = CreateTritonRequest(true);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetSingleRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest("127.0.0.1", "1025", tlsItems, "/v2/models/llama_65b/generate_stream", body.dump(),
        response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate", body.dump(), response);
    EXPECT_EQ(response, "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    body = CreateTritonRequest(false);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/infer", body.dump(), response);
    EXPECT_EQ(response, "396319"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    sleep(1);
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}

/*
测试描述: default调度器使用单机部署 triton协议
测试步骤:
    1. 配置DefaultScheduler，使用round_robin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestSingleNode, TestDefaultRoundRobinTC01)
{
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"digs_scheduler_config", "algorithm_type"}, "round_robin");
    manager.Save();
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

        threads.push_back(std::thread(CreateSingleServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, "")); // 默认4线程
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));
    
    auto body = CreateTritonRequest(true);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetSingleRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);
    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate_stream", body.dump(),
        response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and I"); // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate", body.dump(), response);
    EXPECT_EQ(response, "My name is Olivier and I"); // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    body = CreateTritonRequest(false);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/infer", body.dump(), response);
    EXPECT_EQ(response, "396319"); // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    sleep(1);
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}

/*
测试描述: default调度器使用单机部署 tokenizer接口
测试步骤:
    1. 配置DefaultScheduler，使用round_robin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestSingleNode, TestDefaultRoundRobinTC02)
{
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"digs_scheduler_config", "algorithm_type"}, "round_robin");
    manager.Save();
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

        threads.push_back(std::thread(CreateSingleServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, "")); // 默认4线程
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));
    
    auto TokenizerBody = CreateTokenizerRequest("My name is Olivier and I");

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetSingleRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/tokenizer", TokenizerBody, response);
    EXPECT_EQ(response, "{\"token\":[\"My name is Olivier and I\"],\"token_number\":24}"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    sleep(1);
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}