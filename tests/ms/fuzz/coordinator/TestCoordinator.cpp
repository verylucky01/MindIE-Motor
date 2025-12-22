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
#include <random>
#include "gtest/gtest.h"
#include "secodeFuzz.h"
#include "FuzzFramework.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "common/PCommon.h"
#include "ThreadSafeVector.h"
#include "Metrics.h"

#include <cstdlib>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include "stub.h"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "Configure.h"
#include "Coordinator.h"
#include "Logger.h"
#include "Configure.h"

#define main __main_coordinator__
#include "coordinator/main.cpp"

namespace beast = boost::beast;
namespace http = beast::http;

extern void ReadFromFile(char **data, int *len, char *path);

extern void WriteToFile(char *data, int len, char *path);

class TestCoordinator : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }
    static boost::system::error_code UsePrivateKeyFileStub(
        const std::string &path, boost::asio::ssl::context::file_format format, boost::system::error_code &ec)
    {
        std::cout << "UsePrivateKeyFileStub" << std::endl;
    }

    static boost::system::error_code AddCertificateAuthorityStub(
        const boost::asio::const_buffer &ca, boost::system::error_code &ec)
    {
        std::cout << "AddCertificateAuthorityStub" << std::endl;
    }
    static bool ServerDecryptStub(boost::asio::ssl::context &mSslCtx, const MINDIE::MS::HttpServerParams &httpParams,
        std::pair<std::string, int32_t> &mPassword)
    {
        std::cout << "ServerDecryptStub" << std::endl;
        return true;
    }

    static void AssociateServiceStub()
    {
        std::cout << "AssociateServiceStub" << std::endl;
        return;
    }
    void SetUp()
    {
        // 设置监控指标环境变量
        setenv("MIES_SERVICE_MONITOR_MODE", "1", 1);

        std::string exePath = GetExecutablePath();
        std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
        msCoordinatorJson = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_coordinator.json");
        msCoordinatorBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_coordinator.bin");
        msRequestTritonBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_Triton.bin");
        msRequestTGIBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_TGI.bin");
        msRequestOpenAIBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_OpenAI.bin");
        msRequestTokenizerBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_tokenizer.bin");
        msRequestInstanceRefreshBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_refresh.bin");
        msRequestInstanceOfflineBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_offline.bin");
        msRequestDevelopBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_develop.bin");
        msResponsePBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_p_response.bin");
        jsonModule = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/json.xml");
        msMetricsBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_metrics.bin");

        CopyDefaultConfig();
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Save();
        CopyFile(msCoordinatorJson, msCoordinatorBin);
    }
    void TearDown()
    {}

    std::string jsonModule;
    std::string msCoordinatorBin;
    std::string msRequestTritonBin;
    std::string msRequestTGIBin;
    std::string msRequestOpenAIBin;
    std::string msRequestTokenizerBin;
    std::string msRequestInstanceRefreshBin;
    std::string msRequestInstanceOfflineBin;
    std::string msResponsePBin;
    std::string msRequestDevelopBin;
    std::string msCoordinatorJson;
    std::string msMetricsBin;

    Stub stub;
    uint8_t numMindIEServer = 4;
    ThreadSafeVector<std::string> predictIPList;
    ThreadSafeVector<std::string> predictPortList;
    ThreadSafeVector<std::string> managePortList;
    ThreadSafeVector<std::string> interCommonPortLists;
    std::string manageIP = "127.0.0.1";
    std::string managePort = "1026";
    std::string predictIP = "127.0.0.1";
    std::string predictPort = "1025";
};

// coordinator 部署 config初始化
TEST_F(TestCoordinator, TestCoordinatorConfigure)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);
    DT_Pits_SetBinFile(const_cast<char *>(msCoordinatorBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestCoordinatorConfigure\n");

    DT_FUZZ_START1("TestCoordinatorConfigure")
    {
        CopyDefaultConfig();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        // 写入变异后的内容
        WriteToFile(dataout, lenout, const_cast<char *>(msCoordinatorJson.c_str()));
        ChangeFileMode600(msCoordinatorJson);

        MINDIE::MS::Configure conf;
        // 调用被测函数解析变异后的文件
        conf.Init();
    }
    DT_FUZZ_END()

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinatorConfigure\n");
}

static uint32_t g_timeoutSeconds = 3;
static uint32_t g_retryTimes = 2;

// RequestListener::TritonReqHandler "/v2/models/*/generate",
TEST_F(TestCoordinator, TestCoordinatorRequest01)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);
    DT_Pits_SetBinFile(const_cast<char *>(msRequestTritonBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestCoordinator\n");

    std::string url = "/v2/models/*/generate_stream";
    DT_FUZZ_START1("TestCoordinatorRequest01")
    {
        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        std::string response;
        std::string body(dataout, lenout);
        // 调用被测函数解析变异后的文件
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}

// RequestListener::TGIStreamReqHandler '%s/generate'
TEST_F(TestCoordinator, TestCoordinatorRequest02)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);
    DT_Pits_SetBinFile(const_cast<char *>(msRequestTGIBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/generate";
    DT_FUZZ_START1("TestCoordinatorRequest02")
    {
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        std::string response;
        std::string body(dataout, lenout);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        // 调用被测函数解析变异后的文件
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}

// RequestListener::OpenAIReqHandler '%s/v1/chat/completions
TEST_F(TestCoordinator, TestCoordinatorRequest03)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);

    DT_Pits_SetBinFile(const_cast<char *>(msRequestOpenAIBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/v1/chat/completions";
    DT_FUZZ_START1("TestCoordinatorRequest03")
    {
        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        std::string response;
        std::string body(dataout, lenout);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        // 调用被测函数解析变异后的文件
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
}

// RequestListener::TokenizerReqHandler 'http://%s:%s/v1/tokenizer
TEST_F(TestCoordinator, TestCoordinatorRequest04)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);

    DT_Pits_SetBinFile(const_cast<char *>(msRequestTokenizerBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/v1/tokenizer";
    DT_FUZZ_START1("TestCoordinatorRequest04")
    {
        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        std::string response;
        std::string body(dataout, lenout);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        // 调用被测函数解析变异后的文件
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}

// RequestListener::MindIEReqHandler 'http://%s:%s/v1/infer
TEST_F(TestCoordinator, TestCoordinatorRequest05)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);

    DT_Pits_SetBinFile(const_cast<char *>(msRequestDevelopBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/infer";
    DT_FUZZ_START1("TestCoordinatorRequest05")
    {
        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        std::string response;
        std::string body(dataout, lenout);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        // 调用被测函数解析变异后的文件
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}


TEST_F(TestCoordinator, TestSingleNodeMetricsTC01)
{
    int fuzzCount = 3000;
    char *dataout = nullptr;
    int lenout = 0;
    ReadFromFile(&dataout, &lenout, msMetricsBin.c_str());
    std::string metricStr(dataout);
    if (!nlohmann::json::accept(metricStr)) {
        LOG_E("[%s] invalid json input",
            GetErrorCode(ErrorType::INVALID_INPUT, ControllerFeature::JSON_FILE_LOADER).c_str());
    }
    auto metricsJson = nlohmann::json::parse(metricStr);
    std::string originMetric = metricsJson[0]["metrics_str"];
    printf("%s\n", originMetric.c_str());

    auto t = time(nullptr);
    printf("seed: %d\n", t);
    srand(t);

    Metrics m;
    m.InitMetricsPattern();
    DT_FUZZ_START1("TestSingleNodeMetricsTC01")
    {
        for (size_t i = 0; i < originMetric.size(); i++) {
            std::string tmpStr = originMetric;
            int fuzzChar = rand() % 128; // ascii [0, 127]
            printf("%d, %d\n", i, fuzzChar);
            if (char(fuzzChar) == '\\') {
                continue;
            }
            tmpStr[i] = char(fuzzChar);
            metricsJson[0]["metrics_str"] = tmpStr;
            m.ParseMetrics(metricsJson);
        }
    }
    DT_FUZZ_END()

    printf("end ---- TestSingleNodeMetricsTC02\n");
}

// ControllerListener::InstancesRefreshHandler 'http://%s:%s/v1/instances/refresh
TEST_F(TestCoordinator, TestCoordinatorRequest06)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);
    DT_Pits_SetBinFile(const_cast<char *>(msRequestInstanceRefreshBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort)
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/v1/instances/refresh";
    DT_FUZZ_START1("TestCoordinatorRequest06")
    {
        CopyFile(GetMSCoordinatorConfigJsonPath(), GetMSCoordinatorTestJsonPath());

        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        std::string response;
        std::string body(dataout, lenout);
        std::cout << url << std::endl;
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        // 调用被测函数解析变异后的文件
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}

// ControllerListener::InstancesOfflineHandler '/v1/instances/offline
TEST_F(TestCoordinator, TestCoordinatorRequest07)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);
    DT_Pits_SetBinFile(const_cast<char *>(msRequestInstanceOfflineBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/v1/instances/offline";
    DT_FUZZ_START1("TestCoordinatorRequest07")
    {
        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        std::string response;
        std::string body(dataout);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        // 调用被测函数解析变异后的文件
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}

std::string CreateSelfDevelopRequest(std::string textInput, bool stream)
{
    nlohmann::json request;

    request["inputs"] = textInput;
    request["stream"] = stream;
    request["parameters"] = {{"temperature", 0.5},
        {"top_k", 10},
        {"top_p", 0.95},
        {"max_new_tritons", 20},
        {"do_sample", true},
        {"seed", nullptr},
        {"repetition_penalty", 1.03},
        {"details", true},
        {"typical_p", 0.5},
        {"watermark", false}};

    return request.dump();
}

char *g_mDResponse = nullptr;
uint64_t g_mDResponseSize = 0;
std::string g_stubChunk;
const std::string &GetResChunkedBodyStub()
{
    std::cout << "------------------GetResChunkedBodyStub--------------" << std::endl;
    std::cout << std::string(g_mDResponse) << std::endl;
    g_stubChunk.append(g_mDResponse, g_mDResponseSize);
    return g_stubChunk;
}

// RequestRepeater::DResChunkHandler
TEST_F(TestCoordinator, TestCoordinatorRequest08)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int fuzzCount = 30000000;

    std::string url = "/infer";

    DT_FUZZ_START1("TestCoordinatorRequest08")
    {
        TlsItems tlsItems;
        g_mDResponse = DT_SetGetString(&g_Element[1], 25, 10000, "reqId:aaa\0data:aaaaaa\0");
        g_mDResponseSize = DT_GET_MutatedValueLen(&g_Element[1]);
        int lenout = 0;

        std::string response;
        std::string body = CreateSelfDevelopRequest("aaa", false);

        stub.set(ADDR(ClientConnection, GetResChunkedBody), GetResChunkedBodyStub);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        stub.reset(ADDR(ClientConnection, GetResChunkedBody));
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}

char *g_mPResponse = nullptr;
http::response<http::dynamic_body> httpRes;
const http::response<http::dynamic_body> &GetGetResMessageStub()
{
    std::cout << "------------------GetGetResMessageStub--------------" << std::endl;
    // 清空body
    httpRes = http::response<http::dynamic_body>();
    // 设置响应的 HTTP 版本、状态码和原因短语
    httpRes.version(11);  // HTTP/1.1
    httpRes.result(http::status::ok);  // 200 OK
    httpRes.set(http::field::server, "Beast Server");  // 设置头部字段
    httpRes.set(http::field::content_type, "application/json");  // 设置 Content-Type

    // 写入响应的主体内容
    std::string body_content = g_mPResponse;

    // 将字符串内容写入到 dynamic_body 中
    beast::ostream(httpRes.body()) << body_content;
    
    // 设置 Content-Length
    httpRes.content_length(body_content.size());
    httpRes.prepare_payload();
    return httpRes;
}

// RequestRepeater::PResHandler
TEST_F(TestCoordinator, TestCoordinatorRequest09)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);
    DT_Pits_SetBinFile(const_cast<char *>(msResponsePBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/infer";
    DT_FUZZ_START1("TestCoordinatorRequest09")
    {
        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        int lenout = 0;
        DT_Pits_GetMutatorBuf(&g_mPResponse, &lenout);

        std::string response;
        std::string body = CreateSelfDevelopRequest("aaa", true);
        stub.set(ADDR(ClientConnection, GetResMessage), GetGetResMessageStub);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        stub.reset(ADDR(ClientConnection, GetResMessage));
        // 调用被测函数解析变异后的文件
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}

// RequestRepeater::TokenizerResHandler
TEST_F(TestCoordinator, TestCoordinatorRequest10)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);
    DT_Pits_SetBinFile(const_cast<char *>(msResponsePBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/v1/tokenizer";
    DT_FUZZ_START1("TestCoordinatorRequest10")
    {
        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        int lenout = 0;
        DT_Pits_GetMutatorBuf(&g_mPResponse, &lenout);

        std::string response;
        nlohmann::json request;

        request["inputs"] = "aaaa";
        std::string body = request.dump();
        stub.set(ADDR(ClientConnection, GetResMessage), GetGetResMessageStub);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        stub.reset(ADDR(ClientConnection, GetResMessage));
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestCoordinator\n");
}

// RequestListener::MindIEReqHandler 'http://%s:%s/infer_token
TEST_F(TestCoordinator, TestCoordinatorRequest11)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", const_cast<char *>(msCoordinatorJson.c_str()), 1);

    DT_Pits_SetBinFile(const_cast<char *>(msRequestDevelopBin.c_str()));

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto manageIP = predictIPList[i];
        auto predictIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << std::endl;

        threads.push_back(std::thread(CreateMindIEServer,
        std::move(manageIP), std::move(pPort),
        std::move(predictIP), std::move(mPort),
        4, i, "")); // 默认4线程启动服务
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    TlsItems tlsItems;
    auto pdInstance = SetPDRole(predictIPList, predictPortList, managePortList,
        interCommonPortLists, 2, 2, manageIP, managePort);
    WaitCoordinatorReady(manageIP, managePort);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;

    std::string url = "/infer_token";
    DT_FUZZ_START1("TestCoordinatorRequest11")
    {
        printf("\r%d\n", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        std::string response;
        std::vector<int> input_id = {0, 6657, 30568};
        std::string body = CreateSelfDevelopRequestInputId(token_id, true);
        SendInferRequest(predictIP, predictPort, tlsItems, url, body, response);
        // 调用被测函数解析变异后的文件
    }
    DT_FUZZ_END()
    threadObj.detach();  // 将线程放入后台运行
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    
    // 释放工具占用内存
    DT_Pits_ParseFree();
}

std::string CreateSelfDevelopRequestInputId(std::vector<int> input_id, bool stream)
{
    nlohmann::json request;
    request["input_id"] = input_id;
    request["stream"] = stream;
    request["parameters"] = {{"temperature", 0.5},
        {"top_k", 10},
        {"top_p", 0.95},
        {"max_new_tritons", 20},
        {"do_sample", true},
        {"seed", nullptr},
        {"repetition_penalty", 1.03},
        {"details", true},
        {"typical_p", 0.5},
        {"watermark", false}};

    return request.dump();
}