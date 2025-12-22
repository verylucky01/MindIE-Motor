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
#define main __main_server__
#define g_maxPort g_maxServerPort
#define g_minPort g_minServerPort
#define PrintHelp ServerPrintHelp
#include "deployer/server/main.cpp"
#define g_maxPort g_maxClientPort
#define g_minPort g_minClientPort
#define main __main_client__
#define PrintHelp ClientPrintHelp
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include "stub.h"
#include "deployer/msctl/main.cpp"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "CrossNodeServer.cpp"
#include "CommMock.cpp"

extern void ReadFromFile(char **data, int *len, char *path);

extern void WriteToFile(char *data, int len, char *path);

Stub g_deployerStub;
int32_t ObtainJsonValueStub(nlohmann::json mainObj, HttpClientParams &clientParams,
    HttpServerParams &httpServerParams)
{
    g_deployerStub.reset(ObtainJsonValue);
    ObtainJsonValue(mainObj, clientParams, httpServerParams);
    httpServerParams.serverTlsItems.tlsEnable = false;
    g_deployerStub.set(ObtainJsonValue, ObtainJsonValueStub);
    return 0;
}

int32_t ParseInitConfigStub(MSCtlParams &params)
{
    g_deployerStub.reset(ParseInitConfig);
    ParseInitConfig(params);
    params.clientTlsItems.tlsEnable = false;
    g_deployerStub.set(ParseInitConfig, ParseInitConfigStub);

    return 0;
}


int32_t SendDeployerRespondStub(MINDIE::MS::HttpClient selfObj, const Request &request,
    int timeoutSeconds, int retries, std::string& responseBody, int32_t &code)
{
    if (StartsWith(request.target, "/v1/servers")) {
        g_deployerStub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
        auto ret = selfObj.SendRequest(request, timeoutSeconds, retries, responseBody, code);
        g_deployerStub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SendDeployerRespondStub);
        return ret;
    }
    std::cout << "-------------------------------------------------------------------------------------------------"
        << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------"
        << std::endl;
    std::cout << "-------------------------------------------------------------------------------------------------"
        << std::endl;

    return GetDeployerRespond(request, responseBody, code);
}

class TestMindIEServer : public testing::Test {
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

