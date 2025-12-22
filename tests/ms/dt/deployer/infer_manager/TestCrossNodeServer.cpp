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
#include <fstream>
#include <string>
#include <memory>
#include <pthread.h>
#include <sys/stat.h>
#include "gtest/gtest.h"
#include "ServerManager.h"
#include "ConfigParams.h"
#include "Helper.h"
#include "stub.h"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "CrossNodeServer.cpp"
#include "StatusHandler.h"
#include "HttpServer.cpp"
#include "Util.h"
#include "RequestServerStub.h"
#include "nlohmann/json.hpp"
using namespace MINDIE::MS;

static int32_t g_returncode = 0;
static bool g_isLabeled = true;
class TestCrossNodeServer : public testing::Test {
protected:
    static int32_t ReturnTwoHundredStub(const char *format)
    {
        std::cout << "ReturnTwoHundredStub" << std::endl;
        return 200;
    }
    static nlohmann::json JsonParseStub(const char *format)
    {
        nlohmann::json ret;
        std::cout << "JsonParseStub" << std::endl;
        return ret;
    }
    static std::string ReturnStringStub(const char *format)
    {
        std::cout << "ReturnStringStub" << std::endl;
        return std::string{"abc"};
    }
    static int32_t FindAndLabelMasterPodStub(void* obj, uint32_t ind, bool &isLabeled)
    {
        std::cout << "FindAndLabelMasterPodStub" << std::endl;
        isLabeled = g_isLabeled;
        return g_returncode;
    }
    void SetUp() {}
    void TearDown() {}
};

