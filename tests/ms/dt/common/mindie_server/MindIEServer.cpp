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
#include "MindIEServer.h"
#include "Configure.h"
#include "nlohmann/json.hpp"
#include "MyThreadPool.h"
#include "Helper.h"


using namespace MINDIE::MS;

using namespace std;

// 节点

MindIEServerMock::MindIEServerMock(const std::string &keyInit) : key(keyInit), role("none"), init(false) {}

void MindIEServerMock::InitMetrics()
{
    singleNodeMetrics.resize(4);  // 4 种单机模式指标
    PDNodeMetrics.resize(4);  // 4 种PD分离模式指标

    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string metricsSingleNodeData0File = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsSingleNodeData_0.bin");
    std::string metricsSingleNodeData1File = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsSingleNodeData_1.bin");
    std::string metricsSingleNodeData2File = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsSingleNodeData_2.bin");
    std::string metricsSingleNodeData3File = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsSingleNodeData_3.bin");
    std::vector<std::string> singleFilePath = {metricsSingleNodeData0File, metricsSingleNodeData1File,
        metricsSingleNodeData2File, metricsSingleNodeData3File};
    for (auto i = 0; i < singleFilePath.size(); i++) {
        std::ifstream readFile(singleFilePath[i]);
        std::stringstream buf;
        buf << readFile.rdbuf();
        singleNodeMetrics[i] = buf.str();
        readFile.close();
    }

    std::string metricsPDNodeData0File = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsPDNodeData_0.bin");
    std::string metricsPDNodeData1File = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsPDNodeData_1.bin");
    std::string metricsPDNodeData2File = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsPDNodeData_2.bin");
    std::string metricsPDNodeData3File = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsPDNodeData_3.bin");
    std::vector<std::string> PDFilePath = {metricsPDNodeData0File, metricsPDNodeData1File,
        metricsPDNodeData2File, metricsPDNodeData3File};
    for (auto i = 0; i < PDFilePath.size(); i++) {
        std::ifstream readFile(PDFilePath[i]);
        std::stringstream buf;
        buf << readFile.rdbuf();
        PDNodeMetrics[i] = buf.str();
        readFile.close();
    }
}

std::string MindIEServerMock::GetKey() const
{
    return key;
}
void MindIEServerMock::SetServerId(int32_t serverId)
{
    id = serverId;
}

void MindIEServerMock::SetPDRespond(std::shared_ptr<ServerConnection> connection, bool isStream, std::string question,
    nlohmann::json &pResJson)
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
        if (question.size() == 1) {
            pResJson["isLastResp"] = true;
        } else {
            pResJson["isLastResp"] = false;
        }
        pResJson["output"] = question;
    }

    ServerRes res;
    res.body = pResJson.dump();
    connection->SendRes(res);
    std::cout << "P res ip: " << key << std::endl;
    // std::cout << "P res: " << reqId << ", data: " << res.body << std::endl;
    // size为0时，D不做响应；size为1时，流式P不做响应，D响应全部的报文内容
    if (question.size() == 0 || question.size() == 1) {
        return;
    }

    auto dInstance = ClusterMock::GetCluster().GetInstance(dTarget, dPort);
    if (dInstance != nullptr) {
        if (isStream) {
            question = question.substr(1);
        }
        MyThreadPool::GetInstance().submit(&MindIEServerMock::DMessageHandler, dInstance.get(), reqId, question,
            isStream);
    }

    return;
}

