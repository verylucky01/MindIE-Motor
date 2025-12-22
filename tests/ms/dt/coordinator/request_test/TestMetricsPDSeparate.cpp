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
#include <thread>
#include "gtest/gtest.h"
#define main __main_coordinator__
#include "coordinator/main.cpp"
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "ThreadSafeVector.h"
#include "stub.h"


using namespace MINDIE::MS;

class TestMetricsPDSeparate : public testing::Test {
protected:

    void SetUp()
    {
        std::string exePath = GetExecutablePath();
        std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));

        std::string aggregate4PDNodeOutput = GetAbsolutePath(
            parentPath, "tests/ms/common/.mindie_ms/ms_metricsAggregate4PDSeparateOutput.bin");
        std::ifstream readFile(aggregate4PDNodeOutput);
        std::stringstream buf;
        buf << readFile.rdbuf();
        metricsAggregate4PDNodeOutput = buf.str();
        readFile.close();

        CopyFile(GetMSCoordinatorConfigJsonPath(), GetMSCoordinatorTestJsonPath());
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;
        setenv("HSECEASY_PATH", GetHSECEASYPATH().c_str(), 1);
        // 设置监控指标环境变量
        setenv("MIES_SERVICE_MONITOR_MODE", "1", 1);

        std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");

        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, predictPort);
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, managePort);
        manager.SetList({"http_config", "external_port"}, externalPort);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "controller_server_tls_items", "ca_cert"},
            GetAbsolutePath(certDir, "coordinator_manage_to_user/ca.pem"));
        manager.SetList({"tls_config", "controller_server_tls_items", "tls_cert"},
            GetAbsolutePath(certDir, "coordinator_manage_to_user/cert.pem"));
        manager.SetList({"tls_config", "controller_server_tls_items", "tls_key"},
            GetAbsolutePath(certDir, "coordinator_manage_to_user/cert.key.pem"));
        manager.SetList({"tls_config", "controller_server_tls_items", "tls_passwd"},
            GetAbsolutePath(certDir, "coordinator_manage_to_user/cert_passwd.txt"));
        manager.SetList({"tls_config", "controller_server_tls_items", "kmcKsfMaster"},
            GetAbsolutePath(certDir, "coordinator_manage_to_user/tools/pmt/master/ksfa"));
        manager.SetList({"tls_config", "controller_server_tls_items", "kmcKsfStandby"},
            GetAbsolutePath(certDir, "coordinator_manage_to_user/tools/pmt/standby/ksfb"));

        manager.SetList({"tls_config", "request_server_tls_enable"}, false);

        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);

        manager.SetList({"tls_config", "mindie_mangment_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_mangment_tls_items", "ca_cert"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/ca.pem"));
        manager.SetList({"tls_config", "mindie_mangment_tls_items", "tls_cert"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert.pem"));
        manager.SetList({"tls_config", "mindie_mangment_tls_items", "tls_key"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert.key.pem"));
        manager.SetList({"tls_config", "mindie_mangment_tls_items", "tls_passwd"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert_passwd.txt"));
        manager.SetList({"tls_config", "mindie_mangment_tls_items", "kmcKsfMaster"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/tools/pmt/master/ksfa"));
        manager.SetList({"tls_config", "mindie_mangment_tls_items", "kmcKsfStandby"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/tools/pmt/standby/ksfb"));
        manager.Save();
        for (auto i = 0; i < numMindIEServer; i++) {
            predictIPList.emplace_back("127.0.0.1");
        }

        // change the permission of cert files
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator_manage/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator_manage/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator_manage/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator_manage/cert_passwd.txt"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator_manage/tools/pmt/master/ksfa"),
            S_IRUSR | S_IWUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator_manage/tools/pmt/standby/ksfb"),
            S_IRUSR | S_IWUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_manage_to_user/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_manage_to_user/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_manage_to_user/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_manage_to_user/cert_passwd.txt"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_manage_to_user/tools/pmt/master/ksfa"),
            S_IRUSR | S_IWUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_manage_to_user/tools/pmt/standby/ksfb"),
            S_IRUSR | S_IWUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert_passwd.txt"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/tools/pmt/master/ksfa"),
            S_IRUSR | S_IWUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/tools/pmt/standby/ksfb"),
            S_IRUSR | S_IWUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert_passwd.txt"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/tools/pmt/master/ksfa"),
            S_IRUSR | S_IWUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/tools/pmt/standby/ksfb"),
            S_IRUSR | S_IWUSR);
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
    std::string metricsAggregate4PDNodeOutput;
};

/*
测试描述: 测试服务监控指标查询，PD分离多实例场景
测试步骤:
    1. 拉起PD分离Server
    2. 向coordinator发送/metrics请求，预期结果1
预期结果:
    1. 请求正常返回
*/
TEST_F(TestMetricsPDSeparate, TestPDSeparateMetricsTC01)
{
    // 把config里面的证书选择置为false
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"tls_config", "request_server_tls_enable"}, false);
    manager.SetList({"tls_config", "mindie_mangment_tls_enable"}, false);
    manager.Save();

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
        threads.push_back(std::thread(CreateMindIEServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(mPort),
        4, i, std::move(metricPort))); // 默认4线程启动服务
    }

    std::cout << "--------------------- Create single server done ----------------------" << std::endl;

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    std::cout << "--------------------- create coodinator success ----------------------" << std::endl;

    // 2P 2D
    int32_t numPRole = 2;
    int32_t numDRole = 2;
    nlohmann::json instancesInfo = SetPDRoleByList(predictIPList, predictPortList, managePortList,
        interCommonPortLists, numPRole, numDRole);

    // 更新npu_mem_size
    for (auto i = 0; i < numPRole + numDRole; i++) {
        instancesInfo["instances"][i]["static_info"]["total_block_num"] = 512 * (i + 1);  // 512 分配NPU内存大小
        instancesInfo["instances"][i]["metric_port"] = metricPortList[i];
    }

    TlsItems tlsItems;
    std::string response;

    auto lambdaFunc = [this, tlsItems, instancesInfo]() -> bool {
        std::string response;
        auto ret = SendInferRequest(this->manageIP, this->managePort, tlsItems, "/v1/instances/refresh",
            instancesInfo.dump(), response);
        std::cout << "------------ SetPDRole ret ----------------" << ret << std::endl;
        if (ret == 0) {
            std::cout << "------------ SetPDRole Ready ----------------" << std::endl;
            return true;
        } else {
            return false;
        }
    };

    // 15s内1s间隔检查是否ready
    EXPECT_FALSE(WaitUtilTrue(lambdaFunc, 15, 1000));  // 15 s, 1000 ms

    WaitCoordinatorReady(manageIP, managePort);

    std::cout << "--------------------- coodinator ready ----------------------" << std::endl;

    // 发送指标查询请求，默认超时时间60，最大重复请求次数3
    auto ret = GetMetricsRequest(manageIP, externalPort, tlsItems, "/metrics", response, false);
    EXPECT_EQ(ret, 200); // 200 表示 HTTP 请求成功
    EXPECT_EQ(response, metricsAggregate4PDNodeOutput);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}

/*
测试描述: 测试服务监控指标查询，PD分离多实例SSL场景
测试步骤:
    1. 拉起PD分离Server，设置证书
    2. 向coordinator发送/metrics请求，预期结果1
预期结果:
    1. 请求正常返回
*/
TEST_F(TestMetricsPDSeparate, TestPDSeparateMetricsTC02)
{
    // 把config里面的证书选择置为true
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"tls_config", "controller_server_tls_enable"}, true);
    manager.SetList({"tls_config", "mindie_mangment_tls_enable"}, true);
    manager.Save();

    std::cout << "--------------------- Test begin ----------------------" << std::endl;
    std::vector<std::thread> threads;
    for (auto i = 0; i < numMindIEServer; i++) {
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

        threads.push_back(std::thread(CreateMetricsSSLServer,
        std::move(predictIP), std::move(pPort),
        std::move(manageIP), std::move(metricPort),
        4, i)); // 默认4线程启动服务
    }

    std::cout << "--------------------- Create single server done ----------------------" << std::endl;

    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");

    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadObj(__main_coordinator__, 1, std::ref(argsCoordinator));

    std::cout << "--------------------- create coodinator success ----------------------" << std::endl;

    // 2P 2D
    int32_t numPRole = 2;
    int32_t numDRole = 2;
    nlohmann::json instancesInfo = SetPDRoleByList(predictIPList, predictPortList, managePortList,
        interCommonPortLists, numPRole, numDRole);

    // 更新npu_mem_size
    for (auto i = 0; i < numPRole + numDRole; i++) {
        instancesInfo["instances"][i]["static_info"]["total_block_num"] = 512 * (i + 1);  // 512 分配NPU内存大小
        instancesInfo["instances"][i]["metric_port"] = metricPortList[i];
    }

    std::string response;
    TlsItems tlsItems;
    tlsItems.caCert = GetAbsolutePath(certDir, "user_to_coordinator_manage/ca.pem");
    tlsItems.tlsCert = GetAbsolutePath(certDir, "user_to_coordinator_manage/cert.pem");
    tlsItems.tlsKey = GetAbsolutePath(certDir, "user_to_coordinator_manage/cert.key.pem");
    tlsItems.tlsPasswd = GetAbsolutePath(certDir, "user_to_coordinator_manage/cert_passwd.txt");
    tlsItems.kmcKsfMaster = GetAbsolutePath(certDir, "user_to_coordinator_manage/tools/pmt/master/ksfa");
    tlsItems.kmcKsfStandby = GetAbsolutePath(certDir, "user_to_coordinator_manage/tools/pmt/standby/ksfb");

    auto lambdaFunc = [this, tlsItems, instancesInfo]() -> bool {
        std::string response;
        auto ret = SendSSLInferRequest(this->manageIP, this->managePort, tlsItems, "/v1/instances/refresh",
            instancesInfo.dump(), response);
        std::cout << "------------ SetPDRole ret ----------------" << ret << std::endl;
        if (ret == 0) {
            std::cout << "------------ SetPDRole Ready ----------------" << std::endl;
            return true;
        } else {
            return false;
        }
    };

    // 15s内1s间隔检查是否ready
    EXPECT_FALSE(WaitUtilTrue(lambdaFunc, 15, 1000));  // 15 s, 1000 ms

    WaitCoordinatorReadySSL(manageIP, managePort, tlsItems);

    std::cout << "--------------------- coodinator ready ----------------------" << std::endl;

    // 发送指标查询请求，默认超时时间60，最大重复请求次数3
    auto ret = GetMetricsRequest(manageIP, externalPort, tlsItems, "/metrics", response, true);
    EXPECT_EQ(ret, 200); // 200 表示 HTTP 请求成功
    EXPECT_EQ(response, metricsAggregate4PDNodeOutput);

    EXPECT_TRUE(threadObj.joinable());
    // 将线程放入后台运行
    threadObj.detach();
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    std::cout << "--------------------- Test end ----------------------" << std::endl;
}
