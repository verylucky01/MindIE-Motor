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
#include "gtest/gtest.h"
#include "secodeFuzz.h"
#include "FuzzFramework.h"
#include "Helper.h"
#include "common/PCommon.h"
#include "ServerManager.h"
#define private public
#define protected public
#define main __single_main_server__
#define g_maxPort g_maxServerPort
#define g_minPort g_minServerPort
#define PrintHelp ServerPrintHelp
#include "deployer/server/main.cpp"
#define g_maxPort g_maxClientPort
#define g_minPort g_minClientPort
#define main __single_main_client__
#define PrintHelp ClientPrintHelp
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include "stub.h"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "deployer/msctl/main.cpp"
#include "SingleNodeServer.cpp"
#include "CommMock.cpp"

extern void ReadFromFile(char **data, int *len, char *path);

extern void WriteToFile(char *data, int len, char *path);

Stub g_SingledeployerStub;

int32_t SingleObtainJsonValueStub(nlohmann::json mainObj, HttpClientParams &clientParams,
    HttpServerParams &httpServerParams)
{
    g_SingledeployerStub.reset(ObtainJsonValue);
    ObtainJsonValue(mainObj, clientParams, httpServerParams);
    httpServerParams.serverTlsItems.tlsEnable = false;
    g_SingledeployerStub.set(ObtainJsonValue, SingleObtainJsonValueStub);
    return 0;
}

int32_t SingleParseInitConfigStub(MSCtlParams &params)
{
    g_SingledeployerStub.reset(ParseInitConfig);
    ParseInitConfig(params);
    params.clientTlsItems.tlsEnable = false;
    g_SingledeployerStub.set(ParseInitConfig, SingleParseInitConfigStub);

    return 0;
}

int32_t SingleSendDeployerRespondStub(MINDIE::MS::HttpClient selfObj, const Request &request,
    int timeoutSeconds, int retries, std::string& responseBody, int32_t &code)
{
    if (StartsWith(request.target, "/v1/servers")) {
        g_SingledeployerStub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
        selfObj.SendRequest(request, timeoutSeconds, retries, responseBody, code);
        g_SingledeployerStub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SingleSendDeployerRespondStub);
    }

    return GetDeployerRespond(request, responseBody, code);
}

class TestMindIESingleServer : public testing::Test {
protected:
    void SetUp()
    {
        std::string exePath = GetExecutablePath();
        std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
        msServerJson = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_server.json");
        msServerBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_server.bin");
        msctlJson = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/msctl.json");
        msctlBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/msctl.bin");
        inferServerJson = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/infer_server.json");
        inferServerBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/infer_server.bin");
        statusJson = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_deploy_status.json");
        statusBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_deploy_status.bin");
        jsonModule = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/json.xml");

        JsonFileManager manager(inferServerJson);
        manager.Load();
        manager.Set("server_type", "mindie_single_node");
        manager.Save();

        CopyDefaultConfig();
        CopyFile(msServerJson, msServerBin);
        CopyFile(msctlJson, msctlBin);
        CopyFile(inferServerJson, inferServerBin);

        auto msctlJson = GetMSClientTestJsonPath();
        auto homePath = GetParentPath(GetParentPath(msctlJson));

        std::cout << "deployer:" << msctlJson << std::endl;
        setenv("HOME", homePath.c_str(), 1);
        std::cout << "HOME=" << homePath << std::endl;
        auto hseceasyPath = GetHSECEASYPATH();
        setenv("HSECEASY_PATH", hseceasyPath.c_str(), 1);
        std::cout << "HSECEASY_PATH=" << hseceasyPath << std::endl;

        g_SingledeployerStub.set(ObtainJsonValue, SingleObtainJsonValueStub);
        g_SingledeployerStub.set(ParseInitConfig, SingleParseInitConfigStub);
        g_SingledeployerStub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SingleSendDeployerRespondStub);
        SetDefaultDeployerRespond();
    }

    void TearDown()
    {
        g_SingledeployerStub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
    }
    std::string msServerJson;
    std::string msServerBin;
    std::string msctlJson;
    std::string msctlBin;
    std::string inferServerJson;
    std::string statusJson;
    std::string inferServerBin;
    std::string statusBin;
    std::string jsonModule;
    std::string serviceName;
    Stub stub;
};

