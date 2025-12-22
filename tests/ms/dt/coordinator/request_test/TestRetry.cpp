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
#include "MyThreadPool.h"

using namespace MINDIE::MS;

class TestRetry : public testing::Test {
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
    std::string predictIP = "127.0.0.1";
    std::string predictPort = "2020";
    std::string manageIP = "127.0.0.1";
    std::string managePort = "2021";
};

class RetryMock : public MindIEServerMock {
public:
    explicit RetryMock(const std::string &keyInit) : MindIEServerMock(keyInit) {}
    ~RetryMock() = default;
    void SetPDRespond(std::shared_ptr<ServerConnection> connection, bool isStream, std::string question,
        nlohmann::json &pResJson) override
    {
        auto &req = connection->GetReq();
        std::string reqId = req["req-id"];
        auto dTarget = req["d-target"];
        auto dPort = req["d-port"];

        pResJson["reqId"] = reqId;
        pResJson["isStream"] = isStream;
        size_t delimiterIndex = dTarget.find(";");
        delimiterIndex = (delimiterIndex == std::string::npos) ? dTarget.size() : delimiterIndex;
        dTarget = dTarget.substr(0, delimiterIndex);

        if (isStream) {
            if (question.size() == 1) {
                nlohmann::json output;
                output["index"] = 0;
                output["value"] = question;
                pResJson["output"] = output.dump();
                pResJson["isLastResp"] = true;
            } else {
                pResJson["isLastResp"] = false;
                char ch = question[0];
                nlohmann::json output;
                output["index"] = 0;
                output["value"] = std::string(1, ch);
                pResJson["output"] = output.dump();
            }
        } else {
            pResJson["isLastResp"] = false;
            pResJson["output"] = question;
        }

        ServerRes res;
        res.body = pResJson.dump();
        connection->SendRes(res);
        std::cout << "P res ip: " << key << std::endl;

        // size 为1时，流式P不做 响应，D响应全部的报文内容
        if (question.size() == 1 && isStream) {
            return;
        }
        
        auto dInstance = ClusterMock::GetCluster().GetInstance(dTarget, dPort);
        if (dInstance != nullptr) {
            if (isStream) {
                question = question.substr(1);
            }
            if (req["is-recompute"] != "true") { // 没有触发过重计算，让D触发重计算
                return ((RetryMock *)dInstance.get())->DSendRetry(reqId, req);
            }
            MyThreadPool::GetInstance().submit(&MindIEServerMock::DMessageHandler, dInstance.get(), reqId, question,
                isStream);
        }

        return;
    }

    void DSendRetry(std::string reqId, const boost::beast::http::request<boost::beast::http::dynamic_body> &req)
    {
        std::cout << "\nDSendRetry reqId: " << reqId << std::endl;
        auto body = boost::beast::buffers_to_string(req.body().data());
        ServerRes res;
        res.contentType = "text/event-stream";
        res.isFinish = false;
        res.body += "reqId:" + reqId + '\0' + "retry:" + body + '\0';
        dConn->SendRes(res);
    }
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
测试描述: OpenAI推理请求，触发重计算
测试步骤:
    1. 构造发送Retry指令的mindie server
    2. 向coordinator发送OpenAI推理请求，预期结果
预期结果:
    1. 第一次从D实例收到的是重计算的消息
    2. 第二次收到的是非重计算的正常推理结果
    3. 最终结果与预期一致
*/
TEST_F(TestRetry, TestRetryTC01)
{
    char *argsCoordinator[1] = {"ms_coordinator"};

    ThreadSafeVector<std::string> predictIPList;
    ThreadSafeVector<std::string> predictPortList;
    ThreadSafeVector<std::string> managePortList;
    ThreadSafeVector<std::string> interCommonPortLists;
    // 构造一个http server
    std::vector<std::thread> threads;
    for (size_t i = 0; i < 4; ++i) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto predictIP = predictIPList[i];
        auto manageIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << std::endl;
        std::cout << "pPort = " << pPort << std::endl;
        std::cout << "mPort = " << mPort << std::endl;
        threads.emplace_back(std::thread([predictIP, pPort, manageIP, mPort] {
            HttpServer httpServer1;
            httpServer1.Init(1);
            // 设置回调函数
            ServerHandler handler1;
            auto retryMock = std::make_shared<RetryMock>(predictIP);
            ClusterMock::GetCluster().AddInstance(predictIP, pPort, retryMock);
            // 注册请求到达时的回调函数
            handler1.RegisterFun(boost::beast::http::verb::post,
                "/v1/chat/completions",
                std::bind(&RetryMock::OpenAIPReqHandler, retryMock.get(), std::placeholders::_1));
            handler1.RegisterFun(boost::beast::http::verb::get,
                "/dresult",
                std::bind(&RetryMock::DReqHandler, retryMock.get(), std::placeholders::_1));
            HttpServerParm parm1;
            parm1.address = predictIP;
            parm1.port = pPort;  // 端口号
            parm1.serverHandler = handler1;
            parm1.tlsItems.tlsEnable = false;
            parm1.maxKeepAliveReqs = 9999999;
            ServerHandler handler2;
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/prefill",
                std::bind(&RetryMock::RolePHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/decode",
                std::bind(&RetryMock::RoleDHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/config",
                std::bind(&RetryMock::ConfigHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/status",
                std::bind(&RetryMock::StatusHandler, retryMock.get(), std::placeholders::_1));
            HttpServerParm parm2;
            parm2.address = manageIP;
            parm2.port = mPort;  // 端口号
            parm2.serverHandler = handler2;
            parm2.tlsItems.tlsEnable = false;
            parm2.maxKeepAliveReqs = 9999999;
            httpServer1.Run({parm1, parm2});
        }));
        sleep(1);
    }

    auto lamda = [&]() {
        __main_coordinator__(1, argsCoordinator);
    };

    std::thread threadObj(lamda); // 创建线程
    EXPECT_TRUE(threadObj.joinable());

    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I"}, true);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "MMy name is Olivier and I"); // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(2); // 睡眠2s，供请求退出
}
