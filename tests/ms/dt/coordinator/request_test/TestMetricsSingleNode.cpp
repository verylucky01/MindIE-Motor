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
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "stub.h"
#include "Metrics.h"

using namespace MINDIE::MS;

class TestMetricsSingleNode : public testing::Test {
protected:

    void SetUp()
    {
        std::string exePath = GetExecutablePath();
        std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
        std::string metricsSingleNodeData0File = GetAbsolutePath(
            parentPath, "tests/ms/common/.mindie_ms/ms_metricsSingleNodeData_0.bin");
        std::string metricsSingleNodeData1File = GetAbsolutePath(
            parentPath, "tests/ms/common/.mindie_ms/ms_metricsSingleNodeData_1.bin");
        std::string metricsSingleNodeData2File = GetAbsolutePath(
            parentPath, "tests/ms/common/.mindie_ms/ms_metricsSingleNodeData_2.bin");
        std::string metricsSingleNodeData3File = GetAbsolutePath(
            parentPath, "tests/ms/common/.mindie_ms/ms_metricsSingleNodeData_3.bin");
        std::vector<std::string> filePath = {metricsSingleNodeData0File, metricsSingleNodeData1File,
            metricsSingleNodeData2File, metricsSingleNodeData3File};
        singleNodeMetrics.resize(4);  // 4 个指标
        msMetricsBin = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/ms_metrics.bin");

        for (auto i = 0; i < filePath.size(); i++) {
            std::ifstream readFile(filePath[i]);
            std::stringstream buf;
            buf << readFile.rdbuf();
            singleNodeMetrics[i] = buf.str();
            readFile.close();
        }

        std::string aggregate4SingleNodeOutput = GetAbsolutePath(
            parentPath, "tests/ms/common/.mindie_ms/ms_metricsAggregate4SingleNodeOutput.bin");
        std::ifstream readFile(aggregate4SingleNodeOutput);
        std::stringstream buf;
        buf << readFile.rdbuf();
        metricsAggregate4SingleNodeOutput = buf.str();
        readFile.close();

        CopyFile(GetMSCoordinatorConfigJsonPath(), GetMSCoordinatorTestJsonPath());
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;
        setenv("HSECEASY_PATH", GetHSECEASYPATH().c_str(), 1);
        // 设置监控指标环境变量
        setenv("MIES_SERVICE_MONITOR_MODE", "1", 1);

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        manager.SetList({"http_config", "external_port"}, externalPort);
        manager.SetList({"prometheus_metrics_config", "reuse_time"}, 10);  // 设置重用时间
        manager.SetList({"digs_scheduler_config", "deploy_mode"}, "single_node");
        manager.SetList({"digs_scheduler_config", "scheduler_type"}, "default_scheduler");
        manager.SetList({"digs_scheduler_config", "algorithm_type"}, "cache_affinity");
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_mangment_tls_enable"}, false);
        manager.Save();
        for (auto i = 0; i < numMindIEServer; i++) {
            predictIPList.emplace_back("127.0.0.1");
        }
    }

    void TearDown()
    {
    }

    uint8_t numMindIEServer = 4;
    std::string initInfo;
    ThreadSafeVector<std::string> predictIPList;
    ThreadSafeVector<std::string> predictPortList;
    ThreadSafeVector<std::string> managePortList;
    ThreadSafeVector<std::string> metricPortList;
    ThreadSafeVector<std::string> interCommonPortLists;
    std::string manageIP = "127.0.0.1";
    std::string managePort = "1026";
    std::string predictIP = "127.0.0.1";
    std::string predictPort = "1025";
    std::string externalPort = "2029";
    std::vector<std::string> singleNodeMetrics;
    std::string metricsAggregate4SingleNodeOutput;
    std::string msMetricsBin;
};

