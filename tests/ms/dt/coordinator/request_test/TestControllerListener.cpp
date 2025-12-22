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
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <algorithm>
#include "gtest/gtest.h"
#define main __main_coordinator__
#include "coordinator/main.cpp"
#include "ControllerListener.h"
#include "HttpClient.h"
#include "Configure.h"
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "ThreadSafeVector.h"
#include "SharedMemoryUtils.h"
#include "IPCConfig.h"
#include "AlarmRequestHandler.h"

using namespace MINDIE::MS;

std::vector<std::unique_ptr<std::thread>> g_testControllerListenerthreadsList{};
ThreadSafeVector<std::string> g_predictIPList;
ThreadSafeVector<std::string> g_predictPortList;
ThreadSafeVector<std::string> g_managePortList;
ThreadSafeVector<std::string> g_interCommonPortLists;
std::string g_predictIP = "127.0.0.1";
std::string g_predictPort = "2020";
std::string g_manageIP = "127.0.0.1";
std::string g_managePort = "2021";
std::string g_externalPort = "2022";
std::string g_statusPort = "2023";

class TestControllerListener : public ::testing::Test {
public:
    TestControllerListener()
    {
        logDirPath = GetMSCoordinatorLogPath();
        runLogPath = GetMSCoordinatorRunLogPath();
        operationLogPath = GetMSCoordinatorOperationLogPath();

        RemoveDirectoryRecursively(logDirPath);
        CreateDirectory(logDirPath);
    }

    static void SetUpTestCase()
    {
        CopyFile(GetMSCoordinatorConfigJsonPath(), GetMSCoordinatorTestJsonPath());
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();

        std::cout << "coordinatorJson:" << coordinatorJson << std::endl;
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        setenv("HSECEASY_PATH", GetHSECEASYPATH().c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;

        g_predictPort = std::to_string(GetUnBindPort());
        g_managePort = std::to_string(GetUnBindPort());
        g_externalPort = std::to_string(GetUnBindPort());
        g_statusPort = std::to_string(GetUnBindPort());

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"http_config", "predict_ip"}, g_predictIP);
        manager.SetList({"http_config", "predict_port"}, g_predictPort);
        manager.SetList({"http_config", "manage_ip"}, g_manageIP);
        manager.SetList({"http_config", "manage_port"}, g_managePort);
        manager.SetList({"http_config", "external_port"}, g_externalPort);
        manager.SetList({"http_config", "status_port"}, g_statusPort);

        manager.SetList({"log_info", "log_level"}, "DEBUG");
        manager.SetList({"log_info", "to_file"}, true);
        manager.SetList({"log_info", "to_stdout"}, true);
        manager.SetList({"log_info", "run_log_path"}, "./runlog.txt");
        manager.SetList({"log_info", "operation_log_path"}, "./operlog.txt");

        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Save();

        {
            std::ifstream infile1("./runlog.txt");
            if (infile1.good()) {
                remove("./runlog.txt");
            }
            std::ifstream infile2("./operlog.txt");
            if (infile2.good()) {
                remove("./operlog.txt");
            }
        }

        for (size_t i = 0; i < 4; ++i) {
            g_predictIPList.emplace_back("127.0.0.1");
            auto pPort = std::to_string(GetUnBindPort());
            auto mPort = std::to_string(GetUnBindPort());
            auto iPort = std::to_string(GetUnBindPort());
            auto predictIP = g_predictIPList[i];
            auto manageIP = g_predictIPList[i];
            g_predictPortList.emplace_back(pPort);
            g_managePortList.emplace_back(mPort);
            g_interCommonPortLists.emplace_back(iPort);
            std::cout << "ip = " << g_predictIPList[i] << std::endl;
            std::cout << "pPort = " << pPort << std::endl;
            std::cout << "mPort = " << mPort << std::endl;

            auto thread = std::make_unique<std::thread>(CreateMindIEServer,
            std::move(predictIP), std::move(pPort),
            std::move(manageIP), std::move(mPort),
            4, i, ""); // 默认4线程启动服务

            g_testControllerListenerthreadsList.emplace_back(std::move(thread));
        }

