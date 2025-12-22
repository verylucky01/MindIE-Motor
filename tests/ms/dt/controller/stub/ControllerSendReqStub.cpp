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
#include "ControllerSendReqStub.h"
#include "ControllerConfig.h"
#include <iostream>
#include <thread>

static std::mutex g_coordinatorMutex;
static std::mutex g_serverMutex;
static std::string g_coordinatorIp = "127.0.0.1";
static int32_t g_okCodeRet = 200;
static int32_t g_notFoundCodeRet = 404;
static int32_t g_invalidInputCodeRet = 400;
static std::map<std::string, CoordinatorMock> g_coordinatorMock;
static std::map<std::string, ServerMock> g_serverMock;
static std::atomic<uint64_t> g_invalidInstanceCount;

uint64_t GetInvalidInstanceCount()
{
    return g_invalidInstanceCount;
}

void ClearAllMock()
{
    {
        std::lock_guard<std::mutex> guard(g_coordinatorMutex);
        g_coordinatorMock.clear();
    }
    {
        std::lock_guard<std::mutex> guard(g_serverMutex);
        g_serverMock.clear();
    }
}

int32_t AddServerMock(const std::string &ip, ServerMock mock)
{
    std::lock_guard<std::mutex> guard(g_serverMutex);
    auto iter = g_serverMock.find(ip);
    if (iter == g_serverMock.end()) {
        g_serverMock[ip] = mock;
    }
}

static CoordinatorMock &GetOrCreateCoordinatorMockLocked(const std::string &ip)
{
    auto iter = g_coordinatorMock.find(ip);
    if (iter == g_coordinatorMock.end()) {
        auto ret = g_coordinatorMock.emplace(ip, CoordinatorMock {});
        return ret.first->second;
    }
    return iter->second;
}

static void SetRespForIllegalInput(std::string& responseBody, int32_t &code)
{
    code = g_invalidInputCodeRet;
    responseBody = "invalid input data";
}

static bool IsInvalidInstance(const nlohmann::json &node)
{
    if (node.at("port") != "1026" || node.at("model_name") != "llama3-8b" ||
        node.at("static_info").at("total_slots_num") != "200" ||
        node.at("static_info").at("total_block_num") != "300" ||
        node.at("static_info").at("max_seq_len") != "1" ||
        node.at("static_info").at("max_output_len") != "5" ||
        node.at("static_info").at("block_size") != "6") {
        return true;
    }
    g_invalidInstanceCount++;
    return false;
}

static int32_t DealCoordinatorPostOnline(const std::string &ip, const Request &request,
    std::string& responseBody, int32_t &code)
{
    try {
        auto bodyJson = nlohmann::json::parse(request.body);
        if (!bodyJson.contains("ids") || !bodyJson.at("ids").is_array()) {
            SetRespForIllegalInput(responseBody, code);
            return 0;
        }
    } catch (const std::exception&) {
        SetRespForIllegalInput(responseBody, code);
        return 0;
    }
    std::lock_guard<std::mutex> guard(g_coordinatorMutex);
    auto &mock = GetOrCreateCoordinatorMockLocked(ip);
    mock.lastPostOnlineRequest = request.body;
    code = mock.postOnlineMockCode;
    responseBody = mock.postOnlineMockResp;
    return mock.postOnlineMockRet;
}

static int32_t DealCoordinatorGetTask(const std::string &ip, const Request &request,
    std::string& responseBody, int32_t &code)
{
    std::lock_guard<std::mutex> guard(g_coordinatorMutex);
    auto &mock = GetOrCreateCoordinatorMockLocked(ip);
    mock.lastGetTaskRequest = request.target;
    responseBody = mock.getTaskMockResp;
    code = mock.getTaskMockCode;
    return mock.getTaskMockRet;
}