// client 单机服务部署文件fuzz PASS
TEST_F(TestMindIESingleServer, TestMindIESingleServerSingleNodeLoadServer)
{
    DT_Pits_SetBinFile(const_cast<char *>(inferServerBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIESingleServerSingleNodeLoadServer\n");

    auto lamda = [&]() {
        // 调用被测函数解析变异后的文件
        char arg1[100] = "ms_server";
        char* file = const_cast<char*>(msServerJson.c_str());
        char *args[2] = {arg1, file};
        int32_t argc = 2;
        __single_main_server__(argc, args);
    };

    std::thread threadObj(lamda); // 创建线程
    EXPECT_TRUE(threadObj.joinable());

    DT_FUZZ_START1("TestMindIESingleServerSingleNodeLoadServer")
    {
        CopyDefaultConfig();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        // 写入变异后的内容
        WriteToFile(dataout, lenout, const_cast<char*>(inferServerJson.c_str()));
        
        ChangeFileMode600(inferServerJson);

        SingleNodeServer nodeServer;
        nodeServer.Deploy(dataout);
        nodeServer.Update(dataout);
        nodeServer.Unload();

        // 调用被测函数解析变异后的文件
        char procName[100] = "msctl";
        char *argsCreate[5] = {procName, "create", "infer_server", "-f", const_cast<char*>(inferServerJson.c_str())};
        int32_t argNum = 5;
        __single_main_client__(argNum, argsCreate);

        char *argsGet[5] = {procName, "get", "infer_server", "-n", "mindie-server"};
        __single_main_client__(argNum, argsGet);

        char *argsUpdate[5] = {procName, "update", "infer_server", "-f", const_cast<char*>(inferServerJson.c_str())};
        __single_main_client__(argNum, argsUpdate);

        char *argsDelete[5] = {procName, "delete", "infer_server", "-n", "mindie-server"};
        __single_main_client__(argNum, argsDelete);
    }
    DT_FUZZ_END()

    EXPECT_TRUE(threadObj.joinable());
    threadObj.detach(); // 将线程放入后台运行
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIESingleServerSingleNodeLoadServer\n");
}


// client 单机服务部署文件fuzz PASS
TEST_F(TestMindIESingleServer, TestMindIESingleServerSingleNodeLoadServerTC02)
{
    DT_Pits_SetBinFile(const_cast<char *>(inferServerBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIESingleServerSingleNodeLoadServer\n");

    auto lamda = [&]() {
        // 调用被测函数解析变异后的文件
        char arg1[100] = "ms_server";
        char* file = const_cast<char*>(msServerJson.c_str());
        char *args[2] = {arg1, file};
        int32_t argc = 2;
        __single_main_server__(argc, args);
    };

    std::thread threadObj(lamda); // 创建线程
    EXPECT_TRUE(threadObj.joinable());

    DT_FUZZ_START1("TestMindIESingleServerSingleNodeLoadServerTC02")
    {
        CopyDefaultConfig();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        SingleNodeServer nodeServer;
        HttpClientParams httpClientCfg;
        httpClientCfg.mindieClientTlsItems.tlsEnable = false;
        const std::string statusPath;
        nodeServer.Init(httpClientCfg, statusPath);
        nodeServer.Deploy(dataout);
        nodeServer.Update(dataout);
        nodeServer.GetDeployStatus();
        nodeServer.Unload();
        nodeServer.GetDeployStatus();
    }
    DT_FUZZ_END()

    EXPECT_TRUE(threadObj.joinable());
    threadObj.detach(); // 将线程放入后台运行
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIESingleServerSingleNodeLoadServer\n");
}