        char *argsCoordinator[1] = {"ms_coordinator"};
        auto threadObj = std::make_unique<std::thread>(__main_coordinator__, 1, argsCoordinator);
        g_testControllerListenerthreadsList.emplace_back(std::move(threadObj));
        std::string response;
        auto pdInstance = SetPDRole(g_predictIPList, g_predictPortList, g_managePortList, g_interCommonPortLists,
            2, 2, g_manageIP, g_managePort);
        WaitCoordinatorReady(g_manageIP, g_statusPort);
    }

    void SetUp() override
    {
        initInfo = SetPDRoleByList(g_predictIPList, g_predictPortList, g_managePortList,
            g_interCommonPortLists, 2, 2).dump(); // 2和2分别是P和D的比例
        logDirPath = GetMSCoordinatorLogPath();
        runLogPath = GetMSCoordinatorRunLogPath();
        operationLogPath = GetMSCoordinatorOperationLogPath();

        RemoveDirectoryRecursively(logDirPath);
        CreateDirectory(logDirPath);
    }

    void TearDown() override
    {}

    static void TearDownTestCase()
    {
        for (size_t i = 0; i < g_testControllerListenerthreadsList.size(); ++i) {
            EXPECT_TRUE(g_testControllerListenerthreadsList[i]->joinable());
            g_testControllerListenerthreadsList[i]->detach();
        }
    }

    std::string initInfo;
    std::string logDirPath;
    std::string runLogPath;
    std::string operationLogPath;
    std::string testCaseName;
    std::string ModifyJsonForTest(std::string Info, std::function<void(json &)> modifyFunc)
    {
        json j = json::parse(Info);
        modifyFunc(j);
        return j.dump();
    }

    bool SearchLogFileForTarget(const std::string &logFileName, const std::string &target)
    {
        std::ifstream logFile(logFileName);
        if (!logFile.is_open()) {
            std::cerr << "Failed to open log file: " << logFileName << std::endl;
            return false;
        }

        std::string line;
        bool found = false;
        while (std::getline(logFile, line)) {
            if (line.find(target) != std::string::npos) {
                found = true;
                break;
            }
        }
        logFile.close();
        return found;
    }
};

// 根据ID删除指定的节点
std::string DeleteServer(std::string serverLists, uint8_t serverID)
{
    // 解析 JSON 字符串
    json j = json::parse(serverLists);

    // 查找并删除 id 为 1 的服务器对象
    bool found = false;
    for (auto it = j["instances"].begin(); it != j["instances"].end();) {
        if (it->at("id") == serverID) {
            it = j["instances"].erase(it);
            found = true;
            // 删除ids
            j["ids"].erase(std::remove(j["ids"].begin(), j["ids"].end(), serverID), j["ids"].end());
        } else {
            ++it;
        }
    }
    return j.dump(4);
}

// ControllerListener::InstancesRefreshHandler
int32_t SendInstanceRefreshInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    // 发送更新后的集群状态信息
    auto ret = SendInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/instances/refresh", instancesInfo, response);
    return ret;
}
 
