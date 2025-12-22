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

class TestTimeout : public testing::Test {
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
        manager.SetList({"exception_config", "first_token_timeout"}, 2);
        manager.SetList({"exception_config", "infer_timeout"}, 5);
        manager.Save();
    }
    std::string predictIP = "127.0.0.1";
    std::string predictPort = "2020";
    std::string manageIP = "127.0.0.1";
    std::string managePort = "2021";
};

class TimeoutMock : public MindIEServerMock {
public:
    explicit TimeoutMock(const std::string &keyInit) : MindIEServerMock(keyInit) {}
    ~TimeoutMock() = default;
    void SetPTimeout(size_t timeout)
    {
        pTimeout = timeout;
    }
    void SetDTimeout(size_t timeout)
    {
        dTimeout = timeout;
    }
    void SetPDRespond(std::shared_ptr<ServerConnection> connection, bool isStream, std::string question,
        nlohmann::json &pResJson) override
    {
        if (pTimeout > 0) {
            std::this_thread::sleep_for(std::chrono::seconds(pTimeout));
        }
        auto &req = connection->GetReq();
        std::string reqId = req["req-id"];
        auto dTarget = req["d-target"];
        auto dPort = req["d-port"];

        pResJson["reqId"] = reqId;
        pResJson["isStream"] = isStream;

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
            if (dTimeout > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(dTimeout));
            }
            MyThreadPool::GetInstance().submit(&MindIEServerMock::DMessageHandler, dInstance.get(), reqId, question,
                isStream);
        }

        return;
    }

private:
    size_t pTimeout = 0;
    size_t dTimeout = 0;
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
测试描述: OpenAI推理请求，首token timeout
测试步骤:
    1. 将ms_coordinator.json中的配置项first_token_timeout设置成2，infer_timeout设置成5
    2. 构造timeout mock，将P的发送时延设置成3
    3. 向coordinator发送OpenAI推理请求，预期结果
预期结果:
    1. 返回Request first token timeout\r\n
*/
TEST_F(TestTimeout, TestFirstTokenTimeoutTC01)
{
    ThreadSafeVector<std::string> predictIPList;
    ThreadSafeVector<std::string> predictPortList;
    ThreadSafeVector<std::string> managePortList;
    ThreadSafeVector<std::string> interCommonPortLists;

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
        std::cout << "ip = " << predictIP << std::endl;
        std::cout << "pPort = " << pPort << std::endl;
        std::cout << "mPort = " << mPort << std::endl;

        threads.emplace_back(std::thread([&] {
            HttpServer httpServer1;
            httpServer1.Init(1);
            // 设置回调函数
            ServerHandler handler1;
            auto timeoutMock = std::make_shared<TimeoutMock>(predictIPList[i]);
            timeoutMock->SetPTimeout(3);
            ClusterMock::GetCluster().AddInstance(predictIPList[i], pPort, timeoutMock);
            // 注册请求到达时的回调函数
            handler1.RegisterFun(boost::beast::http::verb::post,
                "/v1/chat/completions",
                std::bind(&TimeoutMock::OpenAIPReqHandler, timeoutMock.get(), std::placeholders::_1));
            handler1.RegisterFun(boost::beast::http::verb::get,
                "/dresult",
                std::bind(&TimeoutMock::DReqHandler, timeoutMock.get(), std::placeholders::_1));
            HttpServerParm parm1;
            parm1.address = predictIP;
            parm1.port = pPort;  // 端口号
            parm1.serverHandler = handler1;
            parm1.tlsItems.tlsEnable = false;
            parm1.maxKeepAliveReqs = 9999999;
            ServerHandler handler2;
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/prefill",
                std::bind(&TimeoutMock::RolePHandler, timeoutMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/decode",
                std::bind(&TimeoutMock::RoleDHandler, timeoutMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/config",
                std::bind(&TimeoutMock::ConfigHandler, timeoutMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/status",
                std::bind(&TimeoutMock::StatusHandler, timeoutMock.get(), std::placeholders::_1));
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

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I"}, true);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(response, "Request first token timeout\r\n");
    EXPECT_EQ(code, 0);

    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(4); // 暂停4s，等待请求处理完成
}

/*
测试描述: OpenAI推理请求，整个推理请求 timeout
测试步骤:
    1. 将ms_coordinator.json中的配置项first_token_timeout设置成2，infer_timeout设置成5
    2. 构造timeout mock，将D的发送时延设置成6
    3. 向coordinator发送OpenAI推理请求，预期结果
预期结果:
    1. 返回Request inference timeout\r\n
*/
TEST_F(TestTimeout, TestInferTimeoutTC01)
{
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

        threads.emplace_back(std::thread([&] {
            HttpServer httpServer1;
            httpServer1.Init(1);
            // 设置回调函数
            ServerHandler handler1;
            auto timeoutMock = std::make_shared<TimeoutMock>(predictIPList[i]);
            timeoutMock->SetDTimeout(6);
            ClusterMock::GetCluster().AddInstance(predictIPList[i], pPort, timeoutMock);
            // 注册请求到达时的回调函数
            handler1.RegisterFun(boost::beast::http::verb::post,
                "/v1/chat/completions",
                std::bind(&TimeoutMock::OpenAIPReqHandler, timeoutMock.get(), std::placeholders::_1));
            handler1.RegisterFun(boost::beast::http::verb::get,
                "/dresult",
                std::bind(&TimeoutMock::DReqHandler, timeoutMock.get(), std::placeholders::_1));
            HttpServerParm parm1;
            parm1.address = predictIP;
            parm1.port = pPort;  // 端口号
            parm1.serverHandler = handler1;
            parm1.tlsItems.tlsEnable = false;
            parm1.maxKeepAliveReqs = 9999999;
            ServerHandler handler2;
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/prefill",
                std::bind(&TimeoutMock::RolePHandler, timeoutMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/decode",
                std::bind(&TimeoutMock::RoleDHandler, timeoutMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/config",
                std::bind(&TimeoutMock::ConfigHandler, timeoutMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/status",
                std::bind(&TimeoutMock::StatusHandler, timeoutMock.get(), std::placeholders::_1));
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

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I"}, true);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(response, "{\"index\":0,\"value\":\"M\"}Request inference timeout\r\n");
    EXPECT_EQ(code, 0);

    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(9); // 暂停9s，等待请求处理完成
}