/*
测试描述: 测试服务监控指标查询，单机单实例场景
测试步骤:
    1. 拉起单机Server，使用prefixcache算法
    2. 向coordinator发送/metrics请求，预期结果1
预期结果:
    1. 请求正常返回
*/
TEST_F(TestMetricsSingleNode, TestSingleNodeMetricsTC01)
{
    std::cout << "--------------------- Test begin ----------------------" << std::endl;
    std::vector<std::thread> threads;
    int32_t numServer = 1;
    for (size_t i = 0; i < numServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto metricPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto predictIP = predictIPList[i];
        auto manageIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        metricPortList.push_back(metricPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << ", metric port = " << metricPort << std::endl;

        threads.push_back(std::thread(CreateSingleServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, std::move(metricPort))); // 默认4线程启动服务
    }
    std::cout << "--------------------- Create single server done ----------------------" << std::endl;

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));
    std::cout << "--------------------- create coodinator success ----------------------" << std::endl;

    nlohmann::json instancesInfo;
    instancesInfo["ids"] = nlohmann::json::array();
    instancesInfo["instances"] = nlohmann::json::array();

    auto peers = nlohmann::json::array();

    for (auto i = 0; i < numServer; i++) {
        instancesInfo["ids"].emplace_back(i);
        nlohmann::json instance;
        instance["id"] = i;
        instance["ip"] = predictIPList[i];
        instance["port"] = predictPortList[i];
        instance["metric_port"] = metricPortList[i];
        instance["inter_comm_port"] = interCommonPortLists[i];
        instance["model_name"] = "llama_65b";
        nlohmann::json static_info;
        nlohmann::json dynamic_info;
        static_info["group_id"] = 0;
        static_info["max_seq_len"] = 2048;
        static_info["max_output_len"] = 512;
        static_info["total_slots_num"] = 200;
        static_info["total_block_num"] = 512 * (i + 1);  // 不同Node分配不同的NPU MEM SIZE
        static_info["block_size"] = 128;
        static_info["label"] = 0;
        static_info["role"] = 85;
        static_info["virtual_id"] = i;
        static_info["p_percentage"] = 0;

        dynamic_info["avail_slots_num"] = 200;
        dynamic_info["avail_block_num"] = 1024;
        instance["static_info"] = static_info;
        instance["dynamic_info"] = dynamic_info;
        instancesInfo["instances"].emplace_back(instance);
    }

    TlsItems tlsItems;
    std::string response;

    auto lambdaFunc = [this, tlsItems, instancesInfo]() -> bool {
        std::string response;
        auto ret = SendInferRequest(this->manageIP, this->managePort, tlsItems, "/v1/instances/refresh",
            instancesInfo.dump(), response);
        std::cout << "------------ SetSingleRole ret ----------------" << ret << std::endl;
        if (ret == 0) {
            std::cout << "------------ SetSingleRole Ready ----------------" << std::endl;
            return true;
        } else {
            return false;
        }
    };

    // 15s内1s间隔检查是否ready
    EXPECT_FALSE(WaitUtilTrue(lambdaFunc, 15, 1000));

    WaitCoordinatorReady(manageIP, managePort);
    std::cout << "--------------------- coodinator ready ----------------------" << std::endl;

    // 发送指标查询请求，默认超时时间60，最大重复请求次数3
    auto ret = GetMetricsRequest(manageIP, externalPort, tlsItems, "/metrics", response, false);
    EXPECT_EQ(ret, 200); // 200 表示 HTTP 请求成功

    std::cout << "----------------printf metrics-------------------\n" << response << std::endl;
    EXPECT_EQ(response, singleNodeMetrics[0]);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}