// P发送响应函数
void MindIEServerMock::TritonPReqHandler(std::shared_ptr<ServerConnection> connection)
{
    auto &req = connection->GetReq();
    std::string reqId = req["req-id"];
    auto dTarget = req["d-target"];
    bool stream = false;
    auto body = boost::beast::buffers_to_string(req.body().data());
    std::string question = "";
    nlohmann::json pResJson;
    try {
        auto bodyJson = nlohmann::json::parse(body);
        std::cout << "TritonPReqHandler: " << req.target() << std::endl;
        if (req.target().ends_with("generate_stream")) {
            stream = true;
            question = bodyJson.at("text_input");
        } else if (req.target().ends_with("generate")) {
            stream = false;
            question = bodyJson.at("text_input");
        } else if (req.target().ends_with("infer")) {
            stream = false;
            auto inputs = bodyJson.at("inputs");
            for (auto it = inputs.begin(); it != inputs.end(); ++it) {
                auto datas = it->at("data");
                auto data_ = datas.get<std::vector<uint32_t>>();
                for (auto dd: data_) {
                    question += std::to_string(dd);
                }
            }
        }
    } catch (const std::exception &e) {
        ServerRes res;
        res.state = boost::beast::http::status::bad_request;
        res.body = e.what();
        connection->SendRes(res);
        return;
    }
    SetPDRespond(connection, stream, question, pResJson);
}

// P发送响应函数
void MindIEServerMock::TGIPReqHandler(std::shared_ptr<ServerConnection> connection)
{
    auto &req = connection->GetReq();
    std::string reqId = req["req-id"];
    auto dTarget = req["d-target"];
    bool stream = false;
    auto body = boost::beast::buffers_to_string(req.body().data());
    std::string question = "";
    nlohmann::json pResJson;

    try {
        auto bodyJson = nlohmann::json::parse(body);
        std::cout << "TGIPReqHandler: " << req.target() << std::endl;

        if (req.target().ends_with("generate_stream")) {
            stream = true;
            question = bodyJson.at("inputs");
        } else if (req.target().ends_with("generate")) {
            stream = false;
            if (bodyJson.contains("prompt")) {
                question = bodyJson.at("prompt");
                stream = bodyJson.at("stream").template get<bool>();
            } else {
                question = bodyJson.at("inputs");
            }
        } else {
            stream = bodyJson.at("stream").template get<bool>();
            question = bodyJson.at("inputs");
        }
    } catch (const std::exception &e) {
        ServerRes res;
        res.state = boost::beast::http::status::bad_request;
        res.body = e.what();
        connection->SendRes(res);
        return;
    }

    SetPDRespond(connection, stream, question, pResJson);
}

// P发送响应函数
void MindIEServerMock::MindIEPReqHandler(std::shared_ptr<ServerConnection> connection)
{
    auto &req = connection->GetReq();
    std::string reqId = req["req-id"];
    auto dTarget = req["d-target"];
    bool stream = false;
    auto body = boost::beast::buffers_to_string(req.body().data());
    std::string question = "";
    nlohmann::json pResJson;
    try {
        auto bodyJson = nlohmann::json::parse(body);
        question = bodyJson.at("inputs");
        stream = bodyJson.at("stream").template get<bool>();
    } catch (const std::exception &e) {
        ServerRes res;
        res.state = boost::beast::http::status::bad_request;
        res.body = e.what();
        connection->SendRes(res);
        return;
    }

    SetPDRespond(connection, stream, question, pResJson);
}


// P发送响应函数
void MindIEServerMock::OpenAIPReqHandler(std::shared_ptr<ServerConnection> connection)
{
    auto &req = connection->GetReq();
    std::string reqId = req["req-id"];
    auto dTarget = req["d-target"];
    bool stream = false;
    auto body = boost::beast::buffers_to_string(req.body().data());
    std::string question = "";
    nlohmann::json pResJson;
    try {
        auto bodyJson = nlohmann::json::parse(body);
        if (bodyJson.find("prompt") != bodyJson.end()) {
            question = bodyJson.at("prompt").template get<std::string>();
            stream = bodyJson.at("stream").template get<bool>();
        } else if (bodyJson.find("messages") != bodyJson.end()) {
            auto messages = bodyJson.at("messages");
            stream = bodyJson.at("stream").template get<bool>();
            for (auto it = messages.begin(); it != messages.end(); ++it) {
                auto content = it->at("content").template get<std::string>();
                question += content;
            }
        } else {
            ServerRes res;
            res.state = boost::beast::http::status::not_found;
            res.body = "Invalid request format: Missing both 'prompt' or 'messages'";
            connection->SendRes(res);
        }
    } catch (const std::exception &e) {
        ServerRes res;
        res.state = boost::beast::http::status::bad_request;
        res.body = e.what();
        connection->SendRes(res);
        return;
    }

    SetPDRespond(connection, stream, question, pResJson);
}

