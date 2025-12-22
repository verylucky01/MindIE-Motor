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

class TestTriton : public testing::Test {
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

std::string CreateTritonRequest(std::string inferId, std::string textInput, bool separateApi)
{
    nlohmann::json request;

    if (separateApi) {
        if (inferId == "error") {
            request["id"] = 123321;
        } else if (inferId != "") {
            request["id"] = inferId;
        }
        request["text_input"] = textInput;

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
        if (inferId == "error") {
            request["id"] = 123321;
        } else if (inferId != "") {
            request["id"] = inferId;
        }
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

    return request.dump();
}

std::string CreateTritonErrRequest(std::string inferId, std::string textInput, int errCode)
{
    nlohmann::json request;

    if (inferId == "error") {
        request["id"] = 123321;
    } else if (inferId != "") {
        request["id"] = inferId;
    }
    if (errCode == 1) {
        request["inputs"] = {
            { { "name", "input0" }, { "shape", { 1, 10 } }, { "datatype", "UINT64" }, { "data", { 396, 319 } } }
        };
    } else if (errCode == 4) {
        request["inputs"] = { { { "name", "input0" }, { "shape", { 1, 10 } }, { "datatype", "UINT32" } } };
    } else {
        request["inputs"] = {
            { { "name", "input0" }, { "shape", { 1, 10 } }, { "datatype", "UINT32" }, { "data", { 396, 319 } } }
        };
    }

    request["outputs"] = { { { "name", "output0" } } };
    if (errCode == 2) {
        return request.dump();
    }
    if (errCode == 3) {
        request["parameters"] = { { "temperature", 0.5 }, { "top_k", 10 },    { "top_p", 0.95 },
                                  { "do_sample", true },  { "seed", 'null' }, { "repetition_penalty", 1.03 },
                                  { "watermark", true } };
    }

    return request.dump();
}

/*
测试描述: 默认调度器使用PD分离 Triton协议
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestTriton, TestTritonTC01)
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

    auto nonStreamBody = CreateTritonRequest("1", "My name is Olivier and I", true);
    auto streamBody = CreateTritonRequest("1", "My name is Olivier and I", true);
    auto singlestreamBody = CreateTritonRequest("1", "M", true);
    auto singleNonstreamBody = CreateTritonRequest("1", "M", true);
    auto nonSeparateBody = CreateTritonRequest("1", "My name is Olivier and I", false);

    auto errorBody1 = CreateTritonErrRequest("1", "My name is Olivier and I", 1);
    auto errorBody2 = CreateTritonErrRequest("1", "My name is Olivier and I", 2);
    auto errorBody3 = CreateTritonErrRequest("1", "My name is Olivier and I", 3);
    auto errorBody4 = CreateTritonErrRequest("1", "My name is Olivier and I", 4);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate",
        nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate_stream",
        streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate_stream",
        singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate",
        singleNonstreamBody, response);
    EXPECT_EQ(response, "M"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/infer", nonSeparateBody, response);
    EXPECT_EQ(response, "396319"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    // ------------------------反例

    // 打桩AddReq
    Stub stub;
    stub.set(ADDR(ReqManage, AddReq), &AddReqMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate",
        nonStreamBody, response);
    EXPECT_EQ(response, "Duplicate request id\r\n"); // mock实现为将所有的msg返回
    stub.reset(ADDR(ReqManage, AddReq));

    // 打桩GetReqNum
    Stub stub2;
    stub2.set(ADDR(ReqManage, GetReqNum), &GetReqNumMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate",
        nonStreamBody, response);
    EXPECT_EQ(response, "Too many requests\r\n"); // mock实现为将所有的msg返回
    stub2.reset(ADDR(ReqManage, GetReqNum));

    // 打桩GetReqNum
    Stub stub3;
    stub3.set(ADDR(ClusterNodes, IsAvailable), &IsAvailableMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate",
        nonStreamBody, response);
    EXPECT_EQ(response, "MindIE-MS Coordinator is not ready\r\n"); // mock实现为将所有的msg返回
    stub3.reset(ADDR(ClusterNodes, IsAvailable));

    // 错误输入1
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/infer", errorBody1, response);
    EXPECT_EQ(response, "Request format is invalid\r\n"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    // 错误输入2
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/infer", errorBody2, response);
    EXPECT_EQ(response, "396319"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    // 错误输入3
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/infer", errorBody3, response);
    EXPECT_EQ(response, "396319"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    // 错误输入4
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/infer", errorBody4, response);
    EXPECT_EQ(response, "Request format is invalid\r\n"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    // 错误输入5
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models//generate",
        singleNonstreamBody, response);
    EXPECT_EQ(response, "Request format is invalid\r\n"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);

    Stub stub4;
    stub4.set(ADDR(Configure, IsMaster), &IsMasterMock);
    // 打桩LinkWithDNode
    Stub stub5;
    stub5.set(ADDR(RequestRepeater, LinkWithDNode), &LinkWithDNodeMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate",
        nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);
    stub5.reset(ADDR(RequestRepeater, LinkWithDNode));

    // 打桩LinkWithDNodeInMaxRetry
    Stub stub6;
    stub6.set(ADDR(RequestListener, LinkWithDNodeInMaxRetry), &LinkWithDNodeInMaxRetryMock);
    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v2/models/llama_65b/generate",
        nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and I"); // mock实现为将所有的msg返回
    EXPECT_EQ(code, 0);
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
