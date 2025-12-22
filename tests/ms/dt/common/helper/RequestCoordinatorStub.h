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
#ifndef REQUESTCOORDINATORSTUB_H
#define REQUESTCOORDINATORSTUB_H

#include <string>
#include <nlohmann/json.hpp>
#include "HttpClient.h"

using namespace MINDIE::MS;

enum class MetricsCode : int32_t {
    SUCCESS = 0,
    EMPTY,
    INVALIDHELPFORMAT,
    INVALIDTYPEFORMAT,
    INVALIDTYPE,
    INVALIDDATAFORMAT,
    INVALIDCOUTERVALUE,
    INVALIDHISTOGRAMVALUE,
    INVALIDGAUGEVALUE
};

extern std::string g_currentCoordinatorInstances;
extern int32_t g_currentCoordinatorCode;
extern int32_t g_currentCoordinatorRet;

void SetRequestCoordinatorMockCode(int32_t code);
void SetRequestCoordinatorMockRet(int32_t ret);
void SetRequestMetricsMockCode(MetricsCode metricsCode);
int32_t SendRequestCoordinatorMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);
int32_t SendMetricRequestCoordinatorMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code);

std::string GetCoordinatorCurrentInstances();

#endif // REQUESTSERVERSTUB_H