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
#include "RequestServerStub.h"

// 初始化全局变量
std::string g_currentResponse = R"({
    "service": {
        "roleStatus": "RoleUnknown",
        "currentRole": "none"
    },
    "resource": {
        "availSlotsNum": 0,
        "availBlockNum": 0,
        "waitingRequestNum": 0,
        "runningRequestNum": 0,
        "swappedRequestNum": 0,
        "freeNpuBlockNums": 0,
        "freeCpuBlockNums": 0,
        "totalNpuBlockNums": 0,
        "totalCpuBlockNums": 0
    }
})";
std::string g_postRoleResponse = R"({
    "result": "ok"
})";
int32_t g_currentCode = 200;
int32_t g_currentRet = 0;
int32_t g_count = 0;

std::string RoleStatusToString(RoleStatusEnum roleStatus)
{
    switch (roleStatus) {
        case RoleStatusEnum::RoleUnknown:
            return "RoleUnknown";
        case RoleStatusEnum::RoleSwitching:
            return "RoleSwitching";
        case RoleStatusEnum::RoleReady:
            return "RoleReady";
        default:
            return "RoleUnknown";
    }
}

std::string CurrentRoleToString(CurrentRoleEnum currentRole)
{
    switch (currentRole) {
        case CurrentRoleEnum::prefill:
            return "prefill";
        case CurrentRoleEnum::decode:
            return "decode";
        case CurrentRoleEnum::none:
            return "none";
        default:
            return "none";
    }
}

void SetResponse(RoleStatusEnum roleStatus, CurrentRoleEnum currentRole, size_t availSlotsNum, size_t availBlockNum)
{
    nlohmann::json jsonResponse;
    jsonResponse["service"]["roleStatus"] = RoleStatusToString(roleStatus);
    jsonResponse["service"]["currentRole"] = CurrentRoleToString(currentRole);
    jsonResponse["resource"]["availSlotsNum"] = availSlotsNum;
    jsonResponse["resource"]["availBlockNum"] = availBlockNum;
    jsonResponse["resource"]["waitingRequestNum"] = 0;
    jsonResponse["resource"]["runningRequestNum"] = 0;
    jsonResponse["resource"]["swappedRequestNum"] = 0;
    jsonResponse["resource"]["freeNpuBlockNums"] = 0;
    jsonResponse["resource"]["freeCpuBlockNums"] = 0;
    jsonResponse["resource"]["totalNpuBlockNums"] = 0;
    jsonResponse["resource"]["totalCpuBlockNums"] = 0;

    nlohmann::json peers = nlohmann::json::array();
    jsonResponse["linkStatus"]["peers"] = peers;

    g_currentResponse = jsonResponse.dump();  // 使用dump()生成紧凑的JSON字符串
}

void SetResponseInstanceInfo(std::string modelName, size_t maxSeqLen, size_t maxOutputLen, size_t cacheBlockSize)
{
    nlohmann::json jsonResponse;
    jsonResponse["modelName"] = modelName;
    jsonResponse["maxSeqLen"]= maxSeqLen;
    jsonResponse["npuMemSize"] = 8;
    jsonResponse["cpuMemSize"] = 5;
    jsonResponse["worldSize"] = 8;
    jsonResponse["maxOutputLen"] = maxOutputLen;
    jsonResponse["cacheBlockSize"] = cacheBlockSize;

    g_currentResponse = jsonResponse.dump();  // 使用dump()生成紧凑的JSON字符串
}

void SetCode(int32_t code)
{
    g_currentCode = code;
}

void SetRet(int32_t ret)
{
    g_currentRet = ret;
}

void SetCount(int32_t count)
{
    g_count = count;
}

int32_t SendRequestMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    if (nlohmann::json::accept(g_currentResponse)) {
        nlohmann::json jsonResponse = nlohmann::json::parse(g_currentResponse);
        jsonResponse["result"] = "ok";
        responseBody = jsonResponse.dump();
    } else {
        responseBody = g_currentResponse;
    }
    code = g_currentCode;
    return g_currentRet;
}

int32_t SendRequestMockMulti(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    if (nlohmann::json::accept(g_currentResponse)) {
        nlohmann::json jsonResponse = nlohmann::json::parse(g_currentResponse);
        jsonResponse["result"] = "ok";
        responseBody = jsonResponse.dump();
    } else {
        responseBody = g_currentResponse;
    }
    code = g_currentCode;
    if (g_count == 0){
        g_count += 1;
        return g_currentRet;
    }
    return -1;
}