int32_t ControllerHttpClientMock::DealReqToCoordinator(const std::string &ip, const Request &request,
    std::string& responseBody, int32_t &code)
{
    if ((request.target == ControllerConstant::GetInstance()->GetCoordinatorURI(CoordinatorURI::POST_ONLINE)) &&
        (request.method == boost::beast::http::verb::post)) {
        return DealCoordinatorPostOnline(ip, request, responseBody, code);
    }
    if ((request.target == ControllerConstant::GetInstance()->GetCoordinatorURI(CoordinatorURI::GET_TASK)) &&
        (request.method == boost::beast::http::verb::get)) {
        return DealCoordinatorGetTask(ip, request, responseBody, code);
    }
    if ((request.target != ControllerConstant::GetInstance()->GetCoordinatorURI(CoordinatorURI::POST_REFRESH)) ||
        (request.method != boost::beast::http::verb::post)) { // 当前仅支持postRefresh接口打桩
        code = g_notFoundCodeRet;
        responseBody = "";
        return 0;
    }
    std::cout << "coordinator " << ip << " refresh instance " << std::endl;
    try {
        auto bodyJson = nlohmann::json::parse(request.body);
        auto idsVec = bodyJson.at("ids").get<std::vector<uint64_t>>();
        auto instanceVec = bodyJson.at("instances");
        if (idsVec.size() != instanceVec.size()) {
            SetRespForIllegalInput(responseBody, code);
            return 0;
        }
        {
            std::lock_guard<std::mutex> guard(g_coordinatorMutex);
            auto iter = g_coordinatorMock.find(ip);
            if (iter == g_coordinatorMock.end()) {
                CoordinatorMock mock;
                g_coordinatorMock[ip] = mock;
            }
            iter = g_coordinatorMock.find(ip);
            iter->second.instances.clear();
        }
        for (int32_t i = 0; i < instanceVec.size(); i++) {
            auto &instance = instanceVec[i];
            if (idsVec[i] != instanceVec[i].at("id")) {
                SetRespForIllegalInput(responseBody, code);
                return 0;
            }
            if (!IsInvalidInstance(instanceVec[i])) {
                std::cout << "coordinator " << ip << " refresh invalid instance " << request.body << std::endl;
                SetRespForIllegalInput(responseBody, code);
                return 0;
            }
            {
                std::lock_guard<std::mutex> guard(g_coordinatorMutex);
                auto iter = g_coordinatorMock.find(ip);
                uint64_t groupId = instanceVec[i].at("static_info").at("group_id").get<uint64_t>();
                uint64_t id = instanceVec[i].at("id").get<uint64_t>();
                iter->second.instances[groupId][id] = {};
                if (instanceVec[i].at("static_info").at("role") != MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
                    continue;
                }
                for (auto peer : instanceVec[i].at("dynamic_info").at("peers")) {
                        iter->second.instances[groupId][id].push_back(peer.get<uint64_t>());
                }
            }
        }
        code = g_okCodeRet;
        responseBody = "";
        return 0;
    } catch (const std::exception &e) {
        std::cout << "coordinator " << ip << " refresh instance failed" << std::endl;
        SetRespForIllegalInput(responseBody, code);
        return 0;
    }
}

int32_t ControllerHttpClientMock::DealServerGetConfig(const std::string &ip, std::string& responseBody, int32_t &code)
{
    code = g_okCodeRet;
    nlohmann::json jsonResponse;
    jsonResponse["modelName"] = "llama3-8b";
    jsonResponse["maxSeqLen"]= 1;
    jsonResponse["npuMemSize"] = 2;
    jsonResponse["cpuMemSize"] = 3;
    jsonResponse["worldSize"] = 4;
    jsonResponse["maxOutputLen"] = 5;
    jsonResponse["cacheBlockSize"] = 6;
    responseBody = jsonResponse.dump();
    return 0;
}