        g_deployerStub.set(ObtainJsonValue, ObtainJsonValueStub);
        g_deployerStub.set(ParseInitConfig, ParseInitConfigStub);
        g_deployerStub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SendDeployerRespondStub);
        SetDefaultDeployerRespond();
    }

    void TearDown()
    {
        g_deployerStub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
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

// client 多机服务部署文件fuzz PASS
TEST_F(TestMindIEServer, TestMindIEServerCrossNodeLoadServer)
{
    JsonFileManager manager(inferServerBin);
    manager.Load();
    manager.Set("server_type", "mindie_cross_node");
    manager.Save();

    DT_Pits_SetBinFile(const_cast<char *>(inferServerBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEServerCrossNodeLoadServer\n");

    auto lamda = [&]() {
        // 调用被测函数解析变异后的文件
        char arg1[100] = "ms_server";
        char* file = const_cast<char*>(msServerJson.c_str());
        char *args[2] = {arg1, file};
        int32_t argc = 2;
        __main_server__(argc, args);
    };

    std::thread threadObj(lamda); // 创建线程
    EXPECT_TRUE(threadObj.joinable());

    DT_FUZZ_START1("TestMindIEServerCrossNodeLoadServer")
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

        // 调用被测函数解析变异后的文件
        char procName[100] = "msctl";
        char *argsCreate[5] = {procName, "create", "infer_server", "-f", const_cast<char*>(inferServerJson.c_str())};
        int32_t argNum = 5;
        __main_client__(argNum, argsCreate);

        char *argsGet[5] = {procName, "get", "infer_server", "-n", "mindie-server"};
        __main_client__(argNum, argsGet);

        char *argsUpdate[5] = {procName, "update", "infer_server", "-f", const_cast<char*>(inferServerJson.c_str())};
        __main_client__(argNum, argsUpdate);

        char *argsDelete[5] = {procName, "delete", "infer_server", "-n", "mindie-server"};
        __main_client__(argNum, argsDelete);

        CrossNodeServer nodeServer;

        stub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SendDeployerRespondStub);
        nodeServer.Deploy(dataout);
        nodeServer.Update(dataout);
        nodeServer.Unload();
        stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
    }
    DT_FUZZ_END()

    EXPECT_TRUE(threadObj.joinable());
    threadObj.detach(); // 将线程放入后台运行
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEServerCrossNodeLoadServer\n");
}


// client 多机服务部署文件fuzz PASS
TEST_F(TestMindIEServer, TestMindIEServerCrossNodeLoadServerTC02)
{
    JsonFileManager manager(inferServerBin);
    manager.Load();
    manager.Set("server_type", "mindie_cross_node");
    manager.Save();

    DT_Pits_SetBinFile(const_cast<char *>(inferServerBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEServerCrossNodeLoadServerTC02\n");

    auto lamda = [&]() {
        // 调用被测函数解析变异后的文件
        char arg1[100] = "ms_server";
        char* file = const_cast<char*>(msServerJson.c_str());
        char *args[2] = {arg1, file};
        int32_t argc = 2;
        __main_server__(argc, args);
    };

    std::thread threadObj(lamda); // 创建线程
    EXPECT_TRUE(threadObj.joinable());

    DT_FUZZ_START1("TestMindIEServerCrossNodeLoadServerTC02")
    {
        CopyDefaultConfig();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        ChangeFileMode600(inferServerJson);

        CrossNodeServer nodeServer;
        HttpClientParams httpClientCfg;
        httpClientCfg.mindieClientTlsItems.tlsEnable = false;
        const std::string statusPath;
        nodeServer.Init(httpClientCfg, statusPath);
        stub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SendDeployerRespondStub);
        nodeServer.Deploy(dataout);
        nodeServer.Update(dataout);
        nodeServer.GetDeployStatus();
        nodeServer.Unload();
        nodeServer.GetDeployStatus();
        stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
    }
    DT_FUZZ_END()

    EXPECT_TRUE(threadObj.joinable());
    threadObj.detach(); // 将线程放入后台运行
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEServerCrossNodeLoadServerTC02\n");
}

// server 启动配置的测试
TEST_F(TestMindIEServer, TestMindIEServerStartServer)
{
    DT_Pits_SetBinFile(const_cast<char *>(msServerBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEServerStartServer\n");

    DT_FUZZ_START1("TestMindIEServerStartServer")
    {
        CopyDefaultConfig();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        // 写入变异后的内容
        WriteToFile(dataout, lenout, const_cast<char*>(msServerJson.c_str()));
        ChangeFileMode600(msServerJson);
        MINDIE::MS::HttpClientParams clientParams;
        MINDIE::MS::HttpServerParams serverParams;
        std::string testStatusJson;
        auto ret = ParseCommandArgs(const_cast<char*>(msServerJson.c_str()), clientParams,
            serverParams, testStatusJson);

        MINDIE::MS::ServerManager serverManager(clientParams);

        DT_Pits_OneRunFree();
    }
    DT_FUZZ_END()

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEServerStartServer\n");
}

// server 重建配置文件
TEST_F(TestMindIEServer, TestMindIEServerServerStatus)
{
    DT_Pits_SetBinFile(const_cast<char *>(statusBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEServerServerStatus\n");

    DT_FUZZ_START1("TestMindIEServerServerStatus")
    {
        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        // 写入变异后的内容
        
        WriteToFile(dataout, lenout, const_cast<char*>(statusJson.c_str()));
        ChangeFileMode600(statusJson);

        MINDIE::MS::HttpClientParams clientParams;
        MINDIE::MS::HttpServerParams serverParams;
        int32_t ret = ParseCommandArgs(const_cast<char*>(msServerJson.c_str()), clientParams,
            serverParams, statusJson);

        MINDIE::MS::ServerManager serverManager(clientParams);

        serverManager.FromStatusFile(statusJson);

        DT_Pits_OneRunFree();
    }

    DT_FUZZ_END()

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEServerServerStatus\n");
}

// client启动配置的测试 PASS
TEST_F(TestMindIEServer, TestMindIEServerCreateStartMSctl)
{
    DT_Pits_SetBinFile(const_cast<char *>(msctlBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEServerCreateStartMSctl\n");

    auto lamda = [&]() {
        // 调用被测函数解析变异后的文件
        char arg1[100] = "ms_server";
        char* file = const_cast<char*>(msServerJson.c_str());
        char *args[2] = {arg1, file};
        int32_t argc = 2;
        __main_server__(argc, args);
    };

    std::thread threadObj(lamda); // 创建线程
    EXPECT_TRUE(threadObj.joinable());

    DT_FUZZ_START1("TestMindIEServerCreateStartMSctl")
    {
        CopyDefaultConfig();
        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        // 写入变异后的内容
        WriteToFile(dataout, lenout, const_cast<char *>(msctlJson.c_str()));
        // 调用被测函数解析变异后的文件
        char arg1[100] = "msctl";
        char *args1[5] = {arg1, "create", "infer_server", "-f", const_cast<char *>(inferServerJson.c_str())};
        int32_t argc = 5;
        __main_client__(argc, args1);

        char *args2[5] = {arg1, "get", "infer_server", "-n", "mindie-server"};
        __main_client__(argc, args2);

        char *args3[5] = {arg1, "delete", "infer_server", "-n", "mindie-server"};
        __main_client__(argc, args3);

        DT_Pits_OneRunFree();
    }
    DT_FUZZ_END();
    EXPECT_TRUE(threadObj.joinable());
    threadObj.detach(); // 将线程放入后台运行

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEServerCreateStartMSctl\n");
}