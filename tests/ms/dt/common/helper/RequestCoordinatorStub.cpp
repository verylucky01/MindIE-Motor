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
#include "RequestServerStub.h"
#include "RequestCoordinatorStub.h"

using namespace MINDIE::MS;

// 初始化全局变量
std::string g_currentCoordinatorInstances {};
int32_t g_currentCoordinatorMockCode = 200;
int32_t g_currentCoordinatorMockRet = 0;
MetricsCode g_currentMetricsMock;
int32_t g_CodeOK = 200;

void SetRequestCoordinatorMockCode(int32_t code)
{
    g_currentCoordinatorMockCode = code;
}

void SetRequestCoordinatorMockRet(int32_t ret)
{
    g_currentCoordinatorMockRet = ret;
}

void SetRequestMetricsMockCode(MetricsCode metricsCode)
{
    g_currentMetricsMock = metricsCode;
}

int32_t SendRequestCoordinatorMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    g_currentCoordinatorInstances = request.body;
    if (g_currentCoordinatorMockCode == g_CodeOK) {
        responseBody = R"({"result": "OK"})";
    } else {
        responseBody = "";
    }
    code = g_currentCoordinatorMockCode;
    return g_currentCoordinatorMockRet;
}

int32_t SendMetricRequestCoordinatorMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    g_currentCoordinatorInstances = request.body;
    switch (g_currentMetricsMock) {
        case MetricsCode::SUCCESS:
            responseBody = R"(
                # HELP prompt_tokens_total Number of prefill tokens processed.
                # TYPE prompt_tokens_total counter
                prompt_tokens_total{model_name="llama2-7b"} 9564
                # HELP avg_prompt_throughput_toks_per_s Average prefill throughput in tokens/s.
                # TYPE avg_prompt_throughput_toks_per_s gauge
                avg_prompt_throughput_toks_per_s{model_name="llama2-7b"} 0.586739718914032
                # HELP time_to_first_token_seconds Histogram of time to first token in seconds.
                # TYPE time_to_first_token_seconds histogram
                time_to_first_token_seconds_count{model_name="llama2-7b"} 2523
                time_to_first_token_seconds_sum{model_name="llama2-7b"} 9740.00200343132
                time_to_first_token_seconds_bucket{model_name="llama2-7b",le="0.001"} 0
            )";
            break;
        case MetricsCode::EMPTY:
            responseBody = "";
            break;
        case MetricsCode::INVALIDHELPFORMAT:
            responseBody = "# HELP prompt_tokens_total";
            break;
        case MetricsCode::INVALIDTYPEFORMAT:
            responseBody = "# TYPE prompt_tokens_total";
            break;
        case MetricsCode::INVALIDTYPE:
            responseBody = "# TYPE prompt_tokens_total count";
            break;
        case MetricsCode::INVALIDDATAFORMAT:
            responseBody = R"(
                prompt_tokens_total{model_name="llama2-7b"} 9564
            )";
            break;
        case MetricsCode::INVALIDCOUTERVALUE:
            responseBody = R"(
                # HELP prompt_tokens_total Number of prefill tokens processed.
                # TYPE prompt_tokens_total counter
                prompt_tokens_total{model_name="llama2-7b"} -50
            )";
            break;
        case MetricsCode::INVALIDGAUGEVALUE:
            responseBody = R"(
                # HELP avg_prompt_throughput_toks_per_s Average prefill throughput in tokens/s.
                # TYPE avg_prompt_throughput_toks_per_s gauge
                avg_prompt_throughput_toks_per_s{model_name="llama2-7b"} -50
            )";
            break;
        case MetricsCode::INVALIDHISTOGRAMVALUE:
            responseBody = R"(
                # HELP time_to_first_token_seconds Histogram of time to first token in seconds.
                # TYPE time_to_first_token_seconds histogram
                time_to_first_token_seconds_bucket{model_name="llama2-7b",le="0.001"} -50
            )";         
            break;
    }
    code = g_currentCoordinatorMockCode;
    return g_currentCoordinatorMockRet;
}

std::string GetCoordinatorCurrentInstances()
{
    return g_currentCoordinatorInstances;
}