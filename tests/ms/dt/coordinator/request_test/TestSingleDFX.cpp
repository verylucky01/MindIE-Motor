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
#include <cstdio>
#include "gtest/gtest.h"
#define main __main_coordinator__
#include "coordinator/main.cpp"
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "stub.h"


using namespace MINDIE::MS;

class TestSingleDFX : public testing::Test {
protected:

    void SetUp()
    {
        CopyFile(GetMSCoordinatorConfigJsonPath(), GetMSCoordinatorTestJsonPath());
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        setenv("HSECEASY_PATH", GetHSECEASYPATH().c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        manager.SetList({"http_config", "status_port"}, statusPort);
        manager.SetList({"digs_scheduler_config", "deploy_mode"}, "single_node");
        manager.SetList({"digs_scheduler_config", "scheduler_type"}, "default_scheduler");
        manager.SetList({"digs_scheduler_config", "algorithm_type"}, "cache_affinity");
        manager.SetList({"exception_config", "first_token_timeout"}, 5);
        manager.SetList({"exception_config", "infer_timeout"}, 10);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Save();
        for (auto i = 0; i < numMindIEServer; i++) {
            predictIPList.emplace_back("127.0.0.1");
        }
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
    std::string statusPort = "2028";
};

class SingleResErrorMock : public MindIEServerMock {
public:
    explicit SingleResErrorMock(const std::string &keyInit) : MindIEServerMock(keyInit) {}
    ~SingleResErrorMock() = default;
    void SingleMessageHandler(std::shared_ptr<ServerConnection> connection)
    {
        ServerRes res;
        res.state = boost::beast::http::status::bad_request;
        res.body = "SingleResErrorMock return error";
        connection->SendRes(res);
    }
};

class SingleTimeoutMock : public MindIEServerMock {
public:
    explicit SingleTimeoutMock(const std::string &keyInit) : MindIEServerMock(keyInit) {}
    ~SingleTimeoutMock() = default;
    void SetFirstTimeout(size_t timeout)
    {
        this->firstTimeout = timeout;
    }
    void SetLastTimeout(size_t timeout)
    {
        this->lastTimeout = timeout;
    }
    void SingleTimeoutHandler(std::shared_ptr<ServerConnection> connection)
    {
        auto &req = connection->GetReq();
        std::string reqId = req["req-id"];
        bool stream = false;
        auto body = boost::beast::buffers_to_string(req.body().data());
        std::string question = "";
        nlohmann::json pResJson;
        try {
            auto bodyJson = nlohmann::json::parse(body);
            auto messages = bodyJson.at("messages");
            stream = bodyJson.at("stream").template get<bool>();
            for (auto it = messages.begin(); it != messages.end(); ++it) {
                auto content = it->at("content").template get<std::string>();
                question += content;
            }
        } catch (const std::exception &e) {
            ServerRes res;
            res.state = boost::beast::http::status::bad_request;
            res.body = e.what();
            connection->SendRes(res);
            return;
        }

        pResJson["reqId"] = reqId;
        pResJson["isStream"] = stream;
        if (stream) {
            for (size_t i = 0; i < question.size(); ++i) {
                ServerRes res;
                nlohmann::json data;
                data["index"] = i;
                data["value"] = std::string(1, (char) question[i]);
                if (i == (question.size() - 1)) {
                    if (lastTimeout > 0) {
                        std::this_thread::sleep_for(std::chrono::seconds(lastTimeout));
                    }
                    res.body += "reqId:" + reqId + '\0' + "lastData:" + data.dump() + '\0';
                    res.isFinish = true;
                } else {
                    res.body = "reqId:" + reqId + '\0' + "data:" + data.dump() + '\0';
                    res.isFinish = false;
                }
                res.contentType = "text/event-stream";
                connection->SendRes(res);
                std::cout << "Stream res: " << reqId << ", data: " << res.body << std::endl;
            }
        } else {
            if (firstTimeout > 0) {
                std::this_thread::sleep_for(std::chrono::seconds(firstTimeout));
            }
            ServerRes res;
            res.contentType = "text/event-stream";
            res.body += question;
            res.isFinish = true;
            connection->SendRes(res);
        }
    }

private:
    size_t firstTimeout = 0;
    size_t lastTimeout = 0;
};

class SingleRetryMock : public MindIEServerMock {
public:
    explicit SingleRetryMock(const std::string &keyInit) : MindIEServerMock(keyInit) {}
    ~SingleRetryMock() = default;
    void SingleRetryHandler(std::shared_ptr<ServerConnection> connection)
    {
        auto &req = connection->GetReq();
        std::string reqId = req["req-id"];

        if (req["is-recompute"] != "true") {
            std::cout << "SendRetry reqId: " << reqId << std::endl;
            auto body = boost::beast::buffers_to_string(req.body().data());
            ServerRes res;
            res.contentType = "text/event-stream";
            res.isFinish = false;
            res.body += "reqId:" + reqId + '\0' + "retry:" + body + '\0';
            connection->SendRes(res);
        }

        bool stream = false;
        auto body = boost::beast::buffers_to_string(req.body().data());
        std::string question = "";
        nlohmann::json pResJson;
        try {
            auto bodyJson = nlohmann::json::parse(body);
            auto messages = bodyJson.at("messages");
            stream = bodyJson.at("stream").template get<bool>();
            for (auto it = messages.begin(); it != messages.end(); ++it) {
                auto content = it->at("content").template get<std::string>();
                question += content;
            }
        } catch (const std::exception &e) {
            ServerRes res;
            res.state = boost::beast::http::status::bad_request;
            res.body = e.what();
            connection->SendRes(res);
            return;
        }

        pResJson["reqId"] = reqId;
        pResJson["isStream"] = stream;
        if (stream) {
            for (size_t i = 0; i < question.size(); ++i) {
                ServerRes res;
                nlohmann::json data;
                data["index"] = i;
                data["value"] = std::string(1, (char) question[i]);
                if (i == (question.size() - 1)) {
                    res.body += "reqId:" + reqId + '\0' + "lastData:" + data.dump() + '\0';
                    res.isFinish = true;
                } else {
                    res.body = "reqId:" + reqId + '\0' + "data:" + data.dump() + '\0';
                    res.isFinish = false;
                }
                res.contentType = "text/event-stream";
                connection->SendRes(res);
                std::cout << "Stream res: " << reqId << ", data: " << res.body << std::endl;
            }
        } else {
            ServerRes res;
            res.contentType = "text/event-stream";
            res.body += question;
            res.isFinish = true;
            connection->SendRes(res);
        }
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

std::unique_ptr<HttpServer> CreateSingleErrorServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2)
{
    HttpServer httpServer1;
    httpServer1.Init(2);
    // 设置回调函数
    ServerHandler handler1;
    auto openAIMock1 = std::make_shared<SingleResErrorMock>(ip1);
    ClusterMock::GetCluster().AddInstance(ip1, port1, openAIMock1);
    // 注册请求到达时的回调函数

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/chat/completions",
        std::bind(&SingleResErrorMock::SingleMessageHandler, openAIMock1.get(), std::placeholders::_1));

    HttpServerParm parm1;
    parm1.address = ip1;
    parm1.port = port1;  // 端口号
    parm1.serverHandler = handler1;
    parm1.tlsItems.tlsEnable = false;
    parm1.maxKeepAliveReqs = 9999999;
    ServerHandler handler2;
    handler2.RegisterFun(boost::beast::http::verb::post,
        "/v1/role/prefill",
        std::bind(&MindIEServerMock::RolePHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::post,
        "/v1/role/decode",
        std::bind(&MindIEServerMock::RoleDHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::get,
        "/v1/config",
        std::bind(&MindIEServerMock::ConfigHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::get,
        "/v1/status",
        std::bind(&MindIEServerMock::StatusHandler, openAIMock1.get(), std::placeholders::_1));
    HttpServerParm parm2;
    parm2.address = ip2;
    parm2.port = port2;  // 端口号
    parm2.serverHandler = handler2;
    parm2.tlsItems.tlsEnable = false;
    parm2.maxKeepAliveReqs = 9999999;
    httpServer1.Run({parm1, parm2});
}

std::unique_ptr<HttpServer> CreateSingleTimeoutServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, int firstTimeout, int lastTimeout)
{
    HttpServer httpServer1;
    httpServer1.Init(8);
    // 设置回调函数
    ServerHandler handler1;
    auto openAIMock1 = std::make_shared<SingleTimeoutMock>(ip1);
    openAIMock1->SetFirstTimeout(firstTimeout);
    openAIMock1->SetLastTimeout(lastTimeout);
    ClusterMock::GetCluster().AddInstance(ip1, port1, openAIMock1);
    // 注册请求到达时的回调函数

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/chat/completions",
        std::bind(&SingleTimeoutMock::SingleTimeoutHandler, openAIMock1.get(), std::placeholders::_1));

    HttpServerParm parm1;
    parm1.address = ip1;
    parm1.port = port1;  // 端口号
    parm1.serverHandler = handler1;
    parm1.tlsItems.tlsEnable = false;
    parm1.maxKeepAliveReqs = 9999999;
    ServerHandler handler2;
    handler2.RegisterFun(boost::beast::http::verb::post,
        "/v1/role/prefill",
        std::bind(&MindIEServerMock::RolePHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::post,
        "/v1/role/decode",
        std::bind(&MindIEServerMock::RoleDHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::get,
        "/v1/config",
        std::bind(&MindIEServerMock::ConfigHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::get,
        "/v1/status",
        std::bind(&MindIEServerMock::StatusHandler, openAIMock1.get(), std::placeholders::_1));
    HttpServerParm parm2;
    parm2.address = ip2;
    parm2.port = port2;  // 端口号
    parm2.serverHandler = handler2;
    parm2.tlsItems.tlsEnable = false;
    parm2.maxKeepAliveReqs = 9999999;
    httpServer1.Run({parm1, parm2});
}

std::unique_ptr<HttpServer> CreateSingleRetryServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2)
{
    HttpServer httpServer1;
    httpServer1.Init(2);
    // 设置回调函数
    ServerHandler handler1;
    auto openAIMock1 = std::make_shared<SingleRetryMock>(ip1);
    ClusterMock::GetCluster().AddInstance(ip1, port1, openAIMock1);
    // 注册请求到达时的回调函数

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/chat/completions",
        std::bind(&SingleRetryMock::SingleRetryHandler, openAIMock1.get(), std::placeholders::_1));

    HttpServerParm parm1;
    parm1.address = ip1;
    parm1.port = port1;  // 端口号
    parm1.serverHandler = handler1;
    parm1.tlsItems.tlsEnable = false;
    parm1.maxKeepAliveReqs = 9999999;
    ServerHandler handler2;
    handler2.RegisterFun(boost::beast::http::verb::post,
        "/v1/role/prefill",
        std::bind(&MindIEServerMock::RolePHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::post,
        "/v1/role/decode",
        std::bind(&MindIEServerMock::RoleDHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::get,
        "/v1/config",
        std::bind(&MindIEServerMock::ConfigHandler, openAIMock1.get(), std::placeholders::_1));
    handler2.RegisterFun(boost::beast::http::verb::get,
        "/v1/status",
        std::bind(&MindIEServerMock::StatusHandler, openAIMock1.get(), std::placeholders::_1));
    HttpServerParm parm2;
    parm2.address = ip2;
    parm2.port = port2;  // 端口号
    parm2.serverHandler = handler2;
    parm2.tlsItems.tlsEnable = false;
    parm2.maxKeepAliveReqs = 9999999;
    httpServer1.Run({parm1, parm2});
}

/*
测试描述: default调度器使用单机部署openai协议，节点返回错误信息
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，节点返回错误信息，预期结果1
预期结果:
    1. 返回为错误信息
*/
TEST_F(TestSingleDFX, TestSingleErrorTC01)
{
    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
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

        threads.push_back(std::thread(CreateSingleErrorServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort)));
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
    EXPECT_EQ(response, "SingleResErrorMock return error");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(response, "SingleResErrorMock return error");
    EXPECT_EQ(code, 0);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(5); // 增加5s，等待请求退出
}

/*
测试描述: default调度器使用单机部署openai协议，节点返回信息超时
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，节点返回信息超时，预期结果1
预期结果:
    1. 返回为超时错误
*/
TEST_F(TestSingleDFX, TestSingleTimeoutTC01)
{
    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
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

        threads.push_back(std::thread(CreateSingleTimeoutServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort), 6, 11)); // 默认设置firstTimeout为6, lastTimeout 11线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I"}, true);

    TlsItems tlsItems;
    std::string response;
    auto pdInstance = SetSingleRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "Request first token timeout\r\n");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    auto filteredResponse = response.substr(response.find("\"M\"}") + 5);
    std::string targetString = "Request inference timeout\r\n";
    EXPECT_EQ(filteredResponse, targetString);
    EXPECT_EQ(code, 0);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(9); // 停9s,等待请求处理完成
}

/*
测试描述: 单机部署场景，同步信息中的某实例，在coordinator不存在，扩容场景
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含1个节点的集群状态信息，预期结果1。
    3. 新增一个节点后，发送包含2个节点的集群状态信息，预期结果1。
    4. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0。
*/
TEST_F(TestSingleDFX, SingleInstancesAdd)
{
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"log_info", "to_file"}, true);
    manager.SetList({"log_info", "run_log_path"}, "./runlog_SingleInstancesAdd.txt");
    manager.SetList({"log_info", "operation_log_path"}, "./operlog_SingleInstancesAdd.txt");
    manager.Save();

    {
        std::ifstream infile("./operlog_SingleInstancesAdd.txt");
        if (infile.good()) {
            remove("./operlog_SingleInstancesAdd.txt");
        }
        std::ifstream infile2("./runlog_SingleInstancesAdd.txt");
        if (infile2.good()) {
            remove("./runlog_SingleInstancesAdd.txt");
        }
    }

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
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
    ThreadSafeVector<std::string> predictIP1;
    ThreadSafeVector<std::string> predictPort1;
    predictIP1.emplace_back("127.0.0.1");
    predictPort1.emplace_back(predictPortList[0]);
    auto pdInstance = SetSingleRole(predictIP1, predictPort1, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);

    {
        std::ifstream logFile("./operlog_SingleInstancesAdd.txt");
        std::string line;
        std::string target = "Add instance IP 127.0.0.1:";
        target += predictPortList[1];
        bool found = false;
        while (std::getline(logFile, line)) {
            if (line.find(target) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_EQ(found, false);
    }

    // 增加1个节点
    ThreadSafeVector<std::string> predictIP2;
    predictIP2.emplace_back(predictIPList[0]);
    predictIP2.emplace_back(predictIPList[1]);

    ThreadSafeVector<std::string> predictPort2;
    predictPort2.emplace_back(predictPortList[0]);
    predictPort2.emplace_back(predictPortList[1]);
    pdInstance = SetSingleRole(predictIP2, predictPort2, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);

    {
        std::ifstream logFile("./operlog_SingleInstancesAdd.txt");
        std::string line;
        std::string target = "Add instance IP 127.0.0.1:";
        target += predictPortList[1];
        bool found = false;
        while (std::getline(logFile, line)) {
            if (line.find(target) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_EQ(found, true);
    }

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(2); // 睡眠2s，供请求退出
}

/*
测试描述: 单机部署场景，同步信息中的某实例，在coordinator存在，更新场景
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2个节点的集群状态信息，预期结果1。
    3. 更新一个节点后，发送包含2个节点的集群状态信息，预期结果1。
    4. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0。
*/
TEST_F(TestSingleDFX, SingleInstancesUpdate)
{
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"log_info", "to_file"}, true);
    manager.SetList({"log_info", "log_level"}, "DEBUG");
    manager.SetList({"log_info", "run_log_path"}, "./runlog_SingleInstancesUpdate.txt");
    manager.SetList({"log_info", "operation_log_path"}, "./operlog_SingleInstancesUpdate.txt");
    manager.Save();

    {
        std::ifstream infile("./operlog_SingleInstancesUpdate.txt");
        if (infile.good()) {
            remove("./operlog_SingleInstancesUpdate.txt");
        }
        std::ifstream infile2("./runlog_SingleInstancesUpdate.txt");
        if (infile2.good()) {
            remove("./runlog_SingleInstancesUpdate.txt");
        }
    }

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
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
    ThreadSafeVector<std::string> predictIP1;
    ThreadSafeVector<std::string> predictPort1;
    for (auto i = 0; i < 2; i++) { // 2个节点
        predictIP1.emplace_back("127.0.0.1");
        predictPort1.emplace_back(predictPortList[i]);
    }

    auto pdInstance = SetSingleRole(predictIP1, predictPort1, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);

    // 更新节点
    ThreadSafeVector<std::string> predictIP2;
    ThreadSafeVector<std::string> predictPort2;
    for (auto i = 0; i < 2; i++) { // 2个节点
        predictIP2.emplace_back("127.0.0.1");
        predictPort2.emplace_back(predictPortList[i]);
    }

    pdInstance = SetSingleRole(predictIP2, predictPort2, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);

    {
        std::ifstream logFile("./runlog_SingleInstancesUpdate.txt");
        std::string line;
        std::string target = "[ControllerListener] Updating instance with ID ";
        target += "1";
        bool found = false;
        while (std::getline(logFile, line)) {
            if (line.find(target) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_EQ(found, true);
    }

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(2); // 睡眠2s，供请求退出
}


/*
测试描述: 单机部署场景，同步信息中减少实例，在coordinator存在，缩容场景
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2个节点的集群状态信息，预期结果1。
    3. 减少一个节点后，发送包含1个节点的集群状态信息，预期结果1。
    4. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0。
*/
TEST_F(TestSingleDFX, SingleInstancesRemove)
{
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"log_info", "to_file"}, true);
    manager.SetList({"log_info", "run_log_path"}, "./runlog_SingleInstancesRemove.txt");
    manager.SetList({"log_info", "operation_log_path"}, "./operlog_SingleInstancesRemove.txt");
    manager.Save();

    {
        std::ifstream infile("./operlog_SingleInstancesRemove.txt");
        if (infile.good()) {
            remove("./operlog_SingleInstancesRemove.txt");
        }
        std::ifstream infile2("./runlog_SingleInstancesRemove.txt");
        if (infile2.good()) {
            remove("./runlog_SingleInstancesRemove.txt");
        }
    }

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
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
    ThreadSafeVector<std::string> predictIP1;
    ThreadSafeVector<std::string> predictPort1;
    for (auto i = 0; i < 2; i++) { // 2个节点
        predictIP1.emplace_back("127.0.0.1");
        predictPort1.emplace_back(predictPortList[i]);
    }

    auto pdInstance = SetSingleRole(predictIP1, predictPort1, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    auto code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);

    {
        std::ifstream logFile("./operlog_SingleInstancesRemove.txt");
        std::string line;
        std::string target = "Remove instance IP: 127.0.0.1:";
        bool found = false;
        while (std::getline(logFile, line)) {
            if (line.find(target) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_EQ(found, false);
    }

    // 减少1个节点
    ThreadSafeVector<std::string> predictIP2;
    ThreadSafeVector<std::string> predictPort2;
    for (auto i = 0; i < 1; i++) {
        predictIP2.emplace_back("127.0.0.1");
        predictPort2.emplace_back(predictPortList[i]);
    }

    pdInstance = SetSingleRole(predictIP2, predictPort2, managePortList,
        interCommonPortLists, manageIP, managePort);
    WaitCoordinatorReady(manageIP, statusPort);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");
    EXPECT_EQ(code, 0);

    code = SendInferRequest(predictIP, predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "My name is Olivier and IMy name is Olivier and I");
    EXPECT_EQ(code, 0);

    {
        std::ifstream logFile("./operlog_SingleInstancesRemove.txt");
        std::string line;
        std::string target = "Remove instance IP: 127.0.0.1:";
        target += predictPortList[1];
        bool found = false;
        while (std::getline(logFile, line)) {
            if (line.find(target) != std::string::npos) {
                found = true;
                break;
            }
        }
        EXPECT_EQ(found, true);
    }

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
    sleep(2); // 睡眠2s，供请求退出
}