/*
测试描述: 控制器监听器实例刷新处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的集群状态信息，预期结果1。
    3. 删除一个Partition (P) 后，发送包含1P2D的集群状态信息，预期结果1。
    4. 删除两个Partition (P) 后，发送包含0P2D的集群状态信息，预期结果1。
    5. 删除一个Device (D) 后，发送包含2P1D的集群状态信息，coordinator不可用，预期结果1。
    6. 删除两个Device (D) 后，发送包含2P0D的集群状态信息，coordinator不可用，预期结果1。
预期结果:
    1. 发送集群状态信息成功，返回值为0。
*/
TEST_F(TestControllerListener, InstancesRefreshHandlerTest)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;
    // 发送集群状态信息2P2D
    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 删除一个P后，发送集群状态信息, 变成1P2D
    auto newInfo = DeleteServer(initInfo, 0);
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);

    // 删除两个P后，发送集群状态信息, 变成0P2D
    newInfo = DeleteServer(newInfo, 1);
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 503);

    // 删除一个D后，发送集群状态信息, 变成2P1D, coordinator不可用
    newInfo = DeleteServer(initInfo, 2);
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 删除两个D后，发送集群状态信息, 变成2P0D, coordinator不可用
    newInfo = DeleteServer(newInfo, 3);
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 503);

    // 发送集群状态信息2P2D
    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);
}
/*
测试描述: refresh 请求非法 ids字段不存在
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，包含“ids”字段的信息，预期结果1
    3. 删除“ids”字段
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出 "key 'ids' not found"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC01)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;

    // 发送初始状态
    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    // 检查健康状态
    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 删除“ids”字段
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) { j.erase("ids"); });

    // 发送修改后的信息
    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "key 'ids' not found";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法 ids字段非数组
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，预期结果1
    3. 修改“ids”字段为单个整数
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出"type must be array, but is number"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC02)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 修改ids字段为单个整数
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) { j["ids"] = j["ids"][0]; });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "type must be array, but is number";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法 instance字段不存在
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，包含“instances”字段的信息，预期结果1
    3. 删除“instances”字段
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出"key 'instances' not found"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC03)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 删除instances字段
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) { j.erase("instances"); });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "key 'instances' not found";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法 instances字段非数组
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，包含“instances”字段的信息，预期结果1
    3. 修改“instances”为单个对象
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    1. 发送集群状态信息成功，日志输出"key 'id' not found"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC04)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 修改instances为单个对象
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) { j["instances"][0].erase("id"); });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "key 'id' not found";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法，id冗余，5ids，4instances
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，预期结果1
    3. 增加一个冗余的id到“ids”字段
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 日志显示实例更新，发送集群状态信息成功，返回值为200
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC05)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // ids增加一个id
    std::string modifiedInfo =
        ModifyJsonForTest(initInfo, [](json &j) { j["ids"].insert(j["ids"].begin(), 123456789); });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);
}

json CreateNewInstance()
{
    return json{{"dynamic_info", {{"avail_block_num", 1024}, {"avail_slots_num", 200}}},
        {"id", 5},
        {"ip", "127.0.0.1"},
        {"model_name", "llama_65b"},
        {"port", "45269"},
        {"static_info",
            {{"block_size", 128},
                {"group_id", 0},
                {"label", 2},
                {"p_percentage", 0},
                {"max_output_len", 512},
                {"max_seq_len", 2048},
                {"role", 80},
                {"total_block_num", 1024},
                {"total_slots_num", 200}}}};
}

/*
测试描述: refresh 请求非法，id冗余，4ids，5instances
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，包含“instances”字段的信息，预期结果1
    3. 增加一个冗余的instance到“instances”字段
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 日志显示实例更新，发送集群状态信息成功，返回值为200
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC06)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // instances增加一个instance
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"].insert(j["instances"].begin(), CreateNewInstance());
    });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);
}

/*
测试描述: refresh 请求非法 id缺失，3ids，4instances
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，预期结果1
    3. 删除一个id
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出"Remove D instance IP: 127.0.0.1:"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC07)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // ids删除一个id
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        auto &ids = j["ids"];
        ids.erase(std::remove(ids.begin(), ids.end(), 3), ids.end());
    });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "Remove D instance IP: 127.0.0.1:" + g_predictPortList[3];
    bool found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法 id缺失，4ids，3instances
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，预期结果1
    3. 删除一个instance
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出"Remove D instance IP: 127.0.0.1:"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC08)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 删除instances里id为3的实例
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {j["instances"].erase(j["instances"].end()); });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "Remove D instance IP: 127.0.0.1:" + g_predictPortList[3];
    bool found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法 id对应错误
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，预期结果1
    3. 删除ids字段中id
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出"Remove D instance IP: 127.0.0.1:"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC09)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) { j["ids"] = {-1, 1, 2, 3}; });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "Remove P instance IP: 127.0.0.1:" + g_predictPortList[0];
    bool found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法  id对应错误
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，预期结果1
    3. 删除instances字段中id
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 日志显示实例更新，发送集群状态信息成功，返回值为200
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC10)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) { j["instances"][3]["id"] = -1; });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);
}

/*
测试描述: refresh 请求非法 dynamic_info字段不存在
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，包含“dynamic_info”字段的信息，预期结果1
    3. 删除“dynamic_info”字段
    4. 发送修改后的信息，预期结果1
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出"key 'dynamic_info' not found"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC11)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 删除dynamic_info字段
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        for (auto &instance : j["instances"]) {
            instance.erase("dynamic_info");
        }
    });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "key 'dynamic_info' not found";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法 static_info字段不存在
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，包含“static_info”字段的信息，预期结果1
    3. 删除“static_info”字段
    4. 发送修改后的信息，预期结果1
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出"key 'static_info' not found"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC12)
{
    TlsItems tlsItems;
    std::string response;

    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);

    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 删除static_info字段
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        for (auto &instance : j["instances"]) {
            instance.erase("static_info");
        }
    });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "key 'static_info' not found";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法 ip字段不合法
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送初始状态，包含“ip”字段的信息，预期结果1
    3. 将“ip”字段改为"0.0.0.0"
    4. 发送修改后的信息，预期结果1
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，日志输出"invalid IP"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC13)
{
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        uint64_t newId = 100; // 新的实例id为100
        j["ids"].emplace_back(newId);
        json newIns;
        newIns["id"] = newId;
        newIns["ip"] = "0.0.0.0";
        newIns["port"] = "1025";
        newIns["metric_port"] = "1026";
        newIns["inter_comm_port"] = "20086";
        newIns["model_name"] = "";
        json staticInfo;
        staticInfo["group_id"] = 0;
        staticInfo["max_seq_len"] = 2; // 上限2
        staticInfo["max_output_len"] = 1;
        staticInfo["total_slots_num"] = 1;
        staticInfo["total_block_num"] = 1;
        staticInfo["p_percentage"] = 0;
        staticInfo["block_size"] = 1;
        staticInfo["label"] = 2; // 2表示prefill
        staticInfo["role"] = 80; // 80表示prefill
        staticInfo["virtual_id"] = newId;
        newIns["static_info"] = staticInfo;
        json dynamicInfo;
        dynamicInfo["avail_slots_num"] = 0;
        dynamicInfo["avail_block_num"] = 0;
        newIns["dynamic_info"] = dynamicInfo;
        j["instances"].emplace_back(newIns);
    });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);

    std::string target = "invalid IP";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: refresh 请求非法 instances字段不合法
测试步骤:
    1. 将“instances”字段改为字符串数组
    2. 发送修改后的信息，预期结果1
预期结果:
    1. 发送集群状态信息成功，日志输出"cannot use at() with string"
*/
TEST_F(TestControllerListener, RefreshRequestErrorTC14)
{
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"] = {"123", "456", "789"};
    });

    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);

    std::string target = "cannot use at() with string";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: PD分离场景，分别同步信息中的P实例和D实例，在coordinator不存在，扩容场景
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含1P1D的集群状态信息，预期结果1。
    3. 新增一个P后，发送包含2P1D的集群状态信息，预期结果2
    4. 扩容一个D实例，发送包含2P2D的集群状态信息，预期结果2
    5. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0
    2. 发送集群状态信息成功，返回值为0，日志输出 Add instance p/d
