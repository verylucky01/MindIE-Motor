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
#include "JsonFileManager.h"
#include "common/PCommon.h"

#include <cstdlib>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include "stub.h"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "ControllerConfig.h"
#include "RankTableLoader.h"
#include "ProcessManager.h"
#include "NodeStatus.h"
#include "NodeScheduler.h"
#include "ConfigParams.h"
#include "Controller.h"

#define private public
#define protected public
#include "ServerRequestHandler.h"
#include "RoleSwitcher.h"


using namespace MINDIE::MS;

extern void ReadFromFile(char **data, int *len, char *path);

extern void WriteToFile(char *data, int len, char *path);

// 打桩SendRequest
json g_msRespondMap = nlohmann::json::array();

void SetDefaultRespond()
{
    g_msRespondMap.clear();
    // k8s api server的响应
    std::vector<std::string> postEmptyRespond = {
        "/v1/instances/offline",
        "/v1/instances/online",
        "/v1/instances/refresh",
        "/v1/role/decode",
        "/v1/role/prefill"

    };
    for (auto url:postEmptyRespond) {
        json respond = json();
        respond["url"] = url;
        respond["method"] = "POST";
        respond["respond"] = "";
        respond["status_code"] = 200;
        g_msRespondMap.push_back(respond);
    }

    {
        json respond = json();
        respond["url"] = "/v1/coordinator_info";
        respond["method"] = "GET";
        json res;
        res["schedule_info"] = json();
        nlohmann::json elemBody;
        elemBody["id"] = rand() % 1000;
        elemBody["allocated_slots"] = rand() % 1000;
        elemBody["allocated_blocks"] = rand() % 1000;
        res["schedule_info"].emplace_back(elemBody);
        nlohmann::json summaryBody;
        summaryBody["input_len"] = rand() % 1000;
        summaryBody["output_len"] = rand() % 2000;
        res["request_length_info"] = summaryBody;
        respond["respond"] = res.dump();
        respond["status_code"] = 200;
        g_msRespondMap.push_back(respond);
    }

    {
        json respond = json();
        respond["url"] = "/v1/instances/tasks";
        respond["method"] = "GET";
        json res;
        res["tasks"] = nlohmann::json::array();
        res["tasks"].emplace_back(1);

        respond["respond"] = res.dump();
        respond["status_code"] = 200;
        g_msRespondMap.push_back(respond);
    }

    {
        json respond = json();
        respond["url"] = "/v1/instances/tasks?id=16781740";
        respond["method"] = "GET";
        json res;
        res["tasks"] = nlohmann::json::array();
        res["tasks"].emplace_back(1);

        respond["respond"] = res.dump();
        respond["status_code"] = 200;
        g_msRespondMap.push_back(respond);
    }

    {
        json respond = json();
        respond["url"] = "/v1/instances/query_tasks";
        respond["method"] = "POST";
        json res;
        srand(time(nullptr)); // 初始化随机数种子
        res["is_end"] = rand() % 2 == 0;
        respond["respond"] = res.dump();
        respond["status_code"] = 200;
        g_msRespondMap.push_back(respond);
    }

    {
        json respond = json();
        respond["url"] = "/v1/config";
        respond["method"] = "GET";
        json res;
        res["modelName"] = "llama_65b";
        res["maxSeqLen"] = 2560;
        res["npuMemSize"] = 8;
        res["cpuMemSize"] = 5;
        res["worldSize"] = 8;
        res["maxOutputLen"] = 512;
        res["cacheBlockSize"] = 128;

        respond["respond"] = res.dump();
        respond["status_code"] = 200;
        g_msRespondMap.push_back(respond);
    }

    {
        json respond = json();
        respond["url"] = "/v1/status";
        respond["method"] = "GET";
        json res;
        res["service"] = json();

        srand(time(nullptr)); // 初始化随机数种子
        std::vector<std::string> roleStatus = {"RoleReady", "RoleSwitching", "RoleUnknown", "invalid"} ;
        std::vector<std::string> currentRoles = {"prefill", "decode", "invalid"} ;
        res["service"]["roleStatus"] = roleStatus[rand() % 4];
        res["service"]["currentRole"] = currentRoles[rand() % 3];

        res["resource"] = json();
        res["resource"]["availSlotsNum"] = rand() % 10000;
        res["resource"]["availBlockNum"] = rand() % 10000;
        res["resource"]["waitingRequestNum"] = 0;
        res["resource"]["runningRequestNum"] = 0;
        res["resource"]["swappedRequestNum"] = 0;
        res["resource"]["freeNpuBlockNums"] = 0;
        res["resource"]["freeCpuBlockNums"] = 0;
        res["resource"]["totalNpuBlockNums"] = 0;
        res["resource"]["totalCpuBlockNums"] = 0;
        
        if (res["service"]["currentRole"] == "decode") {
            res["peers"] = nlohmann::json::array();
        }
        respond["respond"] = res.dump();
        respond["status_code"] = 200;
        g_msRespondMap.push_back(respond);
    }
}