int32_t SendRequestMockCheckBody(void* obj, const Request &request, int timeoutSeconds, int retries,
                                 std::string& responseBody, int32_t &code)
{
    responseBody = g_postRoleResponse;
    int32_t failCode = 400;
    auto reqBody = request.body;
    auto bodyJson = nlohmann::json::parse(reqBody);
    if (!bodyJson["local"].contains("host_ip") || !bodyJson["local"].contains("id")) {
        code = failCode;
        return -1;
    }

    for (auto &peer : bodyJson["peers"]) {
        if (!peer.contains("host_ip") || !peer.contains("id")) {
            code = failCode;
            return -1;
        }
    }

    code = g_currentCode;
    return 0;
}

int32_t SendV2RequestMockCheckBody(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    responseBody = g_postRoleResponse;
    int32_t failCode = 400;
    auto reqBody = request.body;
    auto bodyJson = nlohmann::json::parse(reqBody);
    if (!bodyJson["local"].is_array() || !bodyJson["local"][0].contains("host_ip")
        || !bodyJson["local"][0].contains("dp_inst_list")) {
        std::cout << "V2 interface : check local json failed." << std::endl;
        code = failCode;
        return -1;
    }

    for (auto &peer : bodyJson["peers"]) {
        if (!peer.is_array() || !peer[0].contains("host_ip") || !peer[0].contains("dp_inst_list")) {
            code = failCode;
            std::cout << "V2 interface : check peer json failed." << std::endl;
            return -1;
        }
    }

    code = g_currentCode;
    return 0;
}


int32_t SendV2RequestMockCheckBodyForA3(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    responseBody = g_postRoleResponse;
    int32_t failCode = 400;
    auto reqBody = request.body;
    auto bodyJson = nlohmann::json::parse(reqBody);
    if (!bodyJson["local"].is_array() || !bodyJson["local"][0].contains("host_ip")
        || !bodyJson["local"][0].contains("dp_inst_list")
        || !bodyJson["local"][0]["dp_inst_list"][0].contains("device")
        || !bodyJson["local"][0]["dp_inst_list"][0]["device"][0].contains("super_device_id")
        || !bodyJson["local"][0].contains("super_pod_id")) {
        std::cout << "A3 V2 interface : check local json failed." << std::endl;
        code = failCode;
        return -1;
    }

    for (auto &peer : bodyJson["peers"]) {
        if (!peer.is_array() || !peer[0].contains("host_ip") || !peer[0].contains("dp_inst_list")
            || !peer[0]["dp_inst_list"][0]["device"][0].contains("super_device_id")
            || !peer[0].contains("super_pod_id")) {
            code = failCode;
            std::cout << "A3 V2 interface : check peer json failed." << std::endl;
            return -1;
        }
    }

    code = g_currentCode;
    return 0;
}


void SetV2Response(RoleStatusEnum roleStatus, CurrentRoleEnum currentRole, size_t totalAvailNpuSlotsNum,
    size_t totalAvailNpuBlockNum, size_t maxAvailNpuBlockNum)
{
    nlohmann::json jsonResponse;
    jsonResponse["service"]["roleStatus"] = RoleStatusToString(roleStatus);
    jsonResponse["service"]["currentRole"] = CurrentRoleToString(currentRole);
    jsonResponse["resource"]["totalAvailNpuSlotsNum"] = totalAvailNpuSlotsNum;
    jsonResponse["resource"]["totalAvailNpuBlockNum"] = totalAvailNpuBlockNum;
    jsonResponse["resource"]["maxAvailNpuBlockNum"] = maxAvailNpuBlockNum;

    nlohmann::json peers = nlohmann::json::array();
    jsonResponse["linkStatus"]["peers"] = peers;

    g_currentResponse = jsonResponse.dump();  // 使用dump()生成紧凑的JSON字符串
}

void SetInvalidV2Response(RoleStatusEnum roleStatus, CurrentRoleEnum currentRole, size_t totalAvailNpuSlotsNum)
{
    nlohmann::json jsonResponse;
    jsonResponse["service"]["roleStatus"] = RoleStatusToString(roleStatus);
    jsonResponse["service"]["currentRole"] = CurrentRoleToString(currentRole);
    jsonResponse["resource"]["totalAvailNpuSlotsNum"] = totalAvailNpuSlotsNum;
    jsonResponse["resource"]["totalAvailNpuBlockNum"] = "invalid";

    nlohmann::json peers = nlohmann::json::array();
    jsonResponse["linkStatus"]["peers"] = peers;

    g_currentResponse = jsonResponse.dump();
}