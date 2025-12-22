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
#ifndef REQUESTCCAESTUB_H
#define REQUESTCCAESTUB_H

#include <string>
#include <nlohmann/json.hpp>
#include "HttpClient.h"

using namespace MINDIE::MS;

enum class RegisterCode : int32_t {
    SUCCESS = 0,
    INVALIDRESPONSETYPE,
    INVALIDRETCODETYPE,
    INVALIDRETMSGTYPE,
    ERRORCODE,
    INVALIDREQLISTTYPE,
    INVALIDMODELID,
    INVALIDMETRICPERIOD,
    INVALIDFORCEUPDATE
};

enum class InventoriesCode : int32_t {
    SUCCESS = 0,
    INVALIDRESPONSETYPE,
    INVALIDRETCODETYPE,
    INVALIDRETMSGTYPE,
    ERRORCODE
};

extern std::string g_currentRegisterInstances;
extern int32_t g_currentRegisterCode;
extern int32_t g_currentRegisterRet;

extern std::string g_currentInventoriesInstances;
extern int32_t g_currentInventoriesCode;
extern int32_t g_currentInventoriesRet;

void SetRequestRegisterMockCode(int32_t code);
void SetRequestRegisterMockRet(int32_t ret);
void SetRequestRegisterMockCode(RegisterCode registerCode);

void SetRequestInventoriesMockCode(int32_t code);
void SetRequestInventoriesMockRet(int32_t ret);
void SetRequestInventoriesMockCode(InventoriesCode inventoriesCode);

int32_t SendRequestRegisterMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);

int32_t SendRequestInventoriesMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);

std::string GetRegisterCurrentInstances();
std::string GetInventoriesCurrentInstances();

#endif // REQUESTSERVERSTUB_H