int32_t GetRespond(const Request &request, std::string& responseBody, int32_t &code)
{
    std::string target = request.target;
    std::string method = "";
    if (request.method == boost::beast::http::verb::get) {
        method = "GET";
    } else if (request.method == boost::beast::http::verb::post) {
        method = "POST";
    } else if (request.method == boost::beast::http::verb::delete_) {
        method = "DELETE";
    } else if (request.method == boost::beast::http::verb::patch) {
        method = "PATCH";
    }

    for (auto respond : g_msRespondMap) {
        if (respond["url"] == target && respond["method"] == method) {
            responseBody = respond["respond"];
            code = respond["status_code"];
            break;
        }
    }
    if (code != 200) {
        return -1;
    }
    return 0;
}

Stub g_Stub;

int32_t SendRespondStub(MINDIE::MS::HttpClient selfObj, const Request &request,
    int timeoutSeconds, int retries, std::string& responseBody, int32_t &code)
{
    auto ret = GetRespond(request, responseBody, code);
    std::cout<< "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa:  Stub happen! ret: " << ret << std::endl;
    std::cout<< "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb:  request.target: " << request.target << std::endl;
    return ret;
}

// 获取绝对路径的通用函数
std::string GetPath(std::string halfPath)
{
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    return GetAbsolutePath(parentPath, halfPath);
}

// 预声明函数
int MakeStatusJson();

// 定义Test Class
class TestController : public testing::Test {
protected:
    
