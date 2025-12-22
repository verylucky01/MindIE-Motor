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
#ifndef CONTROLLER_SEND_REQ_STUB_H
#define CONTROLLER_SEND_REQ_STUB_H
#include <string>
#include <map>
#include <boost/beast/http.hpp>
#include <arpa/inet.h>
#include "ControllerConstant.h"
#define private public
#include "HttpClient.h"
using namespace MINDIE::MS;

struct CoordinatorMock {
    std::map<uint64_t, std::map<uint64_t, std::vector<uint64_t>>> instances {}; // key groupID
    int32_t postRefreshMockCode = 200;
    int32_t postRefreshMockRet = 0;

    std::string getInfoMockResp {};
    int32_t getInfoMockCode = 200;
    int32_t getInfoMockRet = 0;

    std::string postOfflineMockResp {};
    int32_t postOfflineMockCode = 200;
    int32_t postOfflineMockRet = 0;

    std::string postOnlineMockResp {};
    int32_t postOnlineMockCode = 200;
    int32_t postOnlineMockRet = 0;

    std::string getTaskMockResp {R"({"tasks": [0]})"};
    int32_t getTaskMockCode = 200;
    int32_t getTaskMockRet = 0;

    std::string postQueryTaskMockResp {"{is_end: true}"};
    int32_t postQueryTaskMockCode = 200;
    int32_t postQueryTaskMockRet = 0;

    std::string lastPostOnlineRequest {};
    std::string lastGetTaskRequest {};
};

struct ServerMock {
    int32_t getConfigMockCode = 200;
    int32_t getConfigMockRet = 0;

    int32_t getStatusMockCode = 200;
    int32_t getStatusMockRet = 0;

    std::string postDecodeRoleResp;
    int32_t postRoleMockCode = 200;
    int32_t postRoleMockRet = 0;

    std::string role {"none"};
    std::vector<std::string> connIpVec {};
    RoleState roleState { RoleState::READY };
};

class ControllerHttpClientMock : HttpClient {
public:
    int32_t ControllerSendRequestMock(const Request &request, int timeoutSeconds,
        int retries, std::string& responseBody, int32_t &code);
    void SetHostAndPort(const std::string &host, const std::string &port);
    int32_t OldInstanceOfflineSuccessMock(uint64_t id) { return 0; }
    int32_t OldInstanceOfflineFailMock(uint64_t id) { return -1; }
    int32_t QueryInstancePeerTasksSuccessMock(uint64_t pId, uint64_t dId, DIGSRoleChangeType type)
    {
        return 0;
    }
    int32_t QueryInstancePeerTasksFailMock(uint64_t pId, uint64_t dId, DIGSRoleChangeType type)
    {
        return -1;
    }

protected:
    int32_t DealReqToCoordinator(const std::string &ip, const Request &request, std::string& responseBody,
        int32_t &code);
    int32_t DealReqToServer(const std::string &ip, const Request &request, std::string &responseBody,
        int32_t &code);
    int32_t DealServerGetConfig(const std::string &ip, std::string& responseBody, int32_t &code);
    int32_t DealServerGetStatus(const std::string &ip, std::string &responseBody, int32_t &code);
    int32_t DealServerPostRole(const std::string &ip, const Request &request, std::string& responseBody,
        int32_t &code, const std::string &role);
};

class ControllerToServerHttpClientFail : ControllerHttpClientMock {
public:
    int32_t ControllerSendRequestMock(const Request &request, int timeoutSeconds,
        int retries, std::string& responseBody, int32_t &code);
protected:
    int32_t DealReqToServer(const std::string &ip, const Request &request, std::string &responseBody,
        int32_t &code)
    {
        code = 400;
        responseBody = "";
        return -1;
    }
};

void ClearAllMock();
int32_t AddServerMock(const std::string &ip, ServerMock mock);
bool CheckOnlineInstance(uint64_t groupId, const std::map<uint64_t, std::vector<uint64_t>> &target);
bool CheckOnlineInstanceWithAttempt(uint32_t attempt, uint64_t groupId,
    const std::map<uint64_t, std::vector<uint64_t>> &target);
bool CheckOnlineInstanceEmpty(uint32_t attempt);
bool CheckServerPeers(uint32_t attempt, const std::string &ip, const std::vector<std::string> &peers);
bool CheckServerRole(uint32_t attempt, const std::string &ip, const std::string &role);
uint64_t GetInvalidInstanceCount();
uint64_t GetIdFormIp(const std::string &ip);
void SetCoordinatorPostOnlineMock(const std::string &ip, const std::string &resp, int32_t code, int32_t ret);
std::string GetCoordinatorLastPostOnlineRequest(const std::string &ip);
void SetCoordinatorGetTaskMock(const std::string &ip, const std::string &resp, int32_t code, int32_t ret);
std::string GetCoordinatorLastGetTaskRequest(const std::string &ip);
#endif // CONTROLLER_SEND_REQ_STUB_H