*/
TEST_F(TestControllerListener, InstancesRefreshHandlerExpansion)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息, 上线1P1D，0,2实例
    auto newInfo = DeleteServer(initInfo, 1);
    newInfo = DeleteServer(newInfo, 3);
    auto ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, newInfo);  // 更新集群状态信息，并发送到管理节点
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(
        g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);  // 发送健康检查请求，确认服务准备就绪
    EXPECT_EQ(ret, 200);

    // 创建流式和非流式请求体
    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);
    auto singlestreamBody = CreateOpenAIRequest({"M"}, true);
    auto singlenonstreamBody = CreateOpenAIRequest({"M"}, false);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    auto code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    // 扩容P实例, 上线2P1D，0, 1, 2实例，增加实例1
    newInfo = DeleteServer(initInfo, 3);
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    std::string target = "Add instance P IP 127.0.0.1:" + g_predictPortList[1];
    bool found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);

    // 扩容D实例, 上线2P2D，0,1,2,3实例，增加实例3
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    target = "Add instance D IP 127.0.0.1:" + g_predictPortList[3]; // 3: interval port
    found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: PD分离场景，分别同步信息中的P、D实例，在coordinator存在
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含1P1D的集群状态信息，预期结果1。
    3. 修改P实例的端口信息为无效端口，并发送更新后的集群状态信息，预期结果2
    4. 修改D实例的端口信息为无效端口，并发送更新后的集群状态信息，预期结果2
    5. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0
    2. 日志显示instance更新成功，发送集群状态信息成功，返回值为0
*/
TEST_F(TestControllerListener, InstancesRefreshHandlerUpdate)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息, 上线2P2D
    auto ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(
        g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);  // 发送健康检查请求，确认服务准备就绪
    EXPECT_EQ(ret, 200);

    // 创建流式和非流式请求体
    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);
    auto singlestreamBody = CreateOpenAIRequest({"M"}, true);
    auto singlenonstreamBody = CreateOpenAIRequest({"M"}, false);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    auto code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions",
        nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    // instances修改P实例0端口信息为无效端口
    std::string modifiedInfo = ModifyJsonForTest(initInfo,
        [](json &j) {
            j["instances"][0]["port"] = "invalid_port";
            j["instances"][1]["port"] = "invalid_port";
        });
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    sleep(10);

    // 发送推理请求
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // instances修改D实例2端口信息为无效端口
    modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"][2]["port"] = "invalid_port";
        j["instances"][3]["port"] = "invalid_port";
    });
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 发送推理请求
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
}

/*
测试描述: 缩容场景，分别同步信息中的P实例、D实例不存在，在coordinator中存在
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含1P1D的集群状态信息，预期结果1。
    3. 新增一个P后，发送包含2P1D的集群状态信息，预期结果2
    4. 新增一个D后，发送包含2P1D的集群状态信息，预期结果2
    5. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0。
    2. 发送集群状态信息成功，返回值为0，日志输出Remove p/d instance
*/
TEST_F(TestControllerListener, InstancesRefreshHandlerContraction)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息, 上线2P2D
    auto ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(
        g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);  // 发送健康检查请求，确认服务准备就绪
    EXPECT_EQ(ret, 200);

    // 创建流式和非流式请求体
    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);
    auto singlestreamBody = CreateOpenAIRequest({"M"}, true);
    auto singlenonstreamBody = CreateOpenAIRequest({"M"}, false);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    auto code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions",
        nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    // 缩容P实例，1P2D，1，2，3实例
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"].erase(j["instances"].begin());
        j["ids"].erase(j["ids"].begin());
    });
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");
    EXPECT_EQ(code, 0);

    std::string target = "Remove P instance IP: 127.0.0.1:" + g_predictPortList[0];
    bool found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);

    // 缩容D实例，2P1D，0，1，2实例
    modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"].erase(j["instances"].end());
        j["ids"].erase(j["ids"].end());
    });
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);
    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");
    EXPECT_EQ(code, 0);

    target = "Remove D instance IP: 127.0.0.1:" + g_predictPortList[3]; // 3: internal port
    found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: PD分离场景，同步信息中的D实例，在coordinator不存在，D实例没有启动
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含1P1D的集群状态信息，预期结果1。
    3. 新增一个D后，发送包含1P2D的集群状态信息，预期结果2
    4. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0。
    2. 发送集群状态信息成功，返回值为0，日志输出"Add: Link with d nodes failed"