/*
测试描述: 测试服务监控指标查询，多个单机部署场景
测试步骤:
    1. 拉起单机Server，使用prefixcache算法
    2. 向coordinator发送/metrics请求，预期结果1
预期结果:
    1. 请求正常返回
*/
TEST_F(TestMetricsSingleNode, TestMultiSingleNodeMetricsTC01)
{
    std::cout << "--------------------- Test begin ----------------------" << std::endl;

    std::vector<std::thread> threads;
    for (size_t i = 0; i < numMindIEServer; i++) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto metricPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto predictIP = predictIPList[i];
        auto manageIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        metricPortList.push_back(metricPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << ", predict port = " << pPort <<
                        ", manage port = " << mPort << ", metric port = " << metricPort << std::endl;

        threads.push_back(std::thread(CreateSingleServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, std::move(metricPort))); // 默认4线程启动服务
    }

    std::cout << "--------------------- Create single server done ----------------------" << std::endl;

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    std::cout << "--------------------- create coodinator success ----------------------" << std::endl;

    nlohmann::json instancesInfo;
    instancesInfo["ids"] = nlohmann::json::array();
    instancesInfo["instances"] = nlohmann::json::array();

    auto peers = nlohmann::json::array();

    for (auto i = 0; i < numMindIEServer; i++) {
        instancesInfo["ids"].emplace_back(i);
        nlohmann::json instance;
        instance["id"] = i;
        instance["ip"] = predictIPList[i];
        instance["port"] = predictPortList[i];
        instance["metric_port"] = metricPortList[i];
        instance["inter_comm_port"] = interCommonPortLists[i];
        instance["model_name"] = "llama_65b";
        nlohmann::json static_info;
        nlohmann::json dynamic_info;
        static_info["group_id"] = 0;
        static_info["max_seq_len"] = 2048;
        static_info["max_output_len"] = 512;
        static_info["total_slots_num"] = 200;
        static_info["total_block_num"] = 512 * (i + 1);  // 不同Node分配不同的NPU MEM SIZE
        static_info["block_size"] = 128;
        static_info["label"] = 0;
        static_info["role"] = 85;
        static_info["virtual_id"] = i;
        static_info["p_percentage"] = 0;

        dynamic_info["avail_slots_num"] = 200;
        dynamic_info["avail_block_num"] = 1024;
        instance["static_info"] = static_info;
        instance["dynamic_info"] = dynamic_info;
        instancesInfo["instances"].emplace_back(instance);
    }
    std::cout << "initInfo done" << std::endl;

    TlsItems tlsItems;
    std::string response;

    auto lambdaFunc = [this, tlsItems, instancesInfo]() -> bool {
        std::string response;
        auto ret = SendInferRequest(this->manageIP, this->managePort, tlsItems, "/v1/instances/refresh",
            instancesInfo.dump(), response);
        std::cout << "------------ SetSingleRole ret ----------------" << ret << std::endl;
        if (ret == 0) {
            std::cout << "------------ SetSingleRole Ready ----------------" << std::endl;
            return true;
        } else {
            return false;
        }
    };

    // 15s内1s间隔检查是否ready
    EXPECT_EQ(WaitUtilTrue(lambdaFunc, 15, 1000), false);

    WaitCoordinatorReady(manageIP, managePort);

    std::cout << "--------------------- coodinator ready ----------------------" << std::endl;

    // 发送指标查询请求，默认超时时间60，最大重复请求次数3
    auto ret = GetMetricsRequest(manageIP, externalPort, tlsItems, "/metrics", response, false);
    EXPECT_EQ(ret, 200); // 200 表示 HTTP 请求成功

    std::cout << "----------------printf metrics-------------------\n" << response << std::endl;
    EXPECT_EQ(response, metricsAggregate4SingleNodeOutput);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}


class SortTimeReUseMock : public MindIEServerMock {
public:
    explicit SortTimeReUseMock(const std::string &keyInit) : MindIEServerMock(keyInit) {}
    ~SortTimeReUseMock() = default;
    void MetricsHandler(std::shared_ptr<ServerConnection> connection)
    {
        std::cout << "--------------------- the "<< requestCount <<" times request ----------------------" << std::endl;
        ServerRes res;
        res.body = singleNodeMetrics[requestCount % 4];  // 4 根据请求顺序分配不同的指标数值
        connection->SendRes(res);
        requestCount++;
    }
private:
    int32_t requestCount = 0;  // 统计请求的次数
};

