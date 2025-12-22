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
#ifndef SINGLEPD_SEND_REQ_STUB_H
#define SINGLEPD_SEND_REQ_STUB_H
#include <string>
#include <map>
#include <boost/beast/http.hpp>
#include <arpa/inet.h>
#include "ControllerConstant.h"
#define private public
#include "HttpClient.h"
using namespace MINDIE::MS;
uint64_t GetIdFormIp(const std::string &ip);
// use namepace to distinguish mock for single node pd separate scene
namespace SinglePDMock {
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

    std::string getTaskMockResp {"{tasks: [0]}"};
    int32_t getTaskMockCode = 200;
    int32_t getTaskMockRet = 0;

    std::string postQueryTaskMockResp {"{is_end: true}"};
    int32_t postQueryTaskMockCode = 200;
    int32_t postQueryTaskMockRet = 0;
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
    std::vector<std::string> peersId {};
    RoleState roleState { RoleState::READY };
};

void SetPostRoleHttpStatus(bool flag);
void ClearAllMock();
int32_t AddServerMock(const std::string &ip, ServerMock mock);
int32_t SinglePDSendRequestMock(void* obj, const Request &request, int timeoutSeconds,
    int retries, std::string& responseBody, int32_t &code);
bool CheckOnlineInstance(uint64_t groupId, const std::map<uint64_t, std::vector<uint64_t>> &target);
bool CheckOnlineInstanceWithAttempt(uint32_t attempt, uint64_t groupId,
    const std::map<uint64_t, std::vector<uint64_t>> &target);
uint64_t GetInvalidInstanceCount();
bool CheckOnlineInstanceEmpty(uint32_t attempt);
} // SinglePDMock
#endif // SINGLEPD_SEND_REQ_STUB_H