*/
TEST_F(TestControllerListener, InstancesRefreshHandlerInvalid)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息, 上线1P1D，0,2实例
    auto newInfo = DeleteServer(initInfo, 1);
    newInfo = DeleteServer(newInfo, 3);
    auto ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, newInfo);  // 更新集群状态信息，并发送到管理节点
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(
        g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);  // 发送健康检查请求，确认服务准备就绪
    EXPECT_EQ(ret, 200);

    // 创建流式和非流式请求体
    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);
    auto singlestreamBody = CreateOpenAIRequest({"M"}, true);
    auto singlenonstreamBody = CreateOpenAIRequest({"M"}, false);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    auto code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    // 扩容D实例, 上线1P2D，0, 2, 3实例
    newInfo = DeleteServer(initInfo, 1);
    // 修改实例3信息，实例3无效
    std::string modifiedInfo = ModifyJsonForTest(newInfo, [](json &j) { j["instances"][2]["ip"] = "invalid_ip"; });
    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);

    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");
    EXPECT_EQ(code, 0);

    std::string target = "invalid IP";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: PD分离场景，同步信息为单机部署角色的实例
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的集群状态信息，预期结果1。
    3. 修改P/D实例0为单机角色，role为85，发送集群信息，预期结果2
    4. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0
    2. 发送集群状态信息成功，返回值为0，日志输出Remove p/d instance
*/
TEST_F(TestControllerListener, InstancesRefreshHandlerSingle)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息, 上线2P2D
    auto ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(
        g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);  // 发送健康检查请求，确认服务准备就绪
    EXPECT_EQ(ret, 200);

    // 创建流式和非流式请求体
    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);
    auto singlestreamBody = CreateOpenAIRequest({"M"}, true);
    auto singlenonstreamBody = CreateOpenAIRequest({"M"}, false);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    auto code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    // 修改P实例0为单机角色，role为85
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"][0]["static_info"]["label"] = 0;
        j["instances"][0]["static_info"]["role"] = 85;
    });

    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");

    std::string target = "Remove P instance IP: 127.0.0.1:" + g_predictPortList[0];
    bool found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);

    // 修改D实例3为单机角色，role为85
    modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"][3]["static_info"]["label"] = 0;
        j["instances"][3]["static_info"]["role"] = 85;
    });

    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);

    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");

    target = "Remove D instance IP: 127.0.0.1:" + g_predictPortList[3]; // 3: internal port
    found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: PD分离场景，同步信息为非P/D/U角色
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的集群状态信息，预期结果1。
    3. 修改P/D实例为非P/D/U角色，发送集群信息，预期结果2
    4. 发送推理请求，请求成功处理
预期结果:
    1. 发送集群状态信息成功，返回值为0
    2. 发送集群状态信息成功，返回值为0，日志输出Remove p/d instance
*/
TEST_F(TestControllerListener, InstancesRefreshHandlerOthers)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息, 上线2P2D
    auto ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(
        g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);  // 发送健康检查请求，确认服务准备就绪
    EXPECT_EQ(ret, 200);

    // 创建流式和非流式请求体
    auto nonStreamBody = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto streamBody = CreateOpenAIRequest({"My name is Olivier and I", "My name is Olivier and I"}, true);
    auto singlestreamBody = CreateOpenAIRequest({"M"}, true);
    auto singlenonstreamBody = CreateOpenAIRequest({"M"}, false);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    // 发送推理请求
    auto code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    // 修改P实例0为非P/D/U角色
    std::string modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"][0]["static_info"]["label"] = -1;
        j["instances"][0]["static_info"]["role"] = 0;
    });

    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);

    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");

    std::string target = "Remove P instance IP: 127.0.0.1:" + g_predictPortList[0];
    bool found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);

    // 修改D实例3为非P/D/U
    modifiedInfo = ModifyJsonForTest(initInfo, [](json &j) {
        j["instances"][3]["static_info"]["label"] = -1;
        j["instances"][3]["static_info"]["role"] = 0;
    });

    ret = SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);

    EXPECT_EQ(ret, 0);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    WaitCoordinatorReady(g_manageIP, g_statusPort);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", nonStreamBody, response);
    EXPECT_EQ(response, "My name is Olivier and IOK");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", streamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response),
        "My name is Olivier and IMy name is Olivier and I");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code = SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlestreamBody, response);
    EXPECT_EQ(ProcessStreamRespond(response), "M");  // mock实现为将所有的msg拼接
    EXPECT_EQ(code, 0);

    code =
        SendInferRequest(g_predictIP, g_predictPort, tlsItems, "/v1/chat/completions", singlenonstreamBody, response);
    EXPECT_EQ(response, "M");
    EXPECT_EQ(code, 0);

    target = "Remove D instance IP: 127.0.0.1:" + g_predictPortList[3]; // 3: interval port
    found = SearchLogFileForTarget("./operlog.txt", target);
    EXPECT_TRUE(found);
}