    void SetUp()
    {
        MakeStatusJson();

        jsonModule = GetPath("tests/ms/common/.mindie_ms/json.xml");
        msControllerJson = GetPath("tests/ms/common/.mindie_ms/ms_controller.json");
        msControllerBin = GetPath("tests/ms/common/.mindie_ms/ms_controller.bin");
        mRankTableBin = GetPath("tests/ms/common/.mindie_ms/global_rank_table.bin");
        mRankTableJson = GetPath("tests/ms/common/.mindie_ms/global_rank_table.json");
        mStatusJsonJson = GetPath("tests/ms/common/.mindie_ms/status_json.json");
        mStatusJsonBin = GetPath("tests/ms/common/.mindie_ms/status_json.bin");

        mResponseBin = GetPath("tests/ms/common/.mindie_ms/response_1.bin");

        CopyDefaultConfig();
        CopyFile(msControllerJson, msControllerBin);
        CopyFile(mRankTableJson, mRankTableBin);
        CopyFile(mStatusJsonJson, mStatusJsonBin);

        InitHttpClient();
        InitNodeStatus02();

        InitNodeStatus();
        InitCoordinatorStore();

        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", const_cast<char *>(msControllerJson.c_str()), 1);
        setenv("GLOBAL_RANK_TABLE_FILE_PATH", const_cast<char *>(mRankTableJson.c_str()), 1);
        ChangeFileMode600(msControllerJson);
        ChangeFileMode600(mRankTableJson);
        g_Stub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SendRespondStub);
        SetDefaultRespond();
    }
    void TearDown()
    {
        g_Stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
    }
    std::string jsonModule;
    std::string msControllerBin;
    std::string msControllerJson;
    std::string mRankTableBin;
    std::string mRankTableJson;
    std::string mStatusJsonJson;
    std::string mStatusJsonBin;

    std::string mResponseBin;

    void InitNodeStatus()
    {
        nodeStatus = std::make_shared<NodeStatus>();
        auto nodeInfo1 = std::make_unique<NodeInfo>();
        nodeInfo1->ip = "127.0.0.1";
        nodeInfo1->port = "1026";
        nodeInfo1->isHealthy = true;
        nodeInfo1->isInitialized = true;
        nodeInfo1->roleState = "running";
        nodeInfo1->modelName = "model1";
        nodeInfo1->peers = { 33558956 };
        nodeInfo1->activePeers = { 33558956 };
        nodeInfo1->instanceInfo.staticInfo.id = 16781740;
        nodeInfo1->instanceInfo.staticInfo.groupId = 1;
        nodeInfo1->instanceInfo.staticInfo.maxSeqLen = 2048;
        nodeInfo1->instanceInfo.staticInfo.maxOutputLen = 512;
        nodeInfo1->instanceInfo.staticInfo.totalSlotsNum = 200;
        nodeInfo1->instanceInfo.staticInfo.totalBlockNum = 1024;
        nodeInfo1->instanceInfo.staticInfo.blockSize = 128;
        nodeInfo1->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER;
        nodeInfo1->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        nodeInfo1->instanceInfo.dynamicInfo.availSlotsNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.availBlockNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.waitingRequestNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.runningRequestNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.swappedRequestNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.freeNpuBlockNums = 0;
        nodeInfo1->instanceInfo.dynamicInfo.freeCpuBlockNums = 0;
        nodeInfo1->instanceInfo.dynamicInfo.totalNpuBlockNums = 0;
        nodeInfo1->instanceInfo.dynamicInfo.totalCpuBlockNums = 0;
        nodeStatus->AddNode(std::move(nodeInfo1));
    }
    void InitCoordinatorStore()
    {
        coordinatorStore = std::make_shared<CoordinatorStore>();
        auto coordinatorInfo = std::make_unique<Coordinator>();
        coordinatorInfo->ip = "127.0.0.2";
        coordinatorInfo->port = "8080";
        std::vector<std::unique_ptr<Coordinator>> coordinatorVec;
        coordinatorVec.push_back(std::move(coordinatorInfo));
        coordinatorStore->UpdateCoordinators(coordinatorVec);
    }
    std::shared_ptr<NodeStatus> nodeStatus;
    std::shared_ptr<CoordinatorStore> coordinatorStore;

    void InitNodeStatus02()
    {
        nodeStatus02 = std::make_shared<NodeStatus>();
        auto nodeInfo1 = std::make_unique<NodeInfo>();
        nodeInfo1->ip = host;
        nodeInfo1->port = port;
        nodeInfo1->isHealthy = true;
        nodeInfo1->isInitialized = true;
        nodeInfo1->roleState = "running";
        nodeInfo1->modelName = "model1";
        nodeInfo1->peers = { 50336172 };
        nodeInfo1->activePeers = {};
        nodeInfo1->instanceInfo.staticInfo.id = 16781740;
        nodeInfo1->instanceInfo.staticInfo.groupId = 1;
        nodeInfo1->instanceInfo.staticInfo.maxSeqLen = 2048;
        nodeInfo1->instanceInfo.staticInfo.maxOutputLen = 512;
        nodeInfo1->instanceInfo.staticInfo.totalSlotsNum = 200;
        nodeInfo1->instanceInfo.staticInfo.totalBlockNum = 1024;
        nodeInfo1->instanceInfo.staticInfo.blockSize = 128;
        nodeInfo1->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER;
        nodeInfo1->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        nodeInfo1->instanceInfo.dynamicInfo.availSlotsNum = 0;
        nodeInfo1->instanceInfo.dynamicInfo.availBlockNum = 0;
        nodeStatus02->AddNode(std::move(nodeInfo1));
        auto nodeInfo2 = std::make_unique<NodeInfo>();
        nodeInfo2->ip = "127.0.0.2";
        nodeInfo2->port = port;
        nodeInfo2->isHealthy = true;
        nodeInfo2->isInitialized = true;
        nodeInfo2->roleState = "running";
        nodeInfo2->modelName = "model1";
        nodeInfo2->peers = { 50336172 };
        nodeInfo2->activePeers = {};
        nodeInfo2->instanceInfo.staticInfo.id = 33558956;
        nodeInfo2->instanceInfo.staticInfo.groupId = 1;
        nodeInfo2->instanceInfo.staticInfo.maxSeqLen = 2048;
        nodeInfo2->instanceInfo.staticInfo.maxOutputLen = 512;
        nodeInfo2->instanceInfo.staticInfo.totalSlotsNum = 200;
        nodeInfo2->instanceInfo.staticInfo.totalBlockNum = 1024;
        nodeInfo2->instanceInfo.staticInfo.blockSize = 128;
        nodeInfo2->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER;
        nodeInfo2->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        nodeInfo2->instanceInfo.dynamicInfo.availSlotsNum = 0;
        nodeInfo2->instanceInfo.dynamicInfo.availBlockNum = 0;
        nodeStatus02->AddNode(std::move(nodeInfo2));
        auto nodeInfo3 = std::make_unique<NodeInfo>();
        nodeInfo3->ip = "127.0.0.3";
        nodeInfo3->port = port;
        nodeInfo3->isHealthy = true;
        nodeInfo3->isInitialized = true;
        nodeInfo3->roleState = "running";
        nodeInfo3->modelName = "model1";
        nodeInfo3->peers = { 16781740, 33558956 };
        nodeInfo3->activePeers = { 16781740, 33558956 };
        nodeInfo3->instanceInfo.staticInfo.id = 50336172;
        nodeInfo3->instanceInfo.staticInfo.groupId = 1;
        nodeInfo3->instanceInfo.staticInfo.maxSeqLen = 2048;
        nodeInfo3->instanceInfo.staticInfo.maxOutputLen = 512;
        nodeInfo3->instanceInfo.staticInfo.totalSlotsNum = 200;
        nodeInfo3->instanceInfo.staticInfo.totalBlockNum = 1024;
        nodeInfo3->instanceInfo.staticInfo.blockSize = 128;
        nodeInfo3->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER;
        nodeInfo3->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        nodeInfo3->instanceInfo.dynamicInfo.availSlotsNum = 0;
        nodeInfo3->instanceInfo.dynamicInfo.availBlockNum = 0;
        nodeStatus02->AddNode(std::move(nodeInfo3));
    }
    void InitHttpClient()
    {
        client = std::make_shared<HttpClient>();
        TlsItems tlsItems;
        tlsItems.tlsEnable = false;
        host = "127.0.0.1";
        port = "1026";
        client->Init(host, port, tlsItems, false);
    }
    std::shared_ptr<NodeStatus> nodeStatus02;
    std::shared_ptr<HttpClient> client;
    std::string host;
    std::string port;
    TlsItems tlsItems;

    Stub stub;
};

