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

class TestPResError : public testing::Test {
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

class PResErrorMock : public MindIEServerMock {
public:
    explicit PResErrorMock(const std::string &keyInit) : MindIEServerMock(keyInit) {}
    ~PResErrorMock() = default;
    void SetPDRespond(std::shared_ptr<ServerConnection> connection, bool isStream, std::string question,
        nlohmann::json &pResJson) override
    {
        ServerRes res;
        res.state = boost::beast::http::status::bad_request;
        res.body = "PResErrorMock return error";
        connection->SendRes(res);
        std::cout << "P res ip: " << key << std::endl;

        return;
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
测试描述: OpenAI推理请求，P返回error
测试步骤:
    1. 构造P返回error的mock
    2. 向coordinator发送OpenAI推理请求，预期结果
预期结果:
    1. 返回PResErrorMock return error
*/
TEST_F(TestPResError, TestPResErrorTC01)
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

        threads.emplace_back(std::thread([predictIP, pPort, manageIP, mPort] {
            HttpServer httpServer1;
            httpServer1.Init(1);
            // 设置回调函数
            ServerHandler handler1;
            auto mock = std::make_shared<PResErrorMock>(predictIP);
            ClusterMock::GetCluster().AddInstance(predictIP, pPort, mock);
            // 注册请求到达时的回调函数
            handler1.RegisterFun(boost::beast::http::verb::post,
                "/v1/chat/completions",
                std::bind(&PResErrorMock::OpenAIPReqHandler, mock.get(), std::placeholders::_1));
            handler1.RegisterFun(boost::beast::http::verb::get,
                "/dresult",
                std::bind(&PResErrorMock::DReqHandler, mock.get(), std::placeholders::_1));
            HttpServerParm parm1;
            parm1.address = predictIP;
            parm1.port = pPort;  // 端口号
            parm1.serverHandler = handler1;
            parm1.tlsItems.tlsEnable = false;
            parm1.maxKeepAliveReqs = 9999999;
            ServerHandler handler2;
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/prefill",
                std::bind(&PResErrorMock::RolePHandler, mock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/decode",
                std::bind(&PResErrorMock::RoleDHandler, mock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/config",
                std::bind(&PResErrorMock::ConfigHandler, mock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/status",
                std::bind(&PResErrorMock::StatusHandler, mock.get(), std::placeholders::_1));
            HttpServerParm parm2;
            parm2.address = manageIP;
            parm2.port = mPort;  // 端口号
            parm2.serverHandler = handler2;
            parm2.tlsItems.tlsEnable = false;
            parm2.maxKeepAliveReqs = 9999999;
            httpServer1.Run({parm1, parm2});
        }));
        sleep(5);
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
    EXPECT_EQ(response, "PResErrorMock return error");
    EXPECT_EQ(code, 0);

    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(2); // 睡眠2s，供请求退出
}