// ControllerListener::CoordinatorInfoHandler
int32_t SendCoordinatorInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret = GetInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/coordinator_info", response);
    return ret;
}

/*
测试描述: 控制器监听器协调器信息处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的协调器信息，预期结果1。
    3. 删除一个Partition (P) 后，发送包含1P2D的协调器信息，预期结果1。
    4. 删除两个Partition (P) 后，发送包含0P2D的协调器信息，预期结果1。
    5. 删除一个Device (D) 后，发送包含2P1D的协调器信息，coordinator不可用，预期结果1。
    6. 删除两个Device (D) 后，发送包含2P0D的协调器信息，coordinator不可用，预期结果1。
预期结果:
    1. 发送协调器信息成功，HTTP响应状态码为200。
*/
TEST_F(TestControllerListener, CoordinatorInfoHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息2P2D
    SendCoordinatorInfo(g_manageIP, g_managePort, initInfo);

    // 删除一个P后，发送集群状态信息, 变成1P2D
    auto newInfo = DeleteServer(initInfo, 0);
    auto ret = SendCoordinatorInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 200);

    // 删除两个P后，发送集群状态信息, 变成0P2D
    newInfo = DeleteServer(newInfo, 1);
    ret = SendCoordinatorInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 200);

    // 删除一个D后，发送集群状态信息, 变成2P1D, coordinator不可用
    newInfo = DeleteServer(initInfo, 3);
    ret = SendCoordinatorInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 200);

    // 删除两个D后，发送集群状态信息, 变成2P0D, coordinator不可用
    newInfo = DeleteServer(newInfo, 4);
    ret = SendCoordinatorInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 200);
}

// ControllerListener::InstancesOfflineHandler
int32_t SendInstancesOfflineInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret = SendInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/instances/offline", instancesInfo, response);
    return ret;
}

/*
测试描述: 控制器监听器实例离线处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的实例离线信息，预期结果1。
    3. 删除一个Partition (P) 后，发送包含1P2D的实例离线信息，预期结果1。
    4. 删除两个Partition (P) 后，发送包含0P2D的实例离线信息，预期结果1。
    5. 删除一个Device (D) 后，发送包含2P1D的实例离线信息，coordinator不可用，预期结果1。
    6. 删除两个Device (D) 后，发送包含2P0D的实例离线信息，coordinator不可用，预期结果1。
预期结果:
    1. 发送实例离线信息成功，返回值为0。
*/
TEST_F(TestControllerListener, InstancesOfflineHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息2P2D
    SendInstancesOfflineInfo(g_manageIP, g_managePort, initInfo);

    // 删除一个P后，发送集群状态信息, 变成1P2D
    auto newInfo = DeleteServer(initInfo, 0);
    auto ret = SendInstancesOfflineInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);

    // 删除两个P后，发送集群状态信息, 变成0P2D
    newInfo = DeleteServer(newInfo, 1);
    ret = SendInstancesOfflineInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);

    // 删除一个D后，发送集群状态信息, 变成2P1D, coordinator不可用
    newInfo = DeleteServer(initInfo, 3);
    ret = SendInstancesOfflineInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);

    // 删除两个D后，发送集群状态信息, 变成2P0D, coordinator不可用
    newInfo = DeleteServer(newInfo, 4);
    ret = SendInstancesOfflineInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);
}

// ControllerListener::InstancesOnlineHandler
int32_t SendInstancesOnlineInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret = SendInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/instances/online", instancesInfo, response);
    return ret;
}

/*
测试描述: 控制器监听器实例在线处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的实例在线信息，预期结果1。
    3. 删除一个Partition (P) 后，发送包含1P2D的实例在线信息，预期结果1。
    4. 删除两个Partition (P) 后，发送包含0P2D的实例在线信息，预期结果1。
    5. 删除一个Device (D) 后，发送包含2P1D的实例在线信息，coordinator不可用，预期结果1。
    6. 删除两个Device (D) 后，发送包含2P0D的实例在线信息，coordinator不可用，预期结果1。
预期结果:
    1. 发送实例在线信息成功，返回值为0。
*/
TEST_F(TestControllerListener, InstancesOnlineHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息2P2D
    SendInstancesOnlineInfo(g_manageIP, g_managePort, initInfo);

    // 删除一个P后，发送集群状态信息, 变成1P2D
    auto newInfo = DeleteServer(initInfo, 0);
    auto ret = SendInstancesOnlineInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);

    // 删除两个P后，发送集群状态信息, 变成0P2D
    newInfo = DeleteServer(newInfo, 1);
    ret = SendInstancesOnlineInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);

    // 删除一个D后，发送集群状态信息, 变成2P1D, coordinator不可用
    newInfo = DeleteServer(initInfo, 3);
    ret = SendInstancesOnlineInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);

    // 删除两个D后，发送集群状态信息, 变成2P0D, coordinator不可用
    newInfo = DeleteServer(newInfo, 4);
    ret = SendInstancesOnlineInfo(g_manageIP, g_managePort, newInfo);
    EXPECT_EQ(ret, 0);
}