/*
测试描述: 测试服务监控指标查询，短时间请求多次返回相同指标
测试步骤:
    1. 拉起单机Server，使用prefixcache算法
    2. 向coordinator发送第一次/metrics请求，预期结果1
    3. 短时间发送第二次/metrics请求，预期结果1
    4. sleep到超过指标缓存时间，发送第三次/metrics请求，预期结果2
预期结果:
    1. 请求正常返回
    2. 请求正常返回，指标值与预期结果1不同
*/
TEST_F(TestMetricsSingleNode, TestSingleNodeMetricsSortTimeReUseTC01)
{
    std::vector<std::string> predictIPList = {"127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1"};
    std::vector<std::string> predictPortList;
    std::vector<std::string> managePortList;
    std::vector<std::string> metricPortList;
    std::vector<std::string> interCommonPortLists;
    // 构造一个http server
    int32_t numServer = 1;
    std::vector<std::thread> threads;
    for (size_t i = 0; i < numServer; ++i) {
        predictIPList.emplace_back("127.0.0.1");
        auto pPort = std::to_string(GetUnBindPort());
        auto mPort = std::to_string(GetUnBindPort());
        auto metricPort = std::to_string(GetUnBindPort());
        auto iPort = std::to_string(GetUnBindPort());
        auto predictIP = predictIPList[i];
        auto manageIP = predictIPList[i];
        predictPortList.push_back(pPort);
        managePortList.push_back(mPort);
        metricPortList.push_back(metricPort);
        interCommonPortLists.push_back(iPort);
        std::cout << "ip = " << predictIPList[i] << std::endl;
        std::cout << "pPort = " << pPort << std::endl;
        std::cout << "mPort = " << mPort << std::endl;
        std::cout << "metricPort = " << metricPort << std::endl;
        threads.emplace_back(std::thread([predictIP, pPort, manageIP, mPort, metricPort] {
            HttpServer httpServer1;
            httpServer1.Init(1);
            // 设置回调函数
            ServerHandler handler1;
            auto mock = std::make_shared<SortTimeReUseMock>(predictIP);
            (*mock).InitMetrics();
            ClusterMock::GetCluster().AddInstance(predictIP, pPort, mock);
            // 注册请求到达时的回调函数
            handler1.RegisterFun(boost::beast::http::verb::post,
                "/v1/chat/completions",
                std::bind(&SortTimeReUseMock::OpenAISingleReqHandler, mock.get(), std::placeholders::_1));

            HttpServerParm parm1;
            parm1.address = predictIP;
            parm1.port = pPort;  // 端口号
            parm1.serverHandler = handler1;
            parm1.tlsItems.tlsEnable = false;
            parm1.maxKeepAliveReqs = 9999999;
            ServerHandler handler2;
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/prefill",
                std::bind(&SortTimeReUseMock::RolePHandler, mock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::post,
                "/v1/role/decode",
                std::bind(&SortTimeReUseMock::RoleDHandler, mock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/config",
                std::bind(&SortTimeReUseMock::ConfigHandler, mock.get(), std::placeholders::_1));
            handler2.RegisterFun(boost::beast::http::verb::get,
                "/v1/status",
                std::bind(&SortTimeReUseMock::StatusHandler, mock.get(), std::placeholders::_1));
            HttpServerParm parm2;
            parm2.address = manageIP;
            parm2.port = mPort;  // 端口号
            parm2.serverHandler = handler2;
            parm2.tlsItems.tlsEnable = false;
            parm2.maxKeepAliveReqs = 9999999;

            ServerHandler handler3;
            handler3.RegisterFun(boost::beast::http::verb::get,
                "/metrics",
                std::bind(&SortTimeReUseMock::MetricsHandler, mock.get(), std::placeholders::_1));
            HttpServerParm parm3;
            parm3.address = manageIP;
            parm3.port = metricPort;  // metric端口号
            parm3.serverHandler = handler3;
            parm3.tlsItems.tlsEnable = false;
            parm3.maxKeepAliveReqs = 9999999;

            httpServer1.Run({parm1, parm2, parm3});
        }));
        sleep(5);
    }

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));
    std::cout << "--------------------- create coodinator success ----------------------" << std::endl;

    nlohmann::json instancesInfo;
    instancesInfo["ids"] = nlohmann::json::array();
    instancesInfo["instances"] = nlohmann::json::array();

    auto peers = nlohmann::json::array();

    for (auto i = 0; i < numServer; i++) {
        instancesInfo["ids"].emplace_back(i);
        nlohmann::json instance;
        instance["id"] = i;
        instance["ip"] = predictIPList[i];
        instance["port"] = predictPortList[i];
        instance["metric_port"] = metricPortList[i];
        instance["inter_comm_port"] = interCommonPortLists[i];
        instance["model_name"] = "llama_65b";
        nlohmann::json static_info;
        nlohmann::json dynamic_info;
        static_info["group_id"] = 0;
        static_info["max_seq_len"] = 2048;
        static_info["max_output_len"] = 512;
        static_info["total_slots_num"] = 200;
        static_info["total_block_num"] = 512 * (i + 1);  // 不同Node分配不同的NPU MEM SIZE
        static_info["block_size"] = 128;
        static_info["label"] = 0;
        static_info["role"] = 85;
        static_info["virtual_id"] = i;
        static_info["p_percentage"] = 0;

        dynamic_info["avail_slots_num"] = 200;
        dynamic_info["avail_block_num"] = 1024;
        instance["static_info"] = static_info;
        instance["dynamic_info"] = dynamic_info;
        instancesInfo["instances"].emplace_back(instance);
    }

    TlsItems tlsItems;
    std::string response1;
    std::string response2;
    std::string response3;

    auto lambdaFunc = [this, tlsItems, instancesInfo]() -> bool {
        std::string response;
        auto ret = SendInferRequest(this->manageIP, this->managePort, tlsItems, "/v1/instances/refresh",
            instancesInfo.dump(), response);
        std::cout << "------------ SetSingleRole ret ----------------" << ret << std::endl;
        if (ret == 0) {
            std::cout << "------------ SetSingleRole Ready ----------------" << std::endl;
            return true;
        } else {
            return false;
        }
    };

    // 15s内1s间隔检查是否ready
    EXPECT_FALSE(WaitUtilTrue(lambdaFunc, 15, 1000));

    WaitCoordinatorReady(manageIP, managePort);
    std::cout << "--------------------- coodinator ready ----------------------" << std::endl;

    // 发送第一次指标查询请求
    auto ret = GetMetricsRequest(manageIP, externalPort, tlsItems, "/metrics", response1, false);
    EXPECT_EQ(ret, 200); // 200 表示 HTTP 请求成功

    // 发送第二次指标查询请求
    ret = GetMetricsRequest(manageIP, externalPort, tlsItems, "/metrics", response2, false);
    EXPECT_EQ(ret, 200); // 200 表示 HTTP 请求成功

    std::cout << "----------------printf metrics-------------------\n" << response1 << std::endl;
    EXPECT_EQ(response1, singleNodeMetrics[0]);

    std::cout << "----------------printf metrics-------------------\n" << response2 << std::endl;
    EXPECT_EQ(response2, response1);

    // 等待10s，发送第三次指标查询请求
    sleep(10);
    ret = GetMetricsRequest(manageIP, externalPort, tlsItems, "/metrics", response3, false);
    EXPECT_EQ(ret, 200); // 200 表示 HTTP 请求成功
    EXPECT_NE(response3, response2);
    std::cout << "----------------printf metrics-------------------\n" << response3 << std::endl;

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}