int32_t ControllerHttpClientMock::DealServerGetStatus(const std::string &ip, std::string &responseBody, int32_t &code)
{
    code = g_okCodeRet;
    nlohmann::json jsonResponse;
    {
        std::lock_guard<std::mutex> guard(g_serverMutex);
        auto iter = g_serverMock.find(ip);
        if (iter == g_serverMock.end()) {
            ServerMock mock;
            g_serverMock[ip] = mock;
        }
        iter = g_serverMock.find(ip);
        jsonResponse["service"]["roleStatus"] = iter->second.role == "none" ?
            ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN) :
            ControllerConstant::GetInstance()->GetRoleState(iter->second.roleState);
        jsonResponse["service"]["currentRole"] = iter->second.role;
        if (ControllerConfig::GetInstance()->IsMultiNodeMode()) {
            jsonResponse["resource"]["totalAvailNpuSlotsNum"] = 200;
            jsonResponse["resource"]["totalAvailNpuBlockNum"] = 300;
            jsonResponse["resource"]["maxAvailNpuBlockNum"] = 100;
        } else {
            jsonResponse["resource"]["availSlotsNum"] = 200;
            jsonResponse["resource"]["availBlockNum"] = 300;
            jsonResponse["resource"]["waitingRequestNum"] = 0;
            jsonResponse["resource"]["runningRequestNum"] = 0;
            jsonResponse["resource"]["swappedRequestNum"] = 0;
            jsonResponse["resource"]["freeNpuBlockNums"] = 0;
            jsonResponse["resource"]["freeCpuBlockNums"] = 0;
            jsonResponse["resource"]["totalNpuBlockNums"] = 0;
            jsonResponse["resource"]["totalCpuBlockNums"] = 0;
        }
        nlohmann::json peers = nlohmann::json::array();
        for (auto &ip : iter->second.connIpVec) {
            nlohmann::json tmpPeer;

            tmpPeer["target"] = GetIdFormIp(ip);
            tmpPeer["link"] = "ok";
            peers.emplace_back(tmpPeer);
            if (iter->second.role == "decode" && iter->second.roleState == RoleState::SWITCHING) { // 切换中的D只返回一个peer
                break;
            }
        }
        jsonResponse["linkStatus"]["peers"] = peers;
    }
    responseBody = jsonResponse.dump();
    std::cout << "server " << ip << " status " << responseBody << std::endl;
    return 0;
}

int32_t ControllerHttpClientMock::DealServerPostRole(const std::string &ip, const Request &request,
    std::string& responseBody, int32_t &code, const std::string &role)
{
    nlohmann::json jsonResponse;
    jsonResponse["result"] = "ok";
    responseBody = jsonResponse.dump();
    code = g_okCodeRet;
    {
        std::lock_guard<std::mutex> guard(g_serverMutex);
        auto iter = g_serverMock.find(ip);
        if (iter == g_serverMock.end()) {
            ServerMock mock;
            g_serverMock[ip] = mock;
        }
        iter = g_serverMock.find(ip);
        iter->second.role = role;
        iter->second.connIpVec.clear();
        auto bodyJson = nlohmann::json::parse(request.body);
        auto peers = bodyJson.at("peers");
        {
            for (auto it = peers.begin(); it != peers.end(); ++it) {
                auto connIp = it->at("server_ip").template get<std::string>();
                iter->second.connIpVec.emplace_back(connIp);
            }
        }
    }
    return 0;
}

int32_t ControllerHttpClientMock::DealReqToServer(const std::string &ip, const Request &request,
    std::string &responseBody, int32_t &code)
{
    if ((request.target == ControllerConstant::GetInstance()->GetServerURI(ServerURI::GET_CONFIG)) &&
        (request.method == boost::beast::http::verb::get)) {
        std::cout << "get config for server " << ip << std::endl;
        return DealServerGetConfig(ip, responseBody, code);
    }

    if ((request.target == ControllerConstant::GetInstance()->GetServerURI(ServerURI::GET_STATUS_V1) ||
        request.target == ControllerConstant::GetInstance()->GetServerURI(ServerURI::GET_STATUS_V2)) &&
        (request.method == boost::beast::http::verb::get)) {
        std::cout << "get status for server " << ip << std::endl;
        return DealServerGetStatus(ip, responseBody, code);
    }

    if ((request.target == ControllerConstant::GetInstance()->GetServerURI(ServerURI::POST_DECODE_ROLE_V1)) &&
        (request.method == boost::beast::http::verb::post)) {
        std::string role = "decode";
        std::cout << "post role " << role << " for server " << ip << std::endl;
        return DealServerPostRole(ip, request, responseBody, code, role);
    }

    if ((request.target == ControllerConstant::GetInstance()->GetServerURI(ServerURI::POST_PREFILL_ROLE_V1)) &&
        (request.method == boost::beast::http::verb::post)) {
        std::string role = "prefill";
        std::cout << "post role " << role << " for server" << ip << std::endl;
        return DealServerPostRole(ip, request, responseBody, code, role);
    }
    code = g_notFoundCodeRet;
    responseBody = "";
    return 0;
}