int32_t SendInstancesTasksInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret = GetInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/instances/tasks*", response);
    return ret;
}

/*
测试描述: 控制器监听器实例任务处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的实例任务信息，预期结果1。
预期结果:
    1. 发送实例任务信息成功，HTTP响应状态码为200。
*/
TEST_F(TestControllerListener, InstancesTasksHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息2P2D
    auto ret = SendInstancesTasksInfo(g_manageIP, g_managePort, initInfo);
    EXPECT_EQ(ret, 200);
}

int32_t SendStartUpProbeInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret = GetInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/startup", response);
    return ret;
}

/*
测试描述: 控制器监听器启动探测处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的启动探测信息，预期结果1。
预期结果:
    1. 发送启动探测信息成功，HTTP响应状态码为200。
*/
TEST_F(TestControllerListener, StartUpProbeHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息2P2D
    auto ret = SendStartUpProbeInfo(g_manageIP, g_statusPort, initInfo);
    EXPECT_EQ(ret, 200);
}

int32_t HealthProbeInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret = GetInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/health", response);
    return ret;
}

/*
测试描述: 控制器监听器健康探测处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的健康探测信息，预期结果1。
预期结果:
    1. 发送健康探测信息成功，HTTP响应状态码为200。
*/
TEST_F(TestControllerListener, HealthProbeHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息2P2D
    auto ret = HealthProbeInfo(g_manageIP, g_statusPort, initInfo);
    EXPECT_EQ(ret, 200);
}

int32_t SendReadinessProbeInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    return ret;
}

/*
测试描述: 控制器监听器就绪探测处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的就绪探测信息，预期结果1。
预期结果:
    1. 发送就绪探测信息成功，HTTP响应状态码为200。
*/
TEST_F(TestControllerListener, ReadinessProbeHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息2P2D
    auto ret = SendReadinessProbeInfo(g_manageIP, g_managePort, initInfo);
    EXPECT_EQ(ret, 200);
}

int32_t SendTritonModelsReadyInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret = GetInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/readiness", response);
    return ret;
}

/*
测试描述: 控制器监听器模型就绪处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的模型就绪信息，预期结果1。
预期结果:
    1. 发送模型就绪信息成功，HTTP响应状态码为200。
*/
TEST_F(TestControllerListener, TritonModelsReadyHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    // 发送集群状态信息2P2D
    auto ret = SendTritonModelsReadyInfo(g_manageIP, g_statusPort, initInfo);
    EXPECT_EQ(ret, 200);
}

int32_t SendInstancesQueryTasksInfo(std::string &g_manageIP, std::string &g_managePort, std::string instancesInfo)
{
    TlsItems tlsItems;
    std::string response;
    auto ret =
        SendInferRequest(g_manageIP, g_managePort, tlsItems, "/v1/instances/query_tasks", instancesInfo, response);
    return ret;
}

/*
测试描述: 控制器监听器实例查询任务处理器测试
测试步骤:
    1. 初始化 TLS 项和响应字符串。
    2. 发送包含2P2D的查询任务信息，预期结果1。
    3. 测试Pid不存在的情况，预期结果2。
    4. 测试Did不存在的情况，预期结果3。
    5. 测试Pid和Did都不存在的情况，预期结果4。
预期结果:
    1. 发送查询任务信息成功，返回值为0。
    2. Pid不存在时，返回值为0。
    3. Did不存在时，返回值为0。
    4. Pid和Did都不存在时，返回值为0。
*/
TEST_F(TestControllerListener, InstancesQueryTasksHandlerTest)
{
    TlsItems tlsItems;
    std::string response;

    {
        std::string taskInfo = R"({
                    "p_id": 1,
                    "d_id": 2
                })";

        // 发送查询任务状态信息
        auto ret = SendInstancesQueryTasksInfo(g_manageIP, g_managePort, initInfo);
        EXPECT_EQ(ret, 0);
    }
    // Pid不存在
    {
        std::string taskInfo = R"({
                    "d_id": 2
                })";

        // 发送查询任务状态信息
        auto ret = SendInstancesQueryTasksInfo(g_manageIP, g_managePort, initInfo);
        EXPECT_EQ(ret, 0);
    }
    // Did不存在
    {
        std::string taskInfo = R"({
                    "p_id": 1
                })";

        // 发送查询任务状态信息
        auto ret = SendInstancesQueryTasksInfo(g_manageIP, g_managePort, initInfo);
        EXPECT_EQ(ret, 0);
    }
    // 都不存在
    {
        std::string taskInfo = R"({
                    "xxx": 1
                })";

        // 发送查询任务状态信息
        auto ret = SendInstancesQueryTasksInfo(g_manageIP, g_managePort, initInfo);
        EXPECT_EQ(ret, 0);
    }
}