int MakeStatusJson()
{
    nlohmann::json statusJson = {
        {"server", {
            {
                {"ip", "172.17.0.1"},
                {"isHealthy", true},
                {"static_info", {
                    {"id", 16781740},
                    {"role", MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE},
                    {"group_id", 1},
                    {"max_seq_len", 512},
                    {"max_output_len", 256},
                    {"total_slots_num", 64},
                    {"total_block_num", 32},
                    {"block_size", 1024},
                    {"label", MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER}
                }},
                {"is_initialized", true},
                {"model_name", "Model_X"},
                {"peers", {33558956}}
            },
            {
                {"ip", "172.17.0.2"},
                {"isHealthy", true},
                {"static_info", {
                    {"id", 33558956},
                    {"role", MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE},
                    {"group_id", 1},
                    {"max_seq_len", 1024},
                    {"max_output_len", 512},
                    {"total_slots_num", 128},
                    {"total_block_num", 64},
                    {"block_size", 2048},
                    {"label", MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER}
                }},
                {"is_initialized", true},
                {"model_name", "Model_Y"},
                {"peers", {16781740}}
            }
        }}
    };
    // 获取路径，保存为json文件
    auto path = GetMSControllerStatusJsonTestJsonPath();
    // 打开文件进行写入
    std::ofstream outputFile(path);
    // 检查文件是否打开成功
    if (!outputFile.is_open()) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return 1;
    }
    // 将 jsonObj 对象写入文件
    outputFile << statusJson.dump();
    // 关闭文件
    outputFile.close();
    return 0;
}

nlohmann::json ReadFromJsonFile(std::string path)
{
    // 打开文件进行读取
    std::ifstream inputFile(path);
    // 检查文件是否打开成功
    if (!inputFile.is_open()) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return 1;
    }
    // 创建一个 nlohmann::json 对象
    nlohmann::json jsonObj;
    // 从文件中读取 JSON 数据并解析为 jsonObj 对象
    try {
        inputFile >> jsonObj;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON: " << e.what() << std::endl;
        return 1;
    }
    // 关闭文件
    inputFile.close();
    return jsonObj;
}