int32_t ControllerHttpClientMock::ControllerSendRequestMock(const Request &request, int timeoutSeconds,
    int retries, std::string& responseBody, int32_t &code)
{
    if (this->mHost == g_coordinatorIp) {
        return DealReqToCoordinator(this->mHost, request, responseBody, code);
    }
    return DealReqToServer(this->mHost, request, responseBody, code);
}

void ControllerHttpClientMock::SetHostAndPort(const std::string &host, const std::string &port)
{
    std::cout << "SetHostAndPort host : " << host << ", port is : " << port << std::endl;
    this->mHost = host;
    this->mPort = port;
}

int32_t ControllerToServerHttpClientFail::ControllerSendRequestMock(const Request &request, int timeoutSeconds,
    int retries, std::string& responseBody, int32_t &code)
{
    if (this->mHost == g_coordinatorIp) {
        return DealReqToCoordinator(this->mHost, request, responseBody, code);
    }
    return DealReqToServer(this->mHost, request, responseBody, code);
}

static std::map<uint64_t, std::vector<uint64_t>> GetOnlineInstances(uint64_t groupId, const std::string &coordinator)
{
    std::lock_guard<std::mutex> guard(g_coordinatorMutex);
    auto coordIter = g_coordinatorMock.find(coordinator);
    if (coordIter == g_coordinatorMock.end()) {
        std::cout << "coordinator " << coordinator << " is not found" << std::endl;
        return {};
    }
    auto groupIter = coordIter->second.instances.find(groupId);
    if (groupIter == coordIter->second.instances.end()) {
        std::cout << "group id " << groupId << " is not found" << std::endl;
        return {};
    }
    return groupIter->second;
}

bool CheckOnlineInstance(uint64_t groupId, const std::map<uint64_t, std::vector<uint64_t>> &target)
{
    auto instances = GetOnlineInstances(groupId, g_coordinatorIp);
    std::cout << "instances.size() " << instances.size() << std::endl;
    for (auto targetIter : std::as_const(target)) {
        auto insIter = instances.find(targetIter.first);
        if (insIter == instances.end()) {
            std::cout << "server instance id " << targetIter.first << " is not found" << std::endl;
            return false;
        }
        if (insIter->second.size() != targetIter.second.size()) {
            std::cout << "server instance id " << targetIter.first << " has different peers. Sizes: "
                << insIter->second.size() << " vs " << targetIter.second.size() << std::endl;
            return false;
        }
        for (auto &ins : std::as_const(targetIter.second)) {
            if (std::find(insIter->second.begin(), insIter->second.end(), ins) == insIter->second.end()) {
                std::cout << "server instance id " << targetIter.first << " is not connected to peer " <<
                    ins << std::endl;
                return false;
            }
        }
    }
    return true;
}