/*
测试描述: 向ControllerListener发送refresh请求更新集群信息，请求中inter_comm_port字段缺失
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送合法的初始状态，预期结果1
    3. 删除所有实例的"inter_comm_port"字段
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，但集群不可用，coordinator非健康，日志输出 "key 'inter_comm_port' not found"
*/
TEST_F(TestControllerListener, RefreshWithoutInterCommPort)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;

    // 发送初始状态
    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    // 检查健康状态
    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 删除instances下的inter_comm_port字段
    auto modFunc = [](json &j) {
        for (auto &ins : j["instances"]) {
            // 修改身份触发实例端口信息刷新
            auto newRole = ins["static_info"]["role"] == int('P') ? int('D') : int('P');
            ins["static_info"]["role"] = newRole;
            ins.erase("inter_comm_port");
        }
    };
    std::string modifiedInfo = ModifyJsonForTest(initInfo, modFunc);
    // 发送修改后的信息
    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_NE(ret, 200); // 实例信息解析失败，集群不可用

    std::string target = "key 'inter_comm_port' not found";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_TRUE(found);
}

/*
测试描述: 向ControllerListener发送refresh请求更新集群信息，请求中inter_comm_port字段非法
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送合法的初始状态，预期结果1
    3. 修改"instance"下的"inter_comm_port"字段，将端口设置为大于65535上限的值
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，inter_comm_port字段使用默认空值，
        集群任然可用，日志输出 inter_comm_port "will not be used for routing inference requests"
*/
TEST_F(TestControllerListener, RefreshWithInvalidInterCommPort)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;

    // 发送初始状态
    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    // 检查健康状态
    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 修改instances下的inter_comm_port字段
    auto modFunc = [](json &j) {
        for (auto &ins : j["instances"]) {
            // 修改身份触发实例端口信息刷新
            auto newRole = ins["static_info"]["role"] == int('P') ? int('D') : int('P');
            ins["static_info"]["role"] = newRole;
            int32_t curPort = static_cast<int32_t>(std::stoi(ins["inter_comm_port"].get<std::string>()));
            curPort += 65535;
            ins["inter_comm_port"] = std::to_string(curPort);
        }
    };
    std::string modifiedInfo = ModifyJsonForTest(initInfo, modFunc);
    // 发送修改后的信息
    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "The provided inter-communication port is invalid";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_FALSE(found);
}

/*
测试描述: 向ControllerListener发送refresh请求更新集群信息，请求中inter_comm_port字段为空
测试步骤:
    1. 初始化 TLS 项和响应字符串
    2. 发送合法的初始状态，预期结果1
    3. 修改"instance"下的"inter_comm_port"字段为空
    4. 发送修改后的信息，预期结果2
预期结果:
    1. 发送集群状态信息成功，返回值为200
    2. 发送集群状态信息成功，inter_comm_port字段使用默认空值，
        集群任然可用，日志输出 inter_comm_port "will not be used for routing inference requests"
*/
TEST_F(TestControllerListener, RefreshWithEmptyInterCommPort)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;

    // 发送初始状态
    SendInstanceRefreshInfo(g_manageIP, g_managePort, initInfo);
    // 检查健康状态
    auto ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    // 修改instances下的inter_comm_port字段
    auto modFunc = [](json &j) {
        for (auto &ins : j["instances"]) {
            // 修改身份触发实例端口信息刷新
            auto newRole = ins["static_info"]["role"] == int('P') ? int('D') : int('P');
            ins["static_info"]["role"] = newRole;
            ins["inter_comm_port"] = "";
        }
    };
    std::string modifiedInfo = ModifyJsonForTest(initInfo, modFunc);
    // 发送修改后的信息
    SendInstanceRefreshInfo(g_manageIP, g_managePort, modifiedInfo);
    ret = GetInferRequest(g_manageIP, g_statusPort, tlsItems, "/v2/health/ready", response);
    EXPECT_EQ(ret, 200);

    std::string target = "The provided inter-communication port is invalid";
    bool found = SearchLogFileForTarget("./runlog.txt", target);
    EXPECT_FALSE(found);
}

TEST_F(TestControllerListener, AbnormalStatusHandlerTestWithValidJson)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;
    
    std::string body = R"({"is_master":true,"is_abnormal":false,"is_random_pick":true})";
    int32_t ret = SendInferRequest(g_manageIP, g_managePort, tlsItems, "/backup_info", body, response);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestControllerListener, AbnormalStatusHandlerTestWithInValidJson)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;
    
    std::string body = R"({"is_master":true,"is_abnormal":false,)"; // 非法 JSON
    int32_t ret = SendInferRequest(g_manageIP, g_managePort, tlsItems, "/backup_info", body, response);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestControllerListener, RecvsUpdaterTest)
{
    TlsItems tlsItems;
    tlsItems.tlsEnable = false;
    std::string response;

    int32_t ret = GetInferRequest(g_manageIP, g_managePort, tlsItems, "/recvs_info", response);
    EXPECT_EQ(ret, 200);
}