// D收到请求的响应函数
void MindIEServerMock::DReqHandler(std::shared_ptr<ServerConnection> connection)
{
    std::cout << "DReqHandler ip: " << key << std::endl;
    dConn = connection;
}

// D发送响应函数
void MindIEServerMock::DMessageHandler(std::string reqId, std::string message, bool isStream)
{
    std::cout << "\nDMessageHandler reqId: " << reqId << "\nmessage: " << message << "\nisStream: "
        << isStream << std::endl;
    if (isStream) {
        for (size_t i = 0; i < message.size(); ++i) {
            ServerRes res;
            nlohmann::json data;
            data["index"] = i + 1;
            data["value"] = std::string(1, (char)message[i]);
            if (i == (message.size() - 1)) {
                res.body += "reqId:" + reqId + '\0' + "lastData:" + data.dump() + '\0';
            } else {
                res.body = "reqId:" + reqId + '\0' + "data:" + data.dump() + '\0';
            }
            res.contentType = "text/event-stream";
            res.isFinish = false;
            dConn->SendRes(res);
            // std::cout << "D res: " << reqId << ", data: " << res.body << std::endl;
        }
    } else {
        ServerRes res;
        res.contentType = "text/event-stream";
        res.body += "reqId:" + reqId + '\0' + "lastData:" + message + '\0';
        res.isFinish = false;
        dConn->SendRes(res);
    }

    std::cout << "DMessageHandler end " << std::endl;
}

// 指定身份为P响应函数 /v1/role/prefill
void MindIEServerMock::RolePHandler(std::shared_ptr<ServerConnection> connection)
{
    std::cout << "RolePHandler ip: " << key << std::endl;

    {
        std::unique_lock<std::mutex> lock(mtx);
        role = "prefill";
    }
    ServerRes res;
    nlohmann::json resJson;
    resJson["result"] = "ok";
    res.body = resJson.dump();
    connection->SendRes(res);
    init = true;
}

// 指定身份为D响应函数 /v1/role/decode
void MindIEServerMock::RoleDHandler(std::shared_ptr<ServerConnection> connection)
{
    std::cout << "RoleDHandler ip: " << key << std::endl;
    auto &req = connection->GetReq();
    auto body = boost::beast::buffers_to_string(req.body().data());
    try {
        auto bodyJson = nlohmann::json::parse(body);
        auto peers = bodyJson.at("peers");
        {
            std::unique_lock<std::mutex> lock(mtx);
            role = "decode";
            for (auto it = peers.begin(); it != peers.end(); ++it) {
                auto connIp = it->at("server_ip").template get<std::string>();
                connIpVec.emplace_back(connIp);
            }
        }
    } catch (const std::exception &e) {
        ServerRes res;
        res.state = boost::beast::http::status::bad_request;
        nlohmann::json resJson;
        resJson["error"] = e.what();
        resJson["error_type"] = "bad_request";
        res.body = resJson.dump();
        connection->SendRes(res);
        return;
    }
    ServerRes res;
    nlohmann::json resJson;
    resJson["result"] = "ok";
    res.body = resJson.dump();
    connection->SendRes(res);
    init = true;
}

// 静态配置采集响应函数 /v1/config
void MindIEServerMock::ConfigHandler(std::shared_ptr<ServerConnection> connection)
{
    nlohmann::json resJson;
    resJson["modelName"] = "llama3_8b";
    resJson["maxSeqLen"] = 2560;
    resJson["npuMemSize"] = 8;
    resJson["cpuMemSize"] = 5;
    resJson["worldSize"] = 1;
    resJson["maxOutputLen"] = 512;
    resJson["cacheBlockSize"] = 128;
    ServerRes res;
    res.body = resJson.dump();
    connection->SendRes(res);
}