bool CheckOnlineInstanceWithAttempt(uint32_t attempt, uint64_t groupId, const std::map<uint64_t,
    std::vector<uint64_t>> &target)
{
    for (auto i = 0; i < attempt; i++) {
        if (CheckOnlineInstance(groupId, target)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}

bool CheckServerPeers(const std::string &ip, const std::vector<std::string> &peers)
{
    std::lock_guard<std::mutex> guard(g_serverMutex);
    auto iter = g_serverMock.find(ip);
    if (iter == g_serverMock.end()) {
        std::cout << "server instance ip " << ip << " is not found " << std::endl;
        return false;
    }
    if (peers.size() != iter->second.connIpVec.size()) {
        std::cout << "server instance ip " << ip << " has peers " << iter->second.connIpVec.size() << std::endl;
        return false;
    }
    for (auto &ip : peers) {
        if (std::find(iter->second.connIpVec.begin(), iter->second.connIpVec.end(), ip)
                == iter->second.connIpVec.end()) {
            std::cout << "server instance ip " << ip << " peer " << ip  << " not found" << std::endl;
            return false;
        }
    }
    return true;
}

bool CheckServerPeers(uint32_t attempt, const std::string &ip, const std::vector<std::string> &peers)
{
    for (auto i = 0; i < attempt; i++) {
        if (CheckServerPeers(ip, peers)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}

bool CheckServerRole(const std::string &ip, const std::string &role)
{
    std::lock_guard<std::mutex> guard(g_serverMutex);
    auto iter = g_serverMock.find(ip);
    if (iter == g_serverMock.end()) {
        std::cout << "server instance ip " << ip << " is not found " << std::endl;
        return false;
    }
    if (role != iter->second.role) {
        std::cout << "server instance ip " << ip << " has role " << iter->second.role << std::endl;
        return false;
    }
    return true;
}

bool CheckServerRole(uint32_t attempt, const std::string &ip, const std::string &role)
{
    for (auto i = 0; i < attempt; i++) {
        if (CheckServerRole(ip, role)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}

bool CheckOnlineInstanceEmpty()
{
    std::lock_guard<std::mutex> guard(g_coordinatorMutex);
    auto coordIter = g_coordinatorMock.find(g_coordinatorIp);
    if (coordIter == g_coordinatorMock.end()) {
        std::cout << "coordinator " << g_coordinatorIp << " is not found" << std::endl;
        return false;
    }
    return coordIter->second.instances.empty();
}

bool CheckOnlineInstanceEmpty(uint32_t attempt)
{
    for (auto i = 0; i < attempt; i++) {
        if (CheckOnlineInstanceEmpty()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return false;
}

uint64_t GetIdFormIp(const std::string &ip)
{
    std::unordered_map<std::string, int32_t> ipToIdMap;
    ipToIdMap["127.0.0.2"] = 0;
    ipToIdMap["127.0.0.3"] = 1;
    ipToIdMap["127.0.0.4"] = 2; // 2: instance id
    ipToIdMap["127.0.0.5"] = 3; // 3: instance id
    if (ipToIdMap.find(ip) != ipToIdMap.end()) {
        std::cout << "GetIdFormIp ip is : " << ip << ", id is : " << ipToIdMap[ip] << std::endl;
        return ipToIdMap[ip];
    } else {
        std::cout << "not find in id map." << std::endl;
        return 1000; // 1000: invalid instance id
    }
}

void SetCoordinatorPostOnlineMock(const std::string &ip, const std::string &resp, int32_t code, int32_t ret)
{
    std::lock_guard<std::mutex> guard(g_coordinatorMutex);
    auto &mock = GetOrCreateCoordinatorMockLocked(ip);
    mock.postOnlineMockResp = resp;
    mock.postOnlineMockCode = code;
    mock.postOnlineMockRet = ret;
}

std::string GetCoordinatorLastPostOnlineRequest(const std::string &ip)
{
    std::lock_guard<std::mutex> guard(g_coordinatorMutex);
    auto iter = g_coordinatorMock.find(ip);
    if (iter == g_coordinatorMock.end()) {
        return {};
    }
    return iter->second.lastPostOnlineRequest;
}

void SetCoordinatorGetTaskMock(const std::string &ip, const std::string &resp, int32_t code, int32_t ret)
{
    std::lock_guard<std::mutex> guard(g_coordinatorMutex);
    auto &mock = GetOrCreateCoordinatorMockLocked(ip);
    mock.getTaskMockResp = resp;
    mock.getTaskMockCode = code;
    mock.getTaskMockRet = ret;
}

std::string GetCoordinatorLastGetTaskRequest(const std::string &ip)
{
    std::lock_guard<std::mutex> guard(g_coordinatorMutex);
    auto iter = g_coordinatorMock.find(ip);
    if (iter == g_coordinatorMock.end()) {
        return {};
    }
    return iter->second.lastGetTaskRequest;
}