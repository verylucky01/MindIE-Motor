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
#include <tuple>
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
using PromptParam = std::tuple<nlohmann::json, std::string>; // request, prompt
class TestPDInferReq : public testing::Test {
protected:

    void SetUp() override
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

class TestPDInferReqPrompt : public TestPDInferReq, public ::testing::WithParamInterface<PromptParam> {
    void SetUp()
    {
        TestPDInferReq::SetUp();
    }
};

nlohmann::json CreateDefaultVLLMRequest()
{
    nlohmann::json request;

    request["prompt"] = "My name is Olivier and I";
    request["max_tritons"] = 20;
    request["repetition_penalty"] = 1.03;
    request["presence_penalty"] = 1.2;
    request["frequency_penalty"] = 1.2;
    request["temperature"] = 0.5;
    request["top_k"] = 10;
    request["top_p"] = 0.95;

    return request;
}

nlohmann::json CreateDefaultOpenAIRequest()
{
    // OpenAI request
    nlohmann::json request;
    std::vector<std::string> contents = {"My name is Olivier and I"};
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

    return request;
}

nlohmann::json CreateTGIRequest()
{
    nlohmann::json request;

    request["inputs"] = "My name is Olivier and I";
    request["parameters"] = {
        {"decoder_input_details", true},
        {"details", true},
        {"do_sample", true},
        {"max_new_tritons", 20},
        {"max_new_tokens", 1000},
        {"repetition_penalty", 1.03},
        {"return_full_text", false},
        {"seed", 0},
        {"temperature", 0.5},
        {"top_k", 10},
        {"top_p", 0.95},
        {"truncate", 0},
        {"typical_p", 0.5},
        {"watermark", false}
    };
    return request;
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

nlohmann::json CreateTokenizerRequest()
{
    nlohmann::json request;
    request["inputs"] = "My name is Olivier and I";
    return request;
}

/*
测试描述: 使用PD分离模式，发送不同协议的推理请求
测试步骤:
    1. 配置PD分离模式，发送类型错误和缺失prompt的请求，预期报错
    2. 预期结果1
预期结果:
    1. 报错，response为"Request format is invalid\r\n"
*/
TEST_P(TestPDInferReqPrompt, TestPDInferReqTC01)
{
    auto param = GetParam();
    auto defaultRequest = std::get<0>(param);
    auto prompt = std::get<1>(param);

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

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    // 输入错误prompt预期报错
    auto request = defaultRequest;
    request[prompt] = 123;
    auto requestBody = request.dump();
    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", requestBody, response);
    EXPECT_EQ(response, "Request format is invalid\r\n"); // mock实现为将所有的msg返回

    request = defaultRequest;
    request.erase(prompt);
    requestBody = request.dump();
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/generate", requestBody, response);
    EXPECT_EQ(response, "Request format is invalid\r\n"); // mock实现为将所有的msg返回

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}

std::vector<PromptParam> GetPromptParams()
{
    std::vector<PromptParam> params;

    // VLLM
    auto defaultVLLM = CreateDefaultVLLMRequest();
    auto VLLMstream = defaultVLLM;
    VLLMstream["stream"] = true;
    auto VLLMnostream = defaultVLLM;
    VLLMnostream["stream"] = false;
    params.push_back(PromptParam(VLLMstream, "prompt"));
    params.push_back(PromptParam(VLLMnostream, "prompt"));

    // OpenAI
    auto defaultOpenAI = CreateDefaultOpenAIRequest();
    auto OpenAIstream = defaultOpenAI;
    OpenAIstream["stream"] = true;
    auto OpenAInostream = defaultOpenAI;
    OpenAInostream["stream"] = false;
    params.push_back(PromptParam(OpenAIstream, "messages"));
    params.push_back(PromptParam(OpenAInostream, "messages"));

    // TGI
    auto defaultTGI = CreateTGIRequest();
    auto TGIstream = defaultTGI;
    TGIstream["stream"] = true;
    auto TGInostream = defaultTGI;
    TGInostream["stream"] = false;
    params.push_back(PromptParam(TGIstream, "inputs"));
    params.push_back(PromptParam(TGInostream, "inputs"));

    // Triton
    auto TritonSep = CreateTritonRequest(true);
    auto TritonNoSep = CreateTritonRequest(false);
    params.push_back(PromptParam(TritonSep, "text_input"));
    params.push_back(PromptParam(TritonNoSep, "inputs"));

    // Tokenizer
    auto defaultTokenizer = CreateTokenizerRequest();
    params.push_back(PromptParam(defaultTokenizer, "inputs"));

    return params;
}

INSTANTIATE_TEST_SUITE_P(,
    TestPDInferReqPrompt,
    ::testing::ValuesIn(GetPromptParams())
);