// 动态状态采集响应函数 /v1/status
void MindIEServerMock::StatusHandler(std::shared_ptr<ServerConnection> connection)
{
    nlohmann::json resJson;
    nlohmann::json service;
    if (init) {
        service["roleStatus"] = "RoleReady";
    } else {
        service["roleStatus"] = "RoleUnknown";
    }
    {
        std::unique_lock<std::mutex> lock(mtx);
        service["currentRole"] = role;
    }
    resJson["service"] = service;
    nlohmann::json resource;
    resource["availSlotsNum"] = 5000; // 默认返回5000
    resource["availBlockNum"] = 5000; // 默认返回5000
    resource["waitingRequestNum"] = 5000; // 默认返回5000
    resource["runningRequestNum"] = 5000; // 默认返回5000
    resource["swappedRequestNum"] = 5000; // 默认返回5000
    resource["freeNpuBlockNums"] = 5000; // 默认返回5000
    resource["freeCpuBlockNums"] = 5000; // 默认返回5000
    resource["totalNpuBlockNums"] = 5000; // 默认返回5000
    resource["totalCpuBlockNums"] = 5000; // 默认返回5000
    resJson["resource"] = resource;
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (role == "decode") {
            nlohmann::json peers = nlohmann::json::array();
            for (auto &ip : connIpVec) {
                peers.emplace_back(ip);
            }
            resJson["peers"] = peers;
        }
    }
    ServerRes res;
    res.body = resJson.dump();
    connection->SendRes(res);
}

// 监控指标查询响应函数 /metrics
void MindIEServerMock::MetricsHandler(std::shared_ptr<ServerConnection> connection)
{
    ServerRes res;
    res.body = singleNodeMetrics[id % 4];  // 4 根据serverId分配不同的指标数值
    connection->SendRes(res);
}

// PD分离场景 监控指标查询响应函数 /metrics
void MindIEServerMock::PDMetricsHandler(std::shared_ptr<ServerConnection> connection)
{
    ServerRes res;
    res.body = PDNodeMetrics[id % 4];  // 4 根据serverId分配不同的指标数值
    connection->SendRes(res);
}

// 计算token响应函数 /v1/tokenizer
void MindIEServerMock::TokenizerHandler(std::shared_ptr<ServerConnection> connection)
{
    auto &req = connection->GetReq();
    auto body = boost::beast::buffers_to_string(req.body().data());
    std::string question = "";
    try {
        auto bodyJson = nlohmann::json::parse(body);
        question = bodyJson.at("inputs").template get<std::string>();
    } catch (const std::exception &e) {
        ServerRes res;
        res.state = boost::beast::http::status::bad_request;
        res.body = e.what();
        connection->SendRes(res);
        return;
    }
    nlohmann::json pResJson;
    pResJson["token_number"] = question.size();
    pResJson["token"] = nlohmann::json::array();
    pResJson["token"].emplace_back(question);
    ServerRes res;
    res.body = pResJson.dump();
    connection->SendRes(res);
}

void MindIEServerMock::OpenAISingleReqHandler(std::shared_ptr<ServerConnection> connection)
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
                res.isFinish = true;
            } else {
                res.isFinish = false;
            }
            res.body = data.dump();
            res.contentType = "text/event-stream";
            connection->SendRes(res);
            // std::cout << "D res: " << reqId << ", data: " << res.body << std::endl;
        }
    } else {
        ServerRes res;
        res.contentType = "text/event-stream";
        res.body += question;
        res.isFinish = true;
        connection->SendRes(res);
    }
}

void MindIEServerMock::TritonSingleReqHandler(std::shared_ptr<ServerConnection> connection)
{
    auto &req = connection->GetReq();
    std::string reqId = req["req-id"];
    bool stream = false;
    auto body = boost::beast::buffers_to_string(req.body().data());
    std::string question = "";
    nlohmann::json pResJson;
    try {
        auto bodyJson = nlohmann::json::parse(body);
        std::cout << "TritonPReqHandler: " << req.target() << std::endl;
        if (req.target().ends_with("generate_stream")) {
            stream = true;
            question = bodyJson.at("text_input");
        } else if (req.target().ends_with("generate")) {
            stream = false;
            question = bodyJson.at("text_input");
        } else if (req.target().ends_with("infer")) {
            stream = false;
            auto inputs = bodyJson.at("inputs");
            for (auto it = inputs.begin(); it != inputs.end(); ++it) {
                auto datas = it->at("data");
                auto data_ = datas.get<std::vector<uint32_t>>();
                for (auto dd: data_) {
                    question += std::to_string(dd);
                }
            }
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
                res.isFinish = true;
            } else {
                res.isFinish = false;
            }
            res.body = data.dump();
            res.contentType = "text/event-stream";
            connection->SendRes(res);
            // std::cout << "D res: " << reqId << ", data: " << res.body << std::endl;
        }
    } else {
        ServerRes res;
        res.contentType = "text/event-stream";
        res.body += question;
        res.isFinish = true;
        connection->SendRes(res);
    }
}

void CreateSingleServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, uint32_t threadNum, int32_t serverId,
    const std::string &metricPort)
{
    HttpServer httpServer1;
    httpServer1.Init(threadNum);
    // 设置回调函数
    ServerHandler handler1;
    auto openAIMock1 = std::make_shared<MindIEServerMock>(ip1);
    (*openAIMock1).SetServerId(serverId);
    (*openAIMock1).InitMetrics();
    ClusterMock::GetCluster().AddInstance(ip1, port1, openAIMock1);

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/chat/completions",
        std::bind(&MindIEServerMock::OpenAISingleReqHandler, openAIMock1.get(), std::placeholders::_1));
    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/generate_stream",
        std::bind(&MindIEServerMock::TritonSingleReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/generate",
        std::bind(&MindIEServerMock::TritonSingleReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/infer",
        std::bind(&MindIEServerMock::TritonSingleReqHandler, openAIMock1.get(), std::placeholders::_1));
    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/tokenizer",
        std::bind(&MindIEServerMock::TokenizerHandler, openAIMock1.get(), std::placeholders::_1));
    HttpServerParm parm1;
    parm1.address = ip1;
    parm1.port = port1;  // 推理端口号
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
    parm2.port = port2;  // 管理端口号
    parm2.serverHandler = handler2;
    parm2.tlsItems.tlsEnable = false;
    parm2.maxKeepAliveReqs = 9999999;

    ServerHandler handler3;
    handler3.RegisterFun(boost::beast::http::verb::get,
        "/metrics",
        std::bind(&MindIEServerMock::MetricsHandler, openAIMock1.get(), std::placeholders::_1));
    HttpServerParm parm3;
    parm3.address = ip2;
    parm3.port = metricPort;  // metric端口号
    parm3.serverHandler = handler3;
    parm3.tlsItems.tlsEnable = false;
    parm3.maxKeepAliveReqs = 9999999;

    httpServer1.Run({parm1, parm2, parm3});
}


void CreateMindIEServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, uint32_t threadNum, int32_t serverId,
    const std::string &metricPort)
{
    HttpServer httpServer1;
    httpServer1.Init(threadNum);
    // 设置回调函数
    ServerHandler handler1;
    auto openAIMock1 = std::make_shared<MindIEServerMock>(ip1);
    (*openAIMock1).SetServerId(serverId);
    (*openAIMock1).InitMetrics();
    ClusterMock::GetCluster().AddInstance(ip1, port1, openAIMock1);
    // 注册请求到达时的回调函数
    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/generate_stream",
        std::bind(&MindIEServerMock::TritonPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/generate",
        std::bind(&MindIEServerMock::TritonPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/infer",
        std::bind(&MindIEServerMock::TritonPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/",
        std::bind(&MindIEServerMock::TGIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/generate_stream",
        std::bind(&MindIEServerMock::TGIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/generate",
        std::bind(&MindIEServerMock::TGIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/infer",
        std::bind(&MindIEServerMock::MindIEPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/completions",
        std::bind(&MindIEServerMock::OpenAIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/chat/completions",
        std::bind(&MindIEServerMock::OpenAIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::get,
        "/dresult",
        std::bind(&MindIEServerMock::DReqHandler, openAIMock1.get(), std::placeholders::_1));
    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/tokenizer",
        std::bind(&MindIEServerMock::TokenizerHandler, openAIMock1.get(), std::placeholders::_1));
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

    ServerHandler handler3;
    handler3.RegisterFun(boost::beast::http::verb::get,
        "/metrics",
        std::bind(&MindIEServerMock::PDMetricsHandler, openAIMock1.get(), std::placeholders::_1));
    HttpServerParm parm3;
    parm3.address = ip2;
    parm3.port = metricPort;  // metric端口号
    parm3.serverHandler = handler3;
    parm3.tlsItems.tlsEnable = false;
    parm3.maxKeepAliveReqs = 9999999;

    httpServer1.Run({parm1, parm2, parm3});
}

void CreateSSLServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, uint32_t threadNum, const std::string &metricPort)
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");
    TlsItems serverTlsItems;
    serverTlsItems.caCert = GetAbsolutePath(certDir, "mindie_server_to_coordinator/ca.pem");
    serverTlsItems.tlsCert = GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert.pem");
    serverTlsItems.tlsKey = GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert.key.pem");
    serverTlsItems.tlsPasswd = GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert_passwd.txt");
    serverTlsItems.kmcKsfMaster = GetAbsolutePath(certDir, "mindie_server_to_coordinator/tools/pmt/master/ksfa");
    serverTlsItems.kmcKsfStandby = GetAbsolutePath(certDir, "mindie_server_to_coordinator/tools/pmt/standby/ksfb");
    HttpServer httpServer1;
    httpServer1.Init(threadNum);
    // 设置回调函数
    ServerHandler handler1;
    auto openAIMock1 = std::make_shared<MindIEServerMock>(ip1);
    (*openAIMock1).SetServerId(0);  // 默认 0
    (*openAIMock1).InitMetrics();
    ClusterMock::GetCluster().AddInstance(ip1, port1, openAIMock1);
    // 注册请求到达时的回调函数
    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/generate_stream",
        std::bind(&MindIEServerMock::TritonPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/generate",
        std::bind(&MindIEServerMock::TritonPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/infer",
        std::bind(&MindIEServerMock::TritonPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/",
        std::bind(&MindIEServerMock::TGIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/generate_stream",
        std::bind(&MindIEServerMock::TGIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/generate",
        std::bind(&MindIEServerMock::TGIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/infer",
        std::bind(&MindIEServerMock::MindIEPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/chat/completions",
        std::bind(&MindIEServerMock::OpenAIPReqHandler, openAIMock1.get(), std::placeholders::_1));

    handler1.RegisterFun(boost::beast::http::verb::get,
        "/dresult",
        std::bind(&MindIEServerMock::DReqHandler, openAIMock1.get(), std::placeholders::_1));
    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v1/tokenizer",
        std::bind(&MindIEServerMock::TokenizerHandler, openAIMock1.get(), std::placeholders::_1));
    HttpServerParm parm1;
    parm1.address = ip1;
    parm1.port = port1;  // 端口号
    parm1.serverHandler = handler1;
    parm1.maxKeepAliveReqs = 9999999;
    parm1.tlsItems.tlsEnable = true;
    parm1.tlsItems.caCert = serverTlsItems.caCert;
    parm1.tlsItems.tlsCert = serverTlsItems.tlsCert;
    parm1.tlsItems.tlsKey = serverTlsItems.tlsKey;
    parm1.tlsItems.tlsPasswd = serverTlsItems.tlsPasswd;
    parm1.tlsItems.kmcKsfMaster = serverTlsItems.kmcKsfMaster;
    parm1.tlsItems.kmcKsfStandby = serverTlsItems.kmcKsfStandby;
    parm1.tlsItems.tlsCrl = "";

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

    TlsItems tlsItems;
    tlsItems.caCert = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/ca.pem");
    tlsItems.tlsCert = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert.pem");
    tlsItems.tlsKey = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert.key.pem");
    tlsItems.tlsPasswd = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert_passwd.txt");
    tlsItems.kmcKsfMaster = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/tools/pmt/master/ksfa");
    tlsItems.kmcKsfStandby = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/tools/pmt/standby/ksfb");
    ServerHandler handler3;
    handler3.RegisterFun(boost::beast::http::verb::get,
        "/metrics",
        std::bind(&MindIEServerMock::PDMetricsHandler, openAIMock1.get(), std::placeholders::_1));
    HttpServerParm parm3;
    parm3.address = ip2;
    parm3.port = metricPort;  // metric端口号
    parm3.serverHandler = handler3;
    parm3.maxKeepAliveReqs = 9999999;
    parm3.tlsItems.tlsEnable = false;
    parm3.tlsItems.caCert = tlsItems.caCert;
    parm3.tlsItems.tlsCert = tlsItems.tlsCert;
    parm3.tlsItems.tlsKey = tlsItems.tlsKey;
    parm3.tlsItems.tlsPasswd = tlsItems.tlsPasswd;
    parm3.tlsItems.kmcKsfMaster = tlsItems.kmcKsfMaster;
    parm3.tlsItems.kmcKsfStandby = tlsItems.kmcKsfStandby;
    parm3.tlsItems.tlsCrl = "";

    httpServer1.Run({parm1, parm2, parm3});
}

void CreateMetricsSSLServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, uint32_t threadNum, int32_t serverId)
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");
    HttpServer httpServer1;
    httpServer1.Init(threadNum);
    // 设置回调函数
    ServerHandler handler1;
    auto mock = std::make_shared<MindIEServerMock>(ip1);
    (*mock).SetServerId(serverId);
    (*mock).InitMetrics();
    ClusterMock::GetCluster().AddInstance(ip1, port1, mock);
    // 注册请求到达时的回调函数
    handler1.RegisterFun(boost::beast::http::verb::post,
        "/v2/models/*/generate_stream",
        std::bind(&MindIEServerMock::TritonPReqHandler, mock.get(), std::placeholders::_1));

    HttpServerParm parm1;
    parm1.address = ip1;
    parm1.port = port1;  // 端口号
    parm1.serverHandler = handler1;
    parm1.maxKeepAliveReqs = 9999999;
    parm1.tlsItems.tlsEnable = false;

    TlsItems serverTlsItems;
    serverTlsItems.caCert = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/ca.pem");
    serverTlsItems.tlsCert = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert.pem");
    serverTlsItems.tlsKey = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert.key.pem");
    serverTlsItems.tlsPasswd = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert_passwd.txt");
    serverTlsItems.kmcKsfMaster = GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/tools/pmt/master/ksfa");
    serverTlsItems.kmcKsfStandby = GetAbsolutePath(
        certDir, "mindie_server_manage_to_coordinator/tools/pmt/standby/ksfb");
    ServerHandler handler2;
    handler2.RegisterFun(boost::beast::http::verb::get,
        "/metrics",
        std::bind(&MindIEServerMock::PDMetricsHandler, mock.get(), std::placeholders::_1));
    HttpServerParm parm2;
    parm2.address = ip2;
    parm2.port = port2;  // metric端口号
    parm2.serverHandler = handler2;
    parm2.maxKeepAliveReqs = 9999999;
    parm2.tlsItems.tlsEnable = true;
    parm2.tlsItems.caCert = serverTlsItems.caCert;
    parm2.tlsItems.tlsCert = serverTlsItems.tlsCert;
    parm2.tlsItems.tlsKey = serverTlsItems.tlsKey;
    parm2.tlsItems.tlsPasswd = serverTlsItems.tlsPasswd;
    parm2.tlsItems.kmcKsfMaster = serverTlsItems.kmcKsfMaster;
    parm2.tlsItems.kmcKsfStandby = serverTlsItems.kmcKsfStandby;
    parm2.tlsItems.tlsCrl = "";
    httpServer1.Run({parm1, parm2});
}