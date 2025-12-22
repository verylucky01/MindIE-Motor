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
#ifndef REQUESTSERVERSTUB_H
#define REQUESTSERVERSTUB_H

#include <string>
#include <map>
#include <nlohmann/json.hpp>
#include "HttpClient.h"

using namespace MINDIE::MS;


enum class RoleStatusEnum {
    RoleUnknown,
    RoleSwitching,
    RoleReady
};

enum class CurrentRoleEnum {
    prefill,
    decode,
    none
};

extern std::string g_currentResponse;
extern int32_t g_currentCode;
extern int32_t g_currentRet;
extern int32_t g_count;

void SetResponse(RoleStatusEnum roleStatus, CurrentRoleEnum currentRole, size_t availSlotsNum, size_t availBlockNum);
void SetV2Response(RoleStatusEnum roleStatus, CurrentRoleEnum currentRole, size_t totalAvailNpuSlotsNum,
    size_t totalAvailNpuBlockNum, size_t maxAvailNpuBlockNum);
void SetInvalidV2Response(RoleStatusEnum roleStatus, CurrentRoleEnum currentRole, size_t totalAvailNpuSlotsNum);
void SetResponseInstanceInfo(std::string modelName, size_t maxSeqLen, size_t maxOutputLen, size_t cacheBlockSize);
void SetCode(int32_t code);
void SetRet(int32_t ret);
void SetCount(int32_t count);
int32_t SendRequestMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);
int32_t SendRequestMockMulti(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);
int32_t SendRequestMockCheckBody(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);
int32_t SendV2RequestMockCheckBody(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);
int32_t SendV2RequestMockCheckBodyForA3(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);


std::string RoleStatusToString(RoleStatusEnum roleStatus);
std::string CurrentRoleToString(CurrentRoleEnum currentRole);

#endif // REQUESTSERVERSTUB_H