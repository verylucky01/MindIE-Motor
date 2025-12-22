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
#include "RequestCCAEStub.h"

using namespace MINDIE::MS;

// 初始化全局变量
std::string g_currentRegisterInstances = "";
int32_t g_currentRegisterMockCode = 200;
int32_t g_currentRegisterMockRet = 0;
RegisterCode g_currentRegisterMock;

std::string g_currentInventoriesInstances = "";
int32_t g_currentInventoriesMockCode = 200;
int32_t g_currentInventoriesMockRet = 0;
InventoriesCode g_currentInventoriesMock;

void SetRequestRegisterMockCode(int32_t code)
{
    g_currentRegisterMockCode = code;
}

void SetRequestRegisterMockRet(int32_t ret)
{
    g_currentRegisterMockRet = ret;
}

void SetRequestRegisterMockCode(RegisterCode registerCode)
{
    g_currentRegisterMock = registerCode;
}

int32_t SendRequestRegisterMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    g_currentRegisterInstances = request.body;

    switch (g_currentRegisterMock) {
        case RegisterCode::SUCCESS:
            responseBody = R"({"retCode": 0,"retMsg": "success",
                "reqList": [{"modelID": "6DE620D3-3FC1-4B54-B9FA-05BAE48A873E",
                "metrics": {"metricPeriod": 1},"inventories": {"forceUpdate": false}}]})";
            break;
        case RegisterCode::INVALIDRESPONSETYPE:
            responseBody = "OK";
            break;
        case RegisterCode::INVALIDRETCODETYPE:
            responseBody = R"({"retCode": 2,"retMsg": "success",
                "reqList": [{"modelID": "6DE620D3-3FC1-4B54-B9FA-05BAE48A873E",
                "metrics": {"metricPeriod": 1},"inventories": {"forceUpdate": false}}]})";
            break;
        case RegisterCode::INVALIDRETMSGTYPE:
            responseBody = R"({"retCode": 0,"retMsg": 0,
                "reqList": [{"modelID": "6DE620D3-3FC1-4B54-B9FA-05BAE48A873E",
                "metrics": {"metricPeriod": 1},"inventories": {"forceUpdate": false}}]})";
            break;
        case RegisterCode::ERRORCODE:
            responseBody = R"({"retCode": 1,"retMsg": "Failed to connect.",
                "reqList": [{"modelID": "6DE620D3-3FC1-4B54-B9FA-05BAE48A873E",
                "metrics": {"metricPeriod": 1},"inventories": {"forceUpdate": false}}]})";
            break;
        case RegisterCode::INVALIDREQLISTTYPE:
            responseBody = R"({"retCode": 0,"retMsg": "success","reqList": []})";
            break;
        case RegisterCode::INVALIDMODELID:
            responseBody = R"({"retCode": 0,"retMsg": "success",
                "reqList": [{"modelID": "6DE620D3-3FC1-4B54-B9FA",
                "metrics": {"metricPeriod": 1},"inventories": {"forceUpdate": false}}]})";
            break;
        case RegisterCode::INVALIDMETRICPERIOD:
            responseBody = R"({"retCode": 0,"retMsg": "success",
                "reqList": [{"modelID": "6DE620D3-3FC1-4B54-B9FA-05BAE48A873E",
                "metrics": {"metricPeriod": 6},"inventories": {"forceUpdate": false}}]})";
            break;
        case RegisterCode::INVALIDFORCEUPDATE:
            responseBody = R"({"retCode": 0,"retMsg": "success",
                "reqList": [{"modelID": "6DE620D3-3FC1-4B54-B9FA-05BAE48A873E",
                "metrics": {"metricPeriod": 1},"inventories": {"forceUpdate": "OK"}}]})";
            break;
    }
    code = g_currentRegisterMockCode;
    return g_currentRegisterMockRet;
}

std::string GetRegisterCurrentInstances()
{
    return g_currentRegisterInstances;
}

void SetRequestInventoriesMockCode(int32_t code)
{
    g_currentInventoriesMockCode = code;
}

void SetRequestInventoriesMockRet(int32_t ret)
{
    g_currentInventoriesMockRet = ret;
}

void SetRequestInventoriesMockCode(InventoriesCode inventoriesCode)
{
    g_currentInventoriesMock = inventoriesCode;
}

int32_t SendRequestInventoriesMock(void* obj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    g_currentInventoriesInstances = request.body;

    switch (g_currentInventoriesMock) {
        case InventoriesCode::SUCCESS:
            responseBody = R"({"retCode": 0,"retMsg": "success"})";
            break;
        case InventoriesCode::INVALIDRESPONSETYPE:
            responseBody = "OK";
            break;
        case InventoriesCode::INVALIDRETCODETYPE:
            responseBody = R"({"retCode": 2,"retMsg": "success"})";
            break;
        case InventoriesCode::INVALIDRETMSGTYPE:
            responseBody = R"({"retCode": 0,"retMsg": "success."})";
            break;
        case InventoriesCode::ERRORCODE:
            responseBody = R"({"retCode": 1,"retMsg": "failed"})";
            break;
    }
    code = g_currentInventoriesMockCode;
    return g_currentInventoriesMockRet;
}

std::string GetInventoriesCurrentInstances()
{
    return g_currentInventoriesInstances;
}