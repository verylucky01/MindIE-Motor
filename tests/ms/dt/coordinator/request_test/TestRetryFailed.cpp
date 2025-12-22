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

class TestRetryFailed : public testing::Test {
protected:
    std::string coordinatorJson;
    void SetUp()
    {
        CopyDefaultConfig();
        coordinatorJson = GetMSCoordinatorTestJsonPath();
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

class RetryFailedMock : public MindIEServerMock {
public:
    explicit RetryFailedMock(const std::string &keyInit) : MindIEServerMock(keyInit) {}
    ~RetryFailedMock() = default;
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
        if (req["is-recompute"] == "true") {
            res.body = "recompute failed";
            res.state = boost::beast::http::status::internal_server_error;
        } else {
            res.body = pResJson.dump();
        }
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
                return ((RetryFailedMock *)dInstance.get())->DSendRetry(reqId, req);
            }
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

/*
测试描述: OpenAI推理请求，重计算失败场景
测试步骤:
    1. 构造发送Retry失败的mindie server
       （1）当P收到普通的推理请求时，命令D发送重计算响应
       （2）当P收到重计算的推理请求时，返回错误消息
    2. 向coordinator发送OpenAI推理请求，预期结果
预期结果:
    1. 收到重计算失败的应答Request retry failed\r\n
*/
TEST_F(TestRetryFailed, TestRetryFailedTC01)
{
    ThreadSafeVector<std::string> predictIPList;
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
            auto retryMock = std::make_shared<RetryFailedMock>(predictIP);
            ClusterMock::GetCluster().AddInstance(predictIP, pPort, retryMock);
            // 注册请求到达时的回调函数
            handler1.RegisterFun(boost::beast::http::verb::post,
                "/v1/chat/completions",
                std::bind(&RetryFailedMock::OpenAIPReqHandler, retryMock.get(), std::placeholders::_1));
            handler1.RegisterFun(boost::beast::http::verb::get,
                "/dresult",
                std::bind(&RetryFailedMock::DReqHandler, retryMock.get(), std::placeholders::_1));
            HttpServerParm parm1;
            parm1.address = predictIP;
            parm1.port = pPort;  // 端口号
            parm1.serverHandler = handler1;
            parm1.tlsItems.tlsEnable = false;
            parm1.maxKeepAliveReqs = 9999999;
            ServerHandler handler2;
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/prefill",
                std::bind(&RetryFailedMock::RolePHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/decode",
                std::bind(&RetryFailedMock::RoleDHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/config",
                std::bind(&RetryFailedMock::ConfigHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/status",
                std::bind(&RetryFailedMock::StatusHandler, retryMock.get(), std::placeholders::_1));
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
    EXPECT_EQ(response, "{\"index\":0,\"value\":\"M\"}Request retry failed\r\n");
    EXPECT_EQ(code, 0);

    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(2); // 睡眠2s，供请求退出
}

/*
测试描述: OpenAI推理请求，重计算失败场景,高并发
测试步骤:
    1. 构造发送Retry失败的mindie server
       （1）当P收到普通的推理请求时，命令D发送重计算响应
       （2）当P收到重计算的推理请求时，返回错误消息
    2. 向coordinator并发发送OpenAI推理请求，预期结果
预期结果:
    1. 收到重计算失败的应答Request retry failed\r\n
*/
TEST_F(TestRetryFailed, TestRetryFailedTC02)
{
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"http_config", "server_thread_num"}, 4); // 4线程
    manager.SetList({"http_config", "client_thread_num"}, 4); // 4线程
    manager.Save();
    ThreadSafeVector<std::string> predictIPList;
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
            auto retryMock = std::make_shared<RetryFailedMock>(predictIP);
            ClusterMock::GetCluster().AddInstance(predictIP, pPort, retryMock);
            // 注册请求到达时的回调函数
            handler1.RegisterFun(boost::beast::http::verb::post,
                "/v1/chat/completions",
                std::bind(&RetryFailedMock::OpenAIPReqHandler, retryMock.get(), std::placeholders::_1));
            handler1.RegisterFun(boost::beast::http::verb::get,
                "/dresult",
                std::bind(&RetryFailedMock::DReqHandler, retryMock.get(), std::placeholders::_1));
            HttpServerParm parm1;
            parm1.address = predictIP;
            parm1.port = pPort;  // 端口号
            parm1.serverHandler = handler1;
            parm1.tlsItems.tlsEnable = false;
            parm1.maxKeepAliveReqs = 9999999;
            ServerHandler handler2;
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/prefill",
                std::bind(&RetryFailedMock::RolePHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/decode",
                std::bind(&RetryFailedMock::RoleDHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/config",
                std::bind(&RetryFailedMock::ConfigHandler, retryMock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/status",
                std::bind(&RetryFailedMock::StatusHandler, retryMock.get(), std::placeholders::_1));
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

    std::vector<std::thread> reqThreads;
    size_t reqNum = 50; // 50条并发
    for (size_t i = 0; i < reqNum; ++i) {
        reqThreads.emplace_back(std::thread([&] {
            auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody,
                response);
            EXPECT_EQ(response, "{\"index\":0,\"value\":\"M\"}Request retry failed\r\n");
            EXPECT_EQ(code, 0);
        }));
    }
    for (size_t i = 0; i < reqNum; ++i) {
        ASSERT_TRUE(reqThreads[i].joinable());
        reqThreads[i].join();
    }

    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    sleep(2); // 睡眠2s，供请求退出
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}