/*
测试描述: GetDeployStatus，ret = 0, isOK = true
测试步骤:
    1. 配置mInferInstance
    2. 打桩GetFromMindIEServer，返回值不同会到达不同分支
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetDeployStatusTCa)
{
    CrossNodeServer server;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"GetDeployStatusTCa"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "127.0.0.1";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(MINDIE::MS::CrossNodeServer, GetFromMindIEServer), ReturnZeroStub);

    auto GetDeployStatusPtr = &CrossNodeServer::GetDeployStatus;
    auto ret = InvokePrivateMethod(&server, GetDeployStatusPtr);

    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, GetFromMindIEServer));

    std::cout << ret.second << std::endl;
    EXPECT_EQ(ret.first, 0);
}

/*
测试描述: GetDeployStatus，ret = -1, isOK = false
测试步骤:
    1. 配置mInferInstance
    2. 打桩GetFromMindIEServer，返回值不同会到达不同分支
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetDeployStatusTCb)
{
    CrossNodeServer server;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"GetDeployStatusTCb"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "127.0.0.1";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(MINDIE::MS::CrossNodeServer, GetFromMindIEServer), ReturnNeOneStub);

    auto GetDeployStatusPtr = &CrossNodeServer::GetDeployStatus;
    auto ret = InvokePrivateMethod(&server, GetDeployStatusPtr);

    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, GetFromMindIEServer));

    std::cout << ret.second << std::endl;
    EXPECT_EQ(ret.first, 0);
}

/*
测试描述: GetFromMindIEServer，打桩SendRequest中的返回值，code=200
测试步骤:
    1. 配置mInferInstance
    2. 打桩SendRequest，返回值不同会到达不同分支
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetFromMindIEServerTCa)
{
    CrossNodeServer server;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"GetFromMindIEServerTCa"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "127.0.0.1";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(MINDIE::MS::HttpClient, SendRequestTls), ReturnTwoHundredStub);
    stub.set(ADDR(MINDIE::MS::HttpClient, SendRequestNonTls), ReturnTwoHundredStub);

    auto GetDeployStatusPtr = &CrossNodeServer::GetDeployStatus;
    auto ret = InvokePrivateMethod(&server, GetDeployStatusPtr);

    stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequestTls));
    stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequestNonTls));

    std::cout << ret.second << std::endl;
    EXPECT_EQ(ret.first, 0);
}

/*
测试描述: GetFromMindIEServer，打桩SendRequest中的返回值，code=-1
测试步骤:
    1. 配置mInferInstance
    2. 打桩SendRequest，返回值不同会到达不同分支
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetFromMindIEServerTCb)
{
    CrossNodeServer server;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"GetFromMindIEServerTCb"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "127.0.0.1";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(MINDIE::MS::HttpClient, SendRequestTls), ReturnNeOneStub);
    stub.set(ADDR(MINDIE::MS::HttpClient, SendRequestNonTls), ReturnNeOneStub);

    auto GetDeployStatusPtr = &CrossNodeServer::GetDeployStatus;
    auto ret = InvokePrivateMethod(&server, GetDeployStatusPtr);

    stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequestTls));
    stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequestNonTls));

    std::cout << ret.second << std::endl;
    EXPECT_EQ(ret.first, 0);
}

/*
测试描述: GetServerStatus 实例未准备好分支
测试步骤:
    1. 配置mInferInstance
    2. mInferInstance[0].masterPodIp == ""
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetServerStatusTCa)
{
    CrossNodeServer server;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"GetServerStatusTCa"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(MINDIE::MS::CrossNodeServer, GetFromMindIEServer), ReturnZeroStub);

    auto GetDeployStatusPtr = &CrossNodeServer::GetDeployStatus;
    auto ret = InvokePrivateMethod(&server, GetDeployStatusPtr);

    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, GetFromMindIEServer));

    std::cout << ret.second << std::endl;
    EXPECT_EQ(ret.first, 0);
}

/*
测试描述: GetModelInfo，实例未准备好分支
测试步骤:
    1. 配置mInferInstance
    2. mInferInstance[0].masterPodIp == ""
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetModelInfoTCa)
{
    CrossNodeServer server;
    nlohmann::json modelInfo;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"GetModelInfoTCa"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    auto GetModelInfoPtr = &CrossNodeServer::GetModelInfo;
    auto ret = InvokePrivateMethod(&server, GetModelInfoPtr, modelInfo);

    EXPECT_EQ(ret, -1);
}

/*
测试描述: GetModelInfo，成功
测试步骤:
    1. 配置mInferInstance
    2. mInferInstance[0].masterPodIp == "127.0.0.1"
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetModelInfoTCb)
{
    CrossNodeServer server;
    nlohmann::json modelInfo;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"GetModelInfoTCa"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "127.0.0.1";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto GetModelInfoPtr = &CrossNodeServer::GetModelInfo;
    auto ret = InvokePrivateMethod(&server, GetModelInfoPtr, modelInfo);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, 0);
}

/*
测试描述: LabelMasterPod，SendKubeHttpRequest fail
测试步骤:
    1. 配置mInferInstance
    2. 打桩SendKubeHttpRequest return -1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, LabelMasterPodTCa)
{
    CrossNodeServer server;
    nlohmann::json modelInfo;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"LabelMasterPodTCa"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnNeOneStub);

    auto LabelMasterPodPtr = &CrossNodeServer::LabelMasterPod;
    const std::string ip = "127.0.0.1";
    uint32_t ind = 0;
    bool isLabeled = false;
    auto ret = InvokePrivateMethod(&server, LabelMasterPodPtr, ip, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: LabelMasterPod，SendKubeHttpRequest success, response = ""
测试步骤:
    1. 配置mInferInstance
    2. 打桩SendKubeHttpRequest return 0
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, LabelMasterPodTCb)
{
    CrossNodeServer server;
    nlohmann::json modelInfo;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"LabelMasterPodTCb"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnZeroStub);

    auto LabelMasterPodPtr = &CrossNodeServer::LabelMasterPod;
    const std::string ip = "127.0.0.1";
    uint32_t ind = 0;
    bool isLabeled = false;
    auto ret = InvokePrivateMethod(&server, LabelMasterPodPtr, ip, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: LabelMasterPod，SendKubeHttpRequest success, response data error
测试步骤:
    1. 配置mInferInstance
    2. 打桩SendRequest, response不符合
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, LabelMasterPodTCc)
{
    CrossNodeServer server;
    nlohmann::json modelInfo;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"LabelMasterPodTCc"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto LabelMasterPodPtr = &CrossNodeServer::LabelMasterPod;
    const std::string ip = "127.0.0.1";
    uint32_t ind = 0;
    bool isLabeled = false;
    auto ret = InvokePrivateMethod(&server, LabelMasterPodPtr, ip, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: LabelMasterPod，SendKubeHttpRequest success, response contain metadata
测试步骤:
    1. 配置mInferInstance
    2. 打桩SendKubeHttpRequest return 0
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, LabelMasterPodTCd)
{
    CrossNodeServer server;
    nlohmann::json modelInfo;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"LabelMasterPodTCd"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    g_currentResponse = R"({
    "items": [
    {
      "metadata": {
        "id": 123,
        "name": "example"
      }
    },
    {
      "metadata": {
        "id": 456,
        "name": "another example"
      }
    }
    ]
    })";

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto LabelMasterPodPtr = &CrossNodeServer::LabelMasterPod;
    const std::string ip = "127.0.0.1";
    uint32_t ind = 0;
    bool isLabeled = false;
    auto ret = InvokePrivateMethod(&server, LabelMasterPodPtr, ip, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: LabelMasterPod，SendKubeHttpRequest success, response contain namespace
测试步骤:
    1. 配置mInferInstance
    2. 打桩SendKubeHttpRequest return 0
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, LabelMasterPodTCe)
{
    CrossNodeServer server;
    nlohmann::json modelInfo;

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"LabelMasterPodTCe"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    g_currentResponse = R"({
    "items": [
    {
      "metadata": {
        "namespace": "123",
        "name": "example"
      }
    },
    {
      "metadata": {
        "id": 456,
        "name": "another example"
      }
    }
    ]
    })";

    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto LabelMasterPodPtr = &CrossNodeServer::LabelMasterPod;
    const std::string ip = "127.0.0.1";
    uint32_t ind = 0;
    bool isLabeled = false;
    auto ret = InvokePrivateMethod(&server, LabelMasterPodPtr, ip, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, 0);
}

/*
测试描述: ModifyRanktable 第一个分支失败
测试步骤:
    1. 配置mInferInstance
    2. 打桩IsJsonStringValid return -1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, ModifyRanktableTCa)
{
    CrossNodeServer server;
    const nlohmann::json ranktable = {
        {"server_list", {
            {{"container_ip", "value1"}}
        }}
    };

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"ModifyRanktableTCa"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(IsJsonStringValid, ReturnFalseStub);

    auto ModifyRanktablePtr = &CrossNodeServer::ModifyRanktable;
    uint32_t ind = 0;
    bool isLabeled = false;
    auto ret = InvokePrivateMethod(&server, ModifyRanktablePtr, ranktable, (uint32_t)ind, isLabeled);

    stub.reset(IsJsonStringValid);

    EXPECT_EQ(ret, -1);
}

/*
测试描述: ModifyRanktable 第二个分支失败
测试步骤:
    1. 配置mInferInstance
    2. 打桩IsJsonStringValid return -1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, ModifyRanktableTCb)
{
    CrossNodeServer server;
    const nlohmann::json ranktable = {
        {"server_list", {
            {{"container_ip", "value1"}}
        }}
    };

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"ModifyRanktableTCb"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(CrossNodeServer, LabelMasterPod), ReturnNeOneStub);

    auto ModifyRanktablePtr = &CrossNodeServer::ModifyRanktable;
    uint32_t ind = 0;
    bool isLabeled = false;
    auto ret = InvokePrivateMethod(&server, ModifyRanktablePtr, ranktable, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(CrossNodeServer, LabelMasterPod));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: ModifyRanktable 成功
测试步骤:
    1. 配置mInferInstance
    2. 打桩IsJsonStringValid return -1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, ModifyRanktableTCc)
{
    CrossNodeServer server;
    const nlohmann::json ranktable = {
        {"server_list", {
            {{"container_ip", "value1"}}
        }}
    };

    auto CrossNodeServer::*mServerNamePtr = &CrossNodeServer::mServerName;
    SetPrivateMember(&server, mServerNamePtr, std::string{"ModifyRanktableTCc"});
    auto a = GetPrivateMember(&server, mServerNamePtr);
    std::cout << a << std::endl;

    InferInstance sampleInferInstance;
    std::map<uint32_t, InferInstance> fakemInferInstance = {{0, sampleInferInstance}};
    fakemInferInstance[0].masterPodIp = "";
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    SetPrivateMember(&server, mInferInstancePtr, fakemInferInstance);

    Stub stub;
    stub.set(ADDR(CrossNodeServer, LabelMasterPod), ReturnZeroStub);

    auto ModifyRanktablePtr = &CrossNodeServer::ModifyRanktable;
    uint32_t ind = 0;
    bool isLabeled = false;
    auto ret = InvokePrivateMethod(&server, ModifyRanktablePtr, ranktable, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(CrossNodeServer, LabelMasterPod));

    EXPECT_EQ(ret, 0);
}

/*
测试描述: CreateService 第一个分支失败
测试步骤:
    1. 配置LoadServiceParams
    2. 打桩SendKubeHttpRequest return -1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, CreateServiceTCa)
{
    CrossNodeServer server;
    LoadServiceParams serverParamsT;
    serverParamsT.nameSpace = "nameSpace";
    serverParamsT.name = "name";

    const LoadServiceParams serverParams = serverParamsT;

    auto CreateServicePtr = &CrossNodeServer::CreateService;

    Stub stub;
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnNeOneStub);

    auto ret = InvokePrivateMethod(&server, CreateServicePtr, serverParams);

    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: FindAndLabelMasterPod 成功
测试步骤:
    1. 配置g_currentResponse
    2. 打桩SendRequest，ModifyRanktable
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, FindAndLabelMasterPodTCa)
{
    CrossNodeServer server;
    
    uint32_t ind = 0;
    bool isLabeled = false;
    std::string jsonString = R"(
    {
      "data": {
        "hccl.json": "{\"status\":\"completed\",\"server_list\":[1, 2]}"
      }
    }
    )";
    g_currentResponse = jsonString;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);
    stub.set(ADDR(CrossNodeServer, ModifyRanktable), ReturnZeroStub);

    auto FindAndLabelMasterPodPtr = &CrossNodeServer::FindAndLabelMasterPod;

    auto ret = InvokePrivateMethod(&server, FindAndLabelMasterPodPtr, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));
    stub.reset(ADDR(CrossNodeServer, ModifyRanktable));

    EXPECT_EQ(ret, 0);
}

/*
测试描述: FindAndLabelMasterPod 第一个分支
测试步骤:
    1. 配置g_currentResponse
    2. 打桩SendRequest，ModifyRanktable
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, FindAndLabelMasterPodTCb)
{
    CrossNodeServer server;
    
    uint32_t ind = 0;
    bool isLabeled = false;
    std::string jsonString = R"(
    {
      "data": {
        "hccl.json": "{\"status\":\"completed\",\"server_list\":[1]}"
      }
    }123
    )";
    g_currentResponse = jsonString;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);
    stub.set(ADDR(CrossNodeServer, ModifyRanktable), ReturnZeroStub);

    auto FindAndLabelMasterPodPtr = &CrossNodeServer::FindAndLabelMasterPod;

    auto ret = InvokePrivateMethod(&server, FindAndLabelMasterPodPtr, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));
    stub.reset(ADDR(CrossNodeServer, ModifyRanktable));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: FindAndLabelMasterPod 第二个分支
测试步骤:
    1. 配置g_currentResponse
    2. 打桩SendRequest，ModifyRanktable
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, FindAndLabelMasterPodTCc)
{
    CrossNodeServer server;
    
    uint32_t ind = 0;
    bool isLabeled = false;
    std::string jsonString = R"(
    {
      "qqq": {
        "hccl.json": "{\"status\":\"completed\",\"server_list\":[1]}"
      }
    }
    )";
    g_currentResponse = jsonString;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);
    stub.set(ADDR(CrossNodeServer, ModifyRanktable), ReturnZeroStub);

    auto FindAndLabelMasterPodPtr = &CrossNodeServer::FindAndLabelMasterPod;

    auto ret = InvokePrivateMethod(&server, FindAndLabelMasterPodPtr, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));
    stub.reset(ADDR(CrossNodeServer, ModifyRanktable));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: FindAndLabelMasterPod 第二个分支
测试步骤:
    1. 配置g_currentResponse
    2. 打桩SendRequest，ModifyRanktable
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, FindAndLabelMasterPodTCd)
{
    CrossNodeServer server;
    
    uint32_t ind = 0;
    bool isLabeled = false;
    std::string jsonString = R"(
    {
      "data": {
        "sss": "{\"status\":\"completed\",\"server_list\":[1]}"
      }
    }
    )";
    g_currentResponse = jsonString;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);
    stub.set(ADDR(CrossNodeServer, ModifyRanktable), ReturnZeroStub);

    auto FindAndLabelMasterPodPtr = &CrossNodeServer::FindAndLabelMasterPod;

    auto ret = InvokePrivateMethod(&server, FindAndLabelMasterPodPtr, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));
    stub.reset(ADDR(CrossNodeServer, ModifyRanktable));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: FindAndLabelMasterPod 第四个分支
测试步骤:
    1. 配置g_currentResponse
    2. 打桩SendRequest，ModifyRanktable
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, FindAndLabelMasterPodTCe)
{
    CrossNodeServer server;
    
    uint32_t ind = 0;
    bool isLabeled = false;
    std::string jsonString = R"(
    {
      "data": {
        "hccl.json": "{\"status\":\"completed\",\"server_list\":[1]}111"
      }
    }
    )";
    g_currentResponse = jsonString;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);
    stub.set(ADDR(CrossNodeServer, ModifyRanktable), ReturnZeroStub);

    auto FindAndLabelMasterPodPtr = &CrossNodeServer::FindAndLabelMasterPod;

    auto ret = InvokePrivateMethod(&server, FindAndLabelMasterPodPtr, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));
    stub.reset(ADDR(CrossNodeServer, ModifyRanktable));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: FindAndLabelMasterPod 第五个分支
测试步骤:
    1. 配置g_currentResponse
    2. 打桩SendRequest，ModifyRanktable
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, FindAndLabelMasterPodTCf)
{
    CrossNodeServer server;
    
    uint32_t ind = 0;
    bool isLabeled = false;
    std::string jsonString = R"(
    {
      "data": {
        "hccl.json": "{\"sss\":\"completed\",\"server_list\":[1]}"
      }
    }
    )";
    g_currentResponse = jsonString;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);
    stub.set(ADDR(CrossNodeServer, ModifyRanktable), ReturnZeroStub);

    auto FindAndLabelMasterPodPtr = &CrossNodeServer::FindAndLabelMasterPod;

    auto ret = InvokePrivateMethod(&server, FindAndLabelMasterPodPtr, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));
    stub.reset(ADDR(CrossNodeServer, ModifyRanktable));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: FindAndLabelMasterPod 第六个分支
测试步骤:
    1. 配置g_currentResponse
    2. 打桩SendRequest，ModifyRanktable
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, FindAndLabelMasterPodTCg)
{
    CrossNodeServer server;
    
    uint32_t ind = 0;
    bool isLabeled = false;
    std::string jsonString = R"(
    {
      "data": {
        "hccl.json": "{\"status\":\"aaa\",\"server_list\":[1]}"
      }
    }
    )";
    g_currentResponse = jsonString;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);
    stub.set(ADDR(CrossNodeServer, ModifyRanktable), ReturnZeroStub);

    auto FindAndLabelMasterPodPtr = &CrossNodeServer::FindAndLabelMasterPod;

    auto ret = InvokePrivateMethod(&server, FindAndLabelMasterPodPtr, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));
    stub.reset(ADDR(CrossNodeServer, ModifyRanktable));

    EXPECT_EQ(ret, 0);
}

/*
测试描述: FindAndLabelMasterPod 最后一个分支
测试步骤:
    1. 配置g_currentResponse
    2. 打桩SendRequest，ModifyRanktable
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, FindAndLabelMasterPodTCh)
{
    CrossNodeServer server;
    
    uint32_t ind = 0;
    bool isLabeled = false;
    std::string jsonString = R"(
    {
      "data": {
        "hccl.json": "{\"status\":\"completed\",\"server_list\":[1]}"
      }
    }
    )";
    g_currentResponse = jsonString;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);
    stub.set(ADDR(CrossNodeServer, ModifyRanktable), ReturnNeOneStub);

    auto FindAndLabelMasterPodPtr = &CrossNodeServer::FindAndLabelMasterPod;

    auto ret = InvokePrivateMethod(&server, FindAndLabelMasterPodPtr, (uint32_t)ind, isLabeled);

    stub.reset(ADDR(HttpClient, SendRequest));
    stub.reset(ADDR(CrossNodeServer, ModifyRanktable));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: GetMasterPodIPByLabel 成功
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetMasterPodIPByLabelTCa)
{
    CrossNodeServer server;
    const std::string serverName = "serverName";
    const std::string deploymentName = "deploymentName";
    std::string podIP = "127.0.0.1";
    g_currentResponse = R"(
    {
      "items": [
        {
          "status": {
            "podIP": "192.168.1.100"
          }
        }
      ]
    }
    )";
    auto GetMasterPodIPByLabelPtr = &CrossNodeServer::GetMasterPodIPByLabel;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto ret = InvokePrivateMethod(&server, GetMasterPodIPByLabelPtr, serverName, deploymentName, podIP);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, 0);
}

/*
测试描述: GetMasterPodIPByLabel master pod not found.
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetMasterPodIPByLabelTCb)
{
    CrossNodeServer server;
    const std::string serverName = "serverName";
    const std::string deploymentName = "deploymentName";
    std::string podIP = "127.0.0.1";
    g_currentResponse = R"(
    {
      "items": [
        {
          "status": {
            "podIP": "192.168.1.100"
          }
        }
      ]
    }
    )";
    auto GetMasterPodIPByLabelPtr = &CrossNodeServer::GetMasterPodIPByLabel;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnNeOneStub);

    auto ret = InvokePrivateMethod(&server, GetMasterPodIPByLabelPtr, serverName, deploymentName, podIP);

    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: GetMasterPodIPByLabel format of pod information is not valid json
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetMasterPodIPByLabelTCc)
{
    CrossNodeServer server;
    const std::string serverName = "serverName";
    const std::string deploymentName = "deploymentName";
    std::string podIP = "127.0.0.1";
    g_currentResponse = R"(
    {
      "items": [
        {
          "status": {
            "podIP": "192.168.1.100"
          }
        }
      ]
    }ddd
    )";
    auto GetMasterPodIPByLabelPtr = &CrossNodeServer::GetMasterPodIPByLabel;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto ret = InvokePrivateMethod(&server, GetMasterPodIPByLabelPtr, serverName, deploymentName, podIP);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: GetMasterPodIPByLabel Pod information does not contain items[0]
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetMasterPodIPByLabelTCd)
{
    CrossNodeServer server;
    const std::string serverName = "serverName";
    const std::string deploymentName = "deploymentName";
    std::string podIP = "127.0.0.1";
    g_currentResponse = R"(
    {
      "ddd": [
        {
          "status": {
            "podIP": "192.168.1.100"
          }
        }
      ]
    }
    )";
    auto GetMasterPodIPByLabelPtr = &CrossNodeServer::GetMasterPodIPByLabel;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto ret = InvokePrivateMethod(&server, GetMasterPodIPByLabelPtr, serverName, deploymentName, podIP);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: GetMasterPodIPByLabel Pod information does not contain status
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetMasterPodIPByLabelTCe)
{
    CrossNodeServer server;
    const std::string serverName = "serverName";
    const std::string deploymentName = "deploymentName";
    std::string podIP = "127.0.0.1";
    g_currentResponse = R"(
    {
      "items": [
        {
          "fff": {
            "podIP": "192.168.1.100"
          }
        }
      ]
    }
    )";
    auto GetMasterPodIPByLabelPtr = &CrossNodeServer::GetMasterPodIPByLabel;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto ret = InvokePrivateMethod(&server, GetMasterPodIPByLabelPtr, serverName, deploymentName, podIP);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: GetMasterPodIPByLabel podIP error
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, GetMasterPodIPByLabelTCf)
{
    CrossNodeServer server;
    const std::string serverName = "serverName";
    const std::string deploymentName = "deploymentName";
    std::string podIP = "127.0.0.1";
    g_currentResponse = R"(
    {
      "items": [
        {
          "status": {
            "sss": "192.168.1.100"
          }
        }
      ]
    }
    )";
    auto GetMasterPodIPByLabelPtr = &CrossNodeServer::GetMasterPodIPByLabel;
    Stub stub;
    stub.set(ADDR(HttpClient, SendRequest), &SendRequestMock);

    auto ret = InvokePrivateMethod(&server, GetMasterPodIPByLabelPtr, serverName, deploymentName, podIP);

    stub.reset(ADDR(HttpClient, SendRequest));

    EXPECT_EQ(ret, -1);
}

/*
测试描述: CreateInstanceResource 成功
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, CreateInstanceResourceTCa)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    const std::string nameSpace = "nameSpace";
    const std::string ranktableName = "ranktableName";
    const std::string deplymentName = "deplymentName";
    ResourcesInfo mKubeResourcesF;
    mKubeResourcesF.deploymentJsons.push_back(std::string{"abc"});

    auto CreateInstanceResourcePtr = &CrossNodeServer::CreateInstanceResource;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnZeroStub);
    stub.set(CreateDeployJson, ReturnStringStub);

    auto CrossNodeServer::*mKubeResourcesPtr = &CrossNodeServer::mKubeResources;
    SetPrivateMember(&server, mKubeResourcesPtr, mKubeResourcesF);
    auto ret = InvokePrivateMethod(&server, CreateInstanceResourcePtr, (uint32_t)ind,
        nameSpace, ranktableName, deplymentName);
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(CreateDeployJson);
    EXPECT_EQ(ret, 0);
}

/*
测试描述: CreateInstanceResource Restoring instance: Create ranktable ConfigMap failed!
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, CreateInstanceResourceTCb)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    const std::string nameSpace = "nameSpace";
    const std::string ranktableName = "ranktableName";
    const std::string deplymentName = "deplymentName";
    ResourcesInfo mKubeResourcesF;
    mKubeResourcesF.deploymentJsons.push_back(std::string{"abc"});

    auto CreateInstanceResourcePtr = &CrossNodeServer::CreateInstanceResource;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnNeOneStub);
    stub.set(CreateDeployJson, ReturnStringStub);

    auto CrossNodeServer::*mKubeResourcesPtr = &CrossNodeServer::mKubeResources;
    SetPrivateMember(&server, mKubeResourcesPtr, mKubeResourcesF);
    auto ret = InvokePrivateMethod(&server, CreateInstanceResourcePtr, (uint32_t)ind,
        nameSpace, ranktableName, deplymentName);
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(CreateDeployJson);
    EXPECT_EQ(ret, -1);
}

/*
测试描述: CreateInstanceResource 成功, mKubeResources.deploymentJsons.size() == 0
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, CreateInstanceResourceTCc)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    const std::string nameSpace = "nameSpace";
    const std::string ranktableName = "ranktableName";
    const std::string deplymentName = "deplymentName";
    ResourcesInfo mKubeResourcesF;

    auto CreateInstanceResourcePtr = &CrossNodeServer::CreateInstanceResource;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnZeroStub);
    stub.set(CreateDeployJson, ReturnStringStub);

    auto CrossNodeServer::*mKubeResourcesPtr = &CrossNodeServer::mKubeResources;
    SetPrivateMember(&server, mKubeResourcesPtr, mKubeResourcesF);
    auto ret = InvokePrivateMethod(&server, CreateInstanceResourcePtr, (uint32_t)ind,
        nameSpace, ranktableName, deplymentName);
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(CreateDeployJson);
    EXPECT_EQ(ret, 0);
}

/*
测试描述: RetryLabelMasterPod 成功
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RetryLabelMasterPodTCa)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    RestoreState restoreState;
    g_findMaterPodInterval = 0;
    g_returncode = 0;
    g_isLabeled = true;

    auto RetryLabelMasterPodPtr = &CrossNodeServer::RetryLabelMasterPod;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, FindAndLabelMasterPod), &FindAndLabelMasterPodStub);

    auto ret = InvokePrivateMethod(&server, RetryLabelMasterPodPtr, (uint32_t)ind, restoreState);
    stub.reset(ADDR(CrossNodeServer, FindAndLabelMasterPod));
    EXPECT_EQ(ret, 0);
}

/*
测试描述: RetryLabelMasterPod PENDING
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RetryLabelMasterPodTCb)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    RestoreState restoreState = RestoreState::PENDING;
    g_findMaterPodInterval = 0;
    g_returncode = 0;
    g_isLabeled = false;

    auto RetryLabelMasterPodPtr = &CrossNodeServer::RetryLabelMasterPod;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, FindAndLabelMasterPod), &FindAndLabelMasterPodStub);

    auto ret = InvokePrivateMethod(&server, RetryLabelMasterPodPtr, (uint32_t)ind, restoreState);
    stub.reset(ADDR(CrossNodeServer, FindAndLabelMasterPod));
    EXPECT_EQ(ret, -1);
}

/*
测试描述: RetryLabelMasterPod isLabled false
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RetryLabelMasterPodTCc)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    RestoreState restoreState = RestoreState::NONE;
    g_findMaterPodInterval = 0;
    g_returncode = 0;
    g_isLabeled = false;

    auto RetryLabelMasterPodPtr = &CrossNodeServer::RetryLabelMasterPod;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, FindAndLabelMasterPod), &FindAndLabelMasterPodStub);

    auto ret = InvokePrivateMethod(&server, RetryLabelMasterPodPtr, (uint32_t)ind, restoreState);
    stub.reset(ADDR(CrossNodeServer, FindAndLabelMasterPod));
    EXPECT_EQ(ret, 0);
}

/*
测试描述: RetryLabelMasterPod FindAndLabelMasterPod -1
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RetryLabelMasterPodTCd)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    RestoreState restoreState = RestoreState::NONE;
    g_findMaterPodInterval = 0;
    g_returncode = -1;
    g_isLabeled = true;

    auto RetryLabelMasterPodPtr = &CrossNodeServer::RetryLabelMasterPod;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, FindAndLabelMasterPod), &FindAndLabelMasterPodStub);

    auto ret = InvokePrivateMethod(&server, RetryLabelMasterPodPtr, (uint32_t)ind, restoreState);
    stub.reset(ADDR(CrossNodeServer, FindAndLabelMasterPod));
    EXPECT_EQ(ret, 0);
}

/*
测试描述: RecoverInstance成功 Pendind and RetryLabelMasterPod == 0
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RecoverInstanceTCa)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    InferInstance instacne;
    instacne.restoreState = RestoreState::PENDING;

    auto RecoverInstancePtr = &CrossNodeServer::RecoverInstance;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, RetryLabelMasterPod), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, CreateInstanceResource), ReturnZeroStub);

    auto ret = InvokePrivateMethod(&server, RecoverInstancePtr, (uint32_t)ind, instacne);
    stub.reset(ADDR(CrossNodeServer, RetryLabelMasterPod));
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(ADDR(CrossNodeServer, CreateInstanceResource));
    EXPECT_EQ(ret, 0);
}

/*
测试描述: RecoverInstance成功 instacne.restoreState == RestoreState::NONE
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RecoverInstanceTCb)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    InferInstance instacne;
    instacne.restoreState = RestoreState::NONE;

    auto RecoverInstancePtr = &CrossNodeServer::RecoverInstance;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, RetryLabelMasterPod), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, CreateInstanceResource), ReturnZeroStub);

    auto ret = InvokePrivateMethod(&server, RecoverInstancePtr, (uint32_t)ind, instacne);
    stub.reset(ADDR(CrossNodeServer, RetryLabelMasterPod));
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(ADDR(CrossNodeServer, CreateInstanceResource));
    EXPECT_EQ(ret, 0);
}

/*
测试描述: RecoverInstance成功 instacne.restoreState == RestoreState::PENDING, Retry == -1
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RecoverInstanceTCc)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    InferInstance instacne;
    instacne.restoreState = RestoreState::PENDING;

    auto RecoverInstancePtr = &CrossNodeServer::RecoverInstance;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, RetryLabelMasterPod), ReturnNeOneStub);
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, CreateInstanceResource), ReturnZeroStub);

    auto ret = InvokePrivateMethod(&server, RecoverInstancePtr, (uint32_t)ind, instacne);
    stub.reset(ADDR(CrossNodeServer, RetryLabelMasterPod));
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(ADDR(CrossNodeServer, CreateInstanceResource));
    EXPECT_EQ(ret, -1);
}

/*
测试描述: RecoverInstance成功 instacne.restoreState == RestoreState::NONE, SendKubeHttpRequest == -1
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RecoverInstanceTCd)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    InferInstance instacne;
    instacne.restoreState = RestoreState::NONE;

    auto RecoverInstancePtr = &CrossNodeServer::RecoverInstance;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, RetryLabelMasterPod), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnNeOneStub);
    stub.set(ADDR(CrossNodeServer, CreateInstanceResource), ReturnZeroStub);

    auto ret = InvokePrivateMethod(&server, RecoverInstancePtr, (uint32_t)ind, instacne);
    stub.reset(ADDR(CrossNodeServer, RetryLabelMasterPod));
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(ADDR(CrossNodeServer, CreateInstanceResource));
    EXPECT_EQ(ret, -1);
}

/*
测试描述: RecoverInstance成功 instacne.restoreState == RestoreState::NONE, CreateInstanceResource == -1
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RecoverInstanceTCe)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    InferInstance instacne;
    instacne.restoreState = RestoreState::NONE;

    auto RecoverInstancePtr = &CrossNodeServer::RecoverInstance;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, RetryLabelMasterPod), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, CreateInstanceResource), ReturnNeOneStub);

    auto ret = InvokePrivateMethod(&server, RecoverInstancePtr, (uint32_t)ind, instacne);
    stub.reset(ADDR(CrossNodeServer, RetryLabelMasterPod));
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(ADDR(CrossNodeServer, CreateInstanceResource));
    EXPECT_EQ(ret, -1);
}

/*
测试描述: RecoverInstance成功 instacne.restoreState == RestoreState::NONE, RetryLabelMasterPod == -1
测试步骤:
    1. 配置配置项
    2. 打桩SendKubeHttpRequest，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, RecoverInstanceTCf)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    InferInstance instacne;
    instacne.restoreState = RestoreState::NONE;

    auto RecoverInstancePtr = &CrossNodeServer::RecoverInstance;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, RetryLabelMasterPod), ReturnNeOneStub);
    stub.set(ADDR(CrossNodeServer, SendKubeHttpRequest), ReturnZeroStub);
    stub.set(ADDR(CrossNodeServer, CreateInstanceResource), ReturnZeroStub);

    auto ret = InvokePrivateMethod(&server, RecoverInstancePtr, (uint32_t)ind, instacne);
    stub.reset(ADDR(CrossNodeServer, RetryLabelMasterPod));
    stub.reset(ADDR(CrossNodeServer, SendKubeHttpRequest));
    stub.reset(ADDR(CrossNodeServer, CreateInstanceResource));
    EXPECT_EQ(ret, -1);
}

/*
测试描述: CheckInstanceStatus recover fail
测试步骤:
    1. 配置配置项
    2. 打桩RecoverInstance，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, CheckInstanceStatusTCa)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    InferInstance instacne;
    instacne.status = InstanceStatus::ABNORMAL;
    instacne.deploymentName = "CheckInstanceStatusTCa";

    auto CheckInstanceStatusPtr = &CrossNodeServer::CheckInstanceStatus;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, RecoverInstance), ReturnNeOneStub);

    InvokePrivateMethod(&server, CheckInstanceStatusPtr, (uint32_t)ind, instacne);
    stub.reset(ADDR(CrossNodeServer, RecoverInstance));
}

/*
测试描述: CheckInstanceStatus recover fail
测试步骤:
    1. 配置配置项
    2. 打桩RecoverInstance，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, CheckInstanceStatusTCb)
{
    CrossNodeServer server;
    uint32_t ind = 0;
    InferInstance instacne;
    instacne.status = InstanceStatus::ABNORMAL;
    instacne.deploymentName = "CheckInstanceStatusTCb";

    auto CheckInstanceStatusPtr = &CrossNodeServer::CheckInstanceStatus;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, RecoverInstance), ReturnZeroStub);

    InvokePrivateMethod(&server, CheckInstanceStatusPtr, (uint32_t)ind, instacne);
    stub.reset(ADDR(CrossNodeServer, RecoverInstance));
}

/*
测试描述: UpdateInstanceStatus Instance Ready
测试步骤:
    1. 配置配置项
    2. 打桩RecoverInstance，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, UpdateInstanceStatusTCa)
{
    CrossNodeServer server;
    InferInstance instance;
    instance.status = InstanceStatus::UNREADY;
    instance.deploymentName = "UpdateInstanceStatusTCa";
    instance.masterPodIp = "127.0.0.1";
    LoadServiceParams mServiceParams;
    mServiceParams.mindieServerMngPort = 8888;
    mServiceParams.livenessTimeout = 0;
    std::map<uint32_t, InferInstance> mInferInstance;
    uint32_t id = 0;
    mInferInstance.insert({id, instance});
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    auto CrossNodeServer::*mServiceParamsPtr = &CrossNodeServer::mServiceParams;
    SetPrivateMember(&server, mInferInstancePtr, mInferInstance);
    SetPrivateMember(&server, mServiceParamsPtr, mServiceParams);

    auto UpdateInstanceStatusPtr = &CrossNodeServer::UpdateInstanceStatus;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, GetFromMindIEServer), ReturnZeroStub);
    InvokePrivateMethod(&server, UpdateInstanceStatusPtr);
    stub.reset(ADDR(CrossNodeServer, RecoverInstance));
}

/*
测试描述: UpdateInstanceStatus instance.status == InstanceStatus::READY
测试步骤:
    1. 配置配置项
    2. 打桩RecoverInstance，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, UpdateInstanceStatusTCb)
{
    CrossNodeServer server;
    InferInstance instance;
    instance.status = InstanceStatus::READY;
    instance.deploymentName = "UpdateInstanceStatusTCa";
    instance.masterPodIp = "127.0.0.1";
    LoadServiceParams mServiceParams;
    mServiceParams.mindieServerMngPort = 8888;
    mServiceParams.livenessTimeout = 0;
    std::map<uint32_t, InferInstance> mInferInstance;
    uint32_t id = 0;
    mInferInstance.insert({id, instance});
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    auto CrossNodeServer::*mServiceParamsPtr = &CrossNodeServer::mServiceParams;
    SetPrivateMember(&server, mInferInstancePtr, mInferInstance);
    SetPrivateMember(&server, mServiceParamsPtr, mServiceParams);

    auto UpdateInstanceStatusPtr = &CrossNodeServer::UpdateInstanceStatus;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, GetFromMindIEServer), ReturnNeOneStub);
    InvokePrivateMethod(&server, UpdateInstanceStatusPtr);
    stub.reset(ADDR(CrossNodeServer, RecoverInstance));
}

/*
测试描述: UpdateInstanceStatus instance.status == InstanceStatus::READY
测试步骤:
    1. 配置配置项
    2. 打桩RecoverInstance，
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCrossNodeServer, UpdateInstanceStatusTCc)
{
    CrossNodeServer server;
    InferInstance instance;
    instance.status = InstanceStatus::UNREADY;
    instance.deploymentName = "UpdateInstanceStatusTCa";
    instance.masterPodIp = "127.0.0.1";
    auto pastTime = std::chrono::system_clock::now() - std::chrono::seconds(10);
    instance.masterPodCreateTime =  std::chrono::duration_cast<std::chrono::seconds>(pastTime.time_since_epoch());
    LoadServiceParams mServiceParams;
    mServiceParams.mindieServerMngPort = 8888;
    mServiceParams.livenessTimeout = 0;
    mServiceParams.initDealy = 0;
    
    std::map<uint32_t, InferInstance> mInferInstance;
    uint32_t id = 0;
    mInferInstance.insert({id, instance});
    auto CrossNodeServer::*mInferInstancePtr = &CrossNodeServer::mInferInstance;
    auto CrossNodeServer::*mServiceParamsPtr = &CrossNodeServer::mServiceParams;
    SetPrivateMember(&server, mInferInstancePtr, mInferInstance);
    SetPrivateMember(&server, mServiceParamsPtr, mServiceParams);

    auto UpdateInstanceStatusPtr = &CrossNodeServer::UpdateInstanceStatus;
    Stub stub;
    stub.set(ADDR(CrossNodeServer, GetFromMindIEServer), ReturnNeOneStub);
    InvokePrivateMethod(&server, UpdateInstanceStatusPtr);
    stub.reset(ADDR(CrossNodeServer, RecoverInstance));
}