// —_—_—_—_—_—_—_—_—_—_—_—_—_—_—_—controller 测试 ControllerConfig::Init()_—_—_—_—_—_—_—_—_—_—_—_—_—_—_—_—_
TEST_F(TestController, TestMindIEControllerConfigure)
{
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", const_cast<char *>(msControllerJson.c_str()), 1);
    DT_Pits_SetBinFile(const_cast<char *>(msControllerBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEControllerConfigure\n");

    DT_FUZZ_START1("TestMindIEControllerConfigure")
    {
        CopyDefaultConfig();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        // 写入变异后的内容
        WriteToFile(dataout, lenout, const_cast<char*>(msControllerJson.c_str()));

        ChangeFileMode600(msControllerJson);
        // 调用被测函数解析变异后的文件
        ControllerConfig::GetInstance()->Init();
    }
    DT_FUZZ_END()

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEControllerConfigure\n");
}

Stub g_ranktableStub;
void RunForPDSeparateStub(NodeScheduler selfObj)
{
    selfObj.mRun.store(false);
    g_ranktableStub.reset(ADDR(MINDIE::MS::NodeScheduler, RunForPDSeparate));
    selfObj.RunForPDSeparate();
}
void InitServerClusterStub(NodeScheduler selfObj)
{
    g_ranktableStub.reset(ADDR(MINDIE::MS::NodeScheduler, InitServerCluster));
    selfObj.InitServerCluster();
    selfObj.mRun.store(false);
}

// —_—_—_—_—_—_—_—_—_—_—_—_—_—_—_controller 测试 RankTableLoader::LoadRankTable—_—_—_—_—_—_—_—_—_—_—_—_—_
TEST_F(TestController, TestMindIEControllerLoadRankTable)
{
    DT_Pits_SetBinFile(const_cast<char *>(mRankTableBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEControllerLoadRankTable\n");

    DT_FUZZ_START1("TestMindIEControllerLoadRankTable")
    {
        CopyDefaultConfig();
        SetDefaultRespond();

        JsonFileManager manager(msControllerJson);
        manager.Load();
        manager.Set("server_online_attempt_times", 1);
        manager.Set("server_online_wait_seconds", 1);
        manager.Save();

        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        // 写入变异后的内容
        WriteToFile(dataout, lenout, const_cast<char*>(mRankTableJson.c_str()));

        ChangeFileMode600(mRankTableJson);
        ChangeFileMode600(msControllerJson);
        // 调用被测函数解析变异后的文件
        
        Controller controller;
        if (controller.Init() != 0) {
            std::cout << "Test controller Init failed. " << std::endl;
            continue;
        }

        g_ranktableStub.set(ADDR(MINDIE::MS::NodeScheduler, RunForPDSeparate), RunForPDSeparateStub);
        g_ranktableStub.set(ADDR(MINDIE::MS::NodeScheduler, InitServerCluster), InitServerClusterStub);

        if (controller.Run() != 0) {
            std::cout << "Test controller Run failed. " << std::endl;
            continue;
        }
        
        DT_Pits_OneRunFree();
    }
    DT_FUZZ_END()

    g_ranktableStub.reset(ADDR(MINDIE::MS::NodeScheduler, RunForPDSeparate));
    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEControllerLoadRankTable\n");
}

// —_—_—_—_—_—_—_—_—_—_—_—_—_—_—_—controller 测试 ProcessManager::GetStatusFromPath—_—_—_—_—_—_—_—_—_—_
TEST_F(TestController, TestMindIEControllerGetStatusFromPath)
{
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", const_cast<char *>(msControllerJson.c_str()), 1);

    if (MakeStatusJson()) {
        printf("MakeStatusJson Failed!!!\n");
    }
    DT_Pits_SetBinFile(const_cast<char *>(mStatusJsonBin.c_str()));

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEControllerGetStatusFromPath\n");

    DT_FUZZ_START1("TestMindIEControllerGetStatusFromPath")
    {
        CopyDefaultConfig();
        MakeStatusJson();   // 忘记这句导致一直crash，诶

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        char *dataout = nullptr;
        int lenout = 0;
        DT_Pits_GetMutatorBuf(&dataout, &lenout);

        // 写入变异后的内容
        WriteToFile(dataout, lenout, const_cast<char*>(mStatusJsonJson.c_str()));
        ChangeFileMode600(msControllerJson);

        // 调用被测函数解析变异后的文件
        std::vector<std::unique_ptr<NodeInfo>> servers;
        auto nodeInfo1 = std::make_unique<NodeInfo>();
        nodeInfo1->ip = "172.17.0.1";
        nodeInfo1->instanceInfo.staticInfo.id = 16781740;
        nodeInfo1->instanceInfo.staticInfo.groupId = 1;
        servers.emplace_back(std::move(nodeInfo1));

        auto nodeInfo2 = std::make_unique<NodeInfo>();
        nodeInfo2->ip = "172.17.0.2";
        nodeInfo2->instanceInfo.staticInfo.id = 33558956;
        nodeInfo2->instanceInfo.staticInfo.groupId = 1;
        servers.emplace_back(std::move(nodeInfo2));

        std::vector<std::unique_ptr<NodeInfo>> availableServers;
        std::vector<std::unique_ptr<NodeInfo>> faultyServers;
        GroupInfo groupInfo;
        std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups = groupInfo.groups;
        std::map<uint64_t, std::vector<uint64_t>> &flexGroup = groupInfo.flexGroups;
        
        auto statusJson = ReadFromJsonFile(mStatusJsonJson);
        ControllerConfig::GetInstance()->Init();
        ProcessManager::GetInstance()->Init(nodeStatus02);
        ProcessManager::GetInstance()->GetStatusFromPath(statusJson, servers, availableServers, faultyServers,
                                                         groupInfo);
        sleep(3);
        ProcessManager::GetInstance()->Stop();  // 之前析构出了问题，致使processManager的init一致crash
}
    DT_FUZZ_END()

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEControllerGetStatusFromPath\n");
}


// —_—_—_—_—_—_—_—_—_—_—_—_—_—_—_controller 测试 RoleSwitcher::QueryInstanceTasks—_—_—_—_—_—_—_—_—_—_—_
char *g_mResponse = nullptr;

int MakeResponseBin(uint64_t id, uint64_t waitSeconds, std::string mResponseBin,
                    std::shared_ptr<CoordinatorStore>& coordinatorStore)
{
    std::string response;
    json res;
    res["tasks"] = nlohmann::json::array();
    res["tasks"].emplace_back(0);
    response = res.dump();

    // 保存response为bin文件
    std::ofstream outputFile(mResponseBin, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "无法打开输出文件" << std::endl;
        return 1;
    }
    outputFile.write(response.c_str(), response.size());
    outputFile.close();
    std::cout << "JSON对象已保存到 bin文件" << std::endl;
    return 0;
}

int32_t SendRequestStub(MINDIE::MS::HttpClient selfObj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    responseBody = g_mResponse;
    code = 200;
    return 0;
}


TEST_F(TestController, TestMindIEControllerQueryInstanceTasks)
{
    ChangeFileMode600(msControllerJson);

    DT_Pits_SetBinFile(const_cast<char *>(mResponseBin.c_str()));
    // 产生bin文件
    MakeResponseBin(16781740, 10, mResponseBin, coordinatorStore);
    // 打桩
    stub.set(ADDR(HttpClient, SendRequest), SendRequestStub);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEControllerQueryInstanceTasks\n");

    DT_FUZZ_START1("TestMindIEControllerQueryInstanceTasks")
    {
        CopyDefaultConfig();
        ControllerConfig::GetInstance()->Init();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        int lenout = 0;
        DT_Pits_GetMutatorBuf(&g_mResponse, &lenout);

        // 构造一个roleSwitcher实例
        auto roleSwitcher = RoleSwitcher(nodeStatus, coordinatorStore);
        roleSwitcher.Init();
        // 调用被测函数解析变异后的文件
        roleSwitcher.QueryInstanceTasks(16781740, 10);
    }
    DT_FUZZ_END()
    stub.reset(ADDR(HttpClient, SendRequest));

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEControllerQueryInstanceTasks\n");
}


// —_—_—_—_—_—_—_—_—_—_—_—_—_controller 测试 ServerRequestHandler::QueryInstanceInfo—_—_—_—_—_—_—_—_—_—_—_
char *g_mResponse02 = nullptr;
std::string mResponseBin02 = GetPath("tests/ms/common/.mindie_ms/response_body_02.bin");

int MakeResponseBin02()
{
    std::string response;
    json res;
    res["modelName"] = "llama_65b";
    res["maxSeqLen"] = 2560;
    res["npuMemSize"] = 8;
    res["cpuMemSize"] = 5;
    res["worldSize"] = 8;
    res["maxOutputLen"] = 512;
    res["cacheBlockSize"] = 128;
    response = res.dump();

    // 保存response为bin文件
    std::ofstream outputFile(mResponseBin02, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "无法打开输出文件" << std::endl;
        return 1;
    }
    outputFile.write(response.c_str(), response.size());
    outputFile.close();
    std::cout << "JSON对象已保存到 bin文件" << std::endl;
    return 0;
}

int32_t SendRequestStub02(MINDIE::MS::HttpClient selfObj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    responseBody = g_mResponse02;
    code = 200;
    return 0;
}

TEST_F(TestController, TestMindIEControllerQueryInstanceInfo)
{
    ChangeFileMode600(msControllerJson);

    DT_Pits_SetBinFile(const_cast<char *>(mResponseBin02.c_str()));
    // 产生bin文件
    MakeResponseBin02();
    // 打桩
    stub.set(ADDR(HttpClient, SendRequest), SendRequestStub02);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEControllerQueryInstanceInfo\n");

    DT_FUZZ_START1("TestMindIEControllerQueryInstanceInfo")
    {
        CopyDefaultConfig();
        ControllerConfig::GetInstance()->Init();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        int lenout = 0;
        DT_Pits_GetMutatorBuf(&g_mResponse02, &lenout);

        // 调用被测函数解析变异后的文件
        NodeInfo nodeInfo1;
        nodeInfo1.ip = "172.17.0.1";
        nodeInfo1.instanceInfo.staticInfo.id = 16781740;
        nodeInfo1.instanceInfo.staticInfo.groupId = 1;
        ServerRequestHandler::GetInstance()->QueryInstanceInfo(*client, nodeInfo1);
    }
    DT_FUZZ_END()
    stub.reset(ADDR(HttpClient, SendRequest));

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEControllerQueryInstanceInfo\n");
}


// —_—_—_—_—_—_—_—_—_—_—_—_—_controller 测试 ServerRequestHandler::UpdateNodeStatus—_—_—_—_—_—_—_—_—_—_—_
char *g_mResponse03 = nullptr;
std::string mResponseBin03 = GetPath("tests/ms/common/.mindie_ms/response_body_03.bin");

int MakeResponseBin03()
{
    std::string response;
    json res;
    res["service"] = json();

    srand(time(nullptr)); // 初始化随机数种子
    std::vector<std::string> roleStatus = {"RoleReady", "RoleSwitching", "RoleUnknown", "invalid"} ;
    std::vector<std::string> currentRoles = {"prefill", "decode", "invalid"} ;
    res["service"]["roleStatus"] = roleStatus[rand() % 4];
    res["service"]["currentRole"] = currentRoles[rand() % 3];

    res["resource"] = json();
    res["resource"]["availSlotsNum"] = rand() % 10000;
    res["resource"]["availBlockNum"] = rand() % 10000;
    res["resource"]["waitingRequestNum"] = 0;
    res["resource"]["runningRequestNum"] = 0;
    res["resource"]["swappedRequestNum"] = 0;
    res["resource"]["freeNpuBlockNums"] = 0;
    res["resource"]["freeCpuBlockNums"] = 0;
    res["resource"]["totalNpuBlockNums"] = 0;
    res["resource"]["totalCpuBlockNums"] = 0;
    
    if (res["service"]["currentRole"] == "decode") {
        res["peers"] = nlohmann::json::array();
    }
    response = res.dump();

    // 保存response为bin文件
    std::ofstream outputFile(mResponseBin03, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "无法打开输出文件" << std::endl;
        return 1;
    }
    outputFile.write(response.c_str(), response.size());
    outputFile.close();
    std::cout << "JSON对象已保存到 bin文件" << std::endl;
    return 0;
}

int32_t SendRequestStub03(MINDIE::MS::HttpClient selfObj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    responseBody = g_mResponse03;
    code = 200;
    return 0;
}

TEST_F(TestController, TestMindIEControllerUpdateNodeStatus)
{
    ChangeFileMode600(msControllerJson);

    DT_Pits_SetBinFile(const_cast<char *>(mResponseBin03.c_str()));
    // 产生bin文件
    MakeResponseBin03();
    // 打桩
    stub.set(ADDR(HttpClient, SendRequest), SendRequestStub03);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEControllerUpdateNodeStatus\n");

    DT_FUZZ_START1("TestMindIEControllerUpdateNodeStatus")
    {
        CopyDefaultConfig();
        ControllerConfig::GetInstance()->Init();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        int lenout = 0;
        DT_Pits_GetMutatorBuf(&g_mResponse03, &lenout);

        // 调用被测函数解析变异后的文件
        NodeInfo nodeInfo1;
        nodeInfo1.ip = "172.17.0.1";
        nodeInfo1.instanceInfo.staticInfo.id = 16781740;
        nodeInfo1.instanceInfo.staticInfo.groupId = 1;
        nodeInfo1.instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        nodeInfo1.roleState = "RoleReady";
        bool isReady = true;
        ServerRequestHandler::GetInstance()->UpdateNodeStatus(*client, *nodeStatus, nodeInfo1, true, isReady);
    }
    DT_FUZZ_END()
    stub.reset(ADDR(HttpClient, SendRequest));

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEControllerUpdateNodeStatus\n");
}


// —_—_—_—_—_—_—_—_—_—_—_—_—_controller 测试 ServerRequestHandler::UpdateNodeInfo—_—_—_—_—_—_—_—_—_—_—_
char *g_mResponse04 = nullptr;
std::string mResponseBin04 = GetPath("tests/ms/common/.mindie_ms/response_body_04.bin");

int MakeResponseBin04()
{
    std::string response;
    json res;
    res["service"] = json();
    srand(time(nullptr)); // 初始化随机数种子
    std::vector<std::string> roleStatus = {"RoleReady", "RoleSwitching", "RoleUnknown", "invalid"} ;
    std::vector<std::string> currentRoles = {"prefill", "decode", "invalid"} ;
    res["service"]["roleStatus"] = roleStatus[rand() % 4];
    res["service"]["currentRole"] = currentRoles[rand() % 3];

    res["resource"] = json();
    res["resource"]["availSlotsNum"] = rand() % 10000;
    res["resource"]["availBlockNum"] = rand() % 10000;
    res["resource"]["waitingRequestNum"] = 0;
    res["resource"]["runningRequestNum"] = 0;
    res["resource"]["swappedRequestNum"] = 0;
    res["resource"]["freeNpuBlockNums"] = 0;
    res["resource"]["freeCpuBlockNums"] = 0;
    res["resource"]["totalNpuBlockNums"] = 0;
    res["resource"]["totalCpuBlockNums"] = 0;
    
    if (res["service"]["currentRole"] == "decode") {
        res["peers"] = nlohmann::json::array();
    }
    response = res.dump();

    // 保存response为bin文件
    std::ofstream outputFile(mResponseBin04, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "无法打开输出文件" << std::endl;
        return 1;
    }
    outputFile.write(response.c_str(), response.size());
    outputFile.close();
    std::cout << "JSON对象已保存到 bin文件" << std::endl;
    return 0;
}

int32_t SendRequestStub04(MINDIE::MS::HttpClient selfObj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    responseBody = g_mResponse04;
    code = 200;
    return 0;
}

TEST_F(TestController, TestMindIEControllerUpdateNodeInfo)
{
    ChangeFileMode600(msControllerJson);

    DT_Pits_SetBinFile(const_cast<char *>(mResponseBin04.c_str()));
    // 产生bin文件
    MakeResponseBin04();
    // 打桩
    stub.set(ADDR(HttpClient, SendRequest), SendRequestStub04);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEControllerUpdateNodeInfo\n");

    DT_FUZZ_START1("TestMindIEControllerUpdateNodeInfo")
    {
        CopyDefaultConfig();
        ControllerConfig::GetInstance()->Init();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        int lenout = 0;
        DT_Pits_GetMutatorBuf(&g_mResponse04, &lenout);

        // 调用被测函数解析变异后的文件
        NodeInfo nodeInfo1;
        nodeInfo1.ip = "172.17.0.1";
        nodeInfo1.instanceInfo.staticInfo.id = 16781740;
        nodeInfo1.instanceInfo.staticInfo.groupId = 1;
        nodeInfo1.instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        bool isReady = true;
        ServerRequestHandler::GetInstance()->UpdateNodeInfo(*client, nodeInfo1, true, isReady);
    }
    DT_FUZZ_END()
    stub.reset(ADDR(HttpClient, SendRequest));

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEControllerUpdateNodeInfo\n");
}


// —_—_—_—_—_—_—_—_—_—_—_—_—_controller 测试 NodeScheduler::GetCoordinatorInfo—_—_—_—_—_—_—_—_—_—_—_
char *g_mResponse05 = nullptr;
std::string mResponseBin05 = GetPath("tests/ms/common/.mindie_ms/response_body_05.bin");

int MakeResponseBin05()
{
    std::string response;
    json res;
    res["schedule_info"] = json();
    nlohmann::json elemBody;
    elemBody["id"] = rand() % 1000;
    elemBody["allocated_slots"] = rand() % 1000;
    elemBody["allocated_blocks"] = rand() % 1000;
    res["schedule_info"].emplace_back(elemBody);
    nlohmann::json summaryBody;
    summaryBody["input_len"] = rand() % 1000;
    summaryBody["output_len"] = rand() % 2000;
    res["request_length_info"] = summaryBody;
    response = res.dump();

    // 保存response为bin文件
    std::ofstream outputFile(mResponseBin05, std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "无法打开输出文件" << std::endl;
        return 1;
    }
    outputFile.write(response.c_str(), response.size());
    outputFile.close();
    std::cout << "JSON对象已保存到 bin文件" << std::endl;
    return 0;
}

int32_t SendRequestStub05(MINDIE::MS::HttpClient selfObj, const Request &request, int timeoutSeconds, int retries,
    std::string& responseBody, int32_t &code)
{
    responseBody = g_mResponse05;
    code = 200;
    return 0;
}

TEST_F(TestController, TestMindIEControllerGetCoordinatorInfo)
{
    ChangeFileMode600(msControllerJson);

    DT_Pits_SetBinFile(const_cast<char *>(mResponseBin05.c_str()));
    // 产生bin文件
    MakeResponseBin05();
    // 打桩
    stub.set(ADDR(HttpClient, SendRequest), SendRequestStub05);

    int id = DT_Pits_ParseDataModel(const_cast<char *>(jsonModule.c_str()), "jsona", 1);
    DT_Pits_Enable_AllMutater(1);
    int fuzzCount = 30000000;
    printf("start ---- TestMindIEControllerGetCoordinatorInfo\n");

    DT_FUZZ_START1("TestMindIEControllerGetCoordinatorInfo")
    {
        CopyDefaultConfig();
        ControllerConfig::GetInstance()->Init();

        printf("\r%d", fuzzSeed + fuzzi);
        DT_Pits_DoAction(id);

        int lenout = 0;
        DT_Pits_GetMutatorBuf(&g_mResponse05, &lenout);

        // 调用被测函数解析变异后的文件
        ChangeFileMode600(msControllerJson);
        ChangeFileMode600(mRankTableJson);
        auto nodeScheduler = std::make_unique<NodeScheduler>(nodeStatus, coordinatorStore);
        DeployMode deployMode = DeployMode::PD_SEPARATE;
        nodeScheduler->Init(deployMode);
        nodeScheduler->GetCoordinatorInfo();
    }
    DT_FUZZ_END()
    stub.reset(ADDR(HttpClient, SendRequest));

    // 释放工具占用内存
    DT_Pits_ParseFree();
    printf("end ---- TestMindIEControllerGetCoordinatorInfo\n");
}