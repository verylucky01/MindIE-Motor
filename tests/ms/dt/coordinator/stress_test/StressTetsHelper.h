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
#ifndef StressTetsHelper_H
#define StressTetsHelper_H

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <map>
#include <algorithm>
#include <functional>
#include "gtest/gtest.h"
#include "JsonFileManager.h"
#include "Helper.h"
#include "MindIEServer.h"
#include "RequestHelper.h"
#include "ConcurrentTest.h"
#define main __main_coordinator__
#include "coordinator/main.cpp"


using namespace MINDIE::MS;

using ReqTestFun = std::function<void(const std::string &, const std::string &)>;
using MetricsReqTestFun = std::function<void(const std::string &, const std::string &, const std::string &)>;

// PD分离压测
void StressTetsPDHelper(uint32_t numMindIEServer, ReqTestFun reqTestFun, MetricsReqTestFun metricsFun,
    uint32_t numThreadInfer = 10, uint32_t pRate = 0, uint32_t dRate = 0)
{
    std::cout << "---------------------StressTetsPDHelper Test start ----------------------" << std::endl;

    ThreadSafeVector<std::string> predictIPList;
    std::string manageIP = "127.0.0.1";
    std::uint32_t managePort = 1026;
    std::string predictIP = "127.0.0.1";
    std::uint32_t predictPort = 1025;
    std::uint32_t metricsPort = 1028;
    std::string controllerIP = "127.0.0.1";
    std::uint32_t controllerPort = 1027;

    // 自动生成ip列表
    for (auto i = 1; i <= numMindIEServer; i++) {
        predictIPList.emplace_back("172.16.0." + std::to_string(i));
    }

    // 配置默认文件
    CopyDefaultConfig();
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    auto controllerJson = GetMSControllerTestJsonPath();
    auto rankTable = GetMSGlobalRankTableTestJsonPath();
    auto ldLibraryPath = GetLdLibraryPath();
    auto hseceasyPath = GetHSECEASYPATH();

    // 配置全局信息表
    SetRankTableByServerNum(numMindIEServer, rankTable);

    // 设置启动环境变量
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", rankTable.c_str(), 1);
    setenv("LD_LIBRARY_PATH", ldLibraryPath.c_str(), 1);
    setenv("HSECEASY_PATH", hseceasyPath.c_str(), 1);
    setenv("MIES_SERVICE_MONITOR_MODE", "1", 1);
    std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;
    std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
    std::cout << "GLOBAL_RANK_TABLE_FILE_PATH=" << rankTable << std::endl;
    std::cout << "LD_LIBRARY_PATH=" << ldLibraryPath << std::endl;
    std::cout << "HSECEASY_PATH=" << hseceasyPath << std::endl;

    // 修改启动配置文件
    {
        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"log_info", "log_level"}, "ERROR");
        manager.SetList({"http_config", "server_thread_num"}, 4);
        manager.SetList({"http_config", "client_thread_num"}, 4);
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, std::to_string(predictPort));
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, std::to_string(managePort));
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_mangment_tls_enable"}, false);
        manager.Save();
    }

    {
        JsonFileManager manager(controllerJson);
        manager.Load();
        manager.SetList({"log_info", "log_level"}, "ERROR");
        manager.Set("default_p_rate", pRate);
        manager.Set("default_d_rate", dRate);
        manager.Set("mindie_server_control_port", managePort);
        manager.Set("mindie_server_port", predictPort);
        manager.Set("mindie_server_metric_port", metricsPort);
        manager.Set("mindie_ms_coordinator_port", managePort);
        manager.SetList({"http_server", "ip"}, controllerIP);
        manager.SetList({"http_server", "port"}, controllerPort);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Save();
    }

    // 读取metrics的预期结果
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string stressPDSeparateData = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsPDSeparateNodeData.bin");
    std::ifstream readFile(stressPDSeparateData);
    std::stringstream buf;
    buf << readFile.rdbuf();
    std::string metricsStressTestPDSeparate = buf.str();
    readFile.close();

    // 先清理到参与进程
    ExecuteCommand("pkill -9 ms_controller");
    // 获取进程路径
    auto controllerBinPath = GetMSControllerBinPath();

    // 启动仿真的mindie server
    std::vector<std::thread> threads;
    for (auto index = 0; index < numMindIEServer; index++) {
        std::cout << "ip = " << predictIPList[index] << ", predict port = " << predictPort <<
            ", manage port = " << managePort << ", metircs port = " << metricsPort << std::endl;
        threads.emplace_back(std::thread([&] {
            CreateMindIEServer(predictIPList[index], std::to_string(predictPort),
                predictIPList[index], std::to_string(managePort), 4, 0, std::to_string(metricsPort));  // 4 线程 0 编号
        }));
        sleep(5);
    }

    // 启动ms_controller
    std::thread threadController(RunCmdBackGround, controllerBinPath);

    // 启动ms_coordinator
    std::cout << "Start coordinator" << std::endl;
    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadCoordinator([&] {
        __main_coordinator__(1, argsCoordinator);
    });
    ASSERT_TRUE(threadController.joinable());
    ASSERT_TRUE(threadCoordinator.joinable());

    // 等待ms_coordinator 处于ready状态
    auto strManagePort = std::to_string(managePort);
    WaitCoordinatorReady(manageIP, strManagePort);

    // 并发发送推理请求
    std::vector<std::thread> inferThreads;
    for (auto index = 0; index < numThreadInfer; index++) {
        inferThreads.emplace_back(std::thread([&] {
            reqTestFun(predictIP, std::to_string(predictPort));
            metricsFun(manageIP, std::to_string(managePort), metricsStressTestPDSeparate);
        }));
    }

    // 等待推理请求处理结束
    for (size_t i = 0; i < inferThreads.size(); ++i) {
        inferThreads[i].join();
    }

    // 确保业务是可用的
    std::cout << "---------------final infer------------------" << std::endl;
    sleep(1);
    reqTestFun(predictIP, std::to_string(predictPort));
    metricsFun(manageIP, std::to_string(managePort), metricsStressTestPDSeparate);

    // 结束 ms_controller 进程
    ExecuteCommand("pkill -9 ms_controller");

    // 仿真mindie server放置后台执行，避免线程中断
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    threadCoordinator.detach();
    threadController.detach();
    std::cout << "---------------------StressTetsPDHelper Test end ----------------------" << std::endl;
}

// PD分离TLS压测
void StressTetsPDTLSHelper(uint32_t numMindIEServer, ReqTestFun reqTestFun, MetricsReqTestFun metricsFun,
    uint32_t numThreadInfer = 10, uint32_t pRate = 0, uint32_t dRate = 0)
{
    std::cout << "---------------------StressTetsPDSSLHelper Test start ----------------------" << std::endl;

    ThreadSafeVector<std::string> predictIPList;
    std::string manageIP = "127.0.0.1";
    std::uint32_t managePort = 1026;
    std::string predictIP = "127.0.0.1";
    std::uint32_t predictPort = 1025;
    std::uint32_t metricsPort = 1028;
    std::string controllerIP = "127.0.0.1";
    std::uint32_t controllerPort = 1027;

    // 自动生成ip列表
    for (auto i = 1; i <= numMindIEServer; i++) {
        predictIPList.emplace_back("172.16.0." + std::to_string(i));
    }

    // 配置默认文件
    CopyDefaultConfig();
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    auto controllerJson = GetMSControllerTestJsonPath();
    auto rankTable = GetMSGlobalRankTableTestJsonPath();
    auto ldLibraryPath = GetLdLibraryPath();

    // 配置全局信息表
    SetRankTableByServerNum(numMindIEServer, rankTable);

    // 设置启动环境变量
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", rankTable.c_str(), 1);
    setenv("LD_LIBRARY_PATH", ldLibraryPath.c_str(), 1);
    setenv("MIES_SERVICE_MONITOR_MODE", "1", 1);
    std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;
    std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
    std::cout << "GLOBAL_RANK_TABLE_FILE_PATH=" << rankTable << std::endl;
    std::cout << "LD_LIBRARY_PATH=" << ldLibraryPath << std::endl;

    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");
    setenv("HSECEASY_PATH", GetHSECEASYPATH().c_str(), 1);
    // 修改启动配置文件
    {
        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"log_info", "log_level"}, "ERROR");
        manager.SetList({"log_info", "to_file"}, true);
        manager.SetList({"log_info", "run_log_path"}, "./logs/ms_coordinator_run_log.txt");
        manager.SetList({"log_info", "operation_log_path"}, "./logs/ms_coordinator_operation_log.txt");
        manager.SetList({"http_config", "server_thread_num"}, 4);
        manager.SetList({"http_config", "client_thread_num"}, 4);
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, std::to_string(predictPort));
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, std::to_string(managePort));

        manager.SetList({"tls_config", "request_server_tls_enable"}, true);
        manager.SetList({"tls_config", "request_server_tls_items", "ca_cert"},
            GetAbsolutePath(certDir, "coordinator_to_user/ca.pem"));
        manager.SetList({"tls_config", "request_server_tls_items", "tls_cert"},
            GetAbsolutePath(certDir, "coordinator_to_user/cert.pem"));
        manager.SetList({"tls_config", "request_server_tls_items", "tls_key"},
            GetAbsolutePath(certDir, "coordinator_to_user/cert.key.pem"));
        manager.SetList({"tls_config", "request_server_tls_items", "tls_passwd"},
            GetAbsolutePath(certDir, "coordinator_to_user/cert_passwd.txt"));
        manager.SetList({"tls_config", "request_server_tls_items", "kmcKsfMaster"},
            GetAbsolutePath(certDir, "coordinator_to_user/tools/pmt/master/ksfa"));
        manager.SetList({"tls_config", "request_server_tls_items", "kmcKsfStandby"},
            GetAbsolutePath(certDir, "coordinator_to_user/tools/pmt/standby/ksfb"));
        manager.SetList({"tls_config", "request_server_tls_items", "tls_crl"}, "");

        manager.SetList({"tls_config", "mindie_client_tls_enable"}, true);
        manager.SetList({"tls_config", "mindie_client_tls_items", "ca_cert"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/ca.pem"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "tls_cert"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert.pem"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "tls_key"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert.key.pem"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "tls_passwd"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert_passwd.txt"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "kmcKsfMaster"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/tools/pmt/master/ksfa"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "kmcKsfStandby"},
            GetAbsolutePath(certDir, "coordinator_to_mindie_server/tools/pmt/standby/ksfb"));
        manager.SetList({"tls_config", "mindie_client_tls_items", "tls_crl"}, "");

        manager.SetList({"tls_config", "controller_server_tls_enable"}, true);
        manager.SetList({"tls_config", "controller_server_tls_items", "ca_cert"},
            GetAbsolutePath(certDir, "coordinator_to_controller/ca.pem"));
        manager.SetList({"tls_config", "controller_server_tls_items", "tls_cert"},
            GetAbsolutePath(certDir, "coordinator_to_controller/cert.pem"));
        manager.SetList({"tls_config", "controller_server_tls_items", "tls_key"},
            GetAbsolutePath(certDir, "coordinator_to_controller/cert.key.pem"));
        manager.SetList({"tls_config", "controller_server_tls_items", "tls_passwd"},
            GetAbsolutePath(certDir, "coordinator_to_controller/cert_passwd.txt"));
        manager.SetList({"tls_config", "controller_server_tls_items", "kmcKsfMaster"},
            GetAbsolutePath(certDir, "coordinator_to_controller/tools/pmt/master/ksfa"));
        manager.SetList({"tls_config", "controller_server_tls_items", "kmcKsfStandby"},
            GetAbsolutePath(certDir, "coordinator_to_controller/tools/pmt/standby/ksfb"));
        manager.SetList({"tls_config", "controller_server_tls_items", "tls_crl"}, "");

        manager.SetList({"tls_config", "mindie_mangment_tls_enable"}, true);
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
    }

    {
        JsonFileManager manager(controllerJson);
        manager.Load();
        manager.SetList({"log_info", "log_level"}, "ERROR");
        manager.Set("default_p_rate", pRate);
        manager.Set("default_d_rate", dRate);
        manager.Set("mindie_server_control_port", managePort);
        manager.Set("mindie_server_port", predictPort);
        manager.Set("mindie_server_metric_port", metricsPort);
        manager.Set("mindie_ms_coordinator_port", managePort);
        manager.SetList({"http_server", "ip"}, controllerIP);
        manager.SetList({"http_server", "port"}, controllerPort);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "http_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_mangment_tls_enable"}, false);

        manager.SetList({"tls_config", "request_coordinator_tls_enable"}, true);
        manager.SetList({"tls_config", "request_coordinator_tls_items", "ca_cert"},
            GetAbsolutePath(certDir, "controller_to_coordinator/ca.pem"));
        manager.SetList({"tls_config", "request_coordinator_tls_items", "tls_cert"},
            GetAbsolutePath(certDir, "controller_to_coordinator/cert.pem"));
        manager.SetList({"tls_config", "request_coordinator_tls_items", "tls_key"},
            GetAbsolutePath(certDir, "controller_to_coordinator/cert.key.pem"));
        manager.SetList({"tls_config", "request_coordinator_tls_items", "tls_passwd"},
            GetAbsolutePath(certDir, "controller_to_coordinator/cert_passwd.txt"));
        manager.SetList({"tls_config", "request_coordinator_tls_items", "kmc_ksf_master"},
            GetAbsolutePath(certDir, "controller_to_coordinator/tools/pmt/master/ksfa"));
        manager.SetList({"tls_config", "request_coordinator_tls_items", "kmc_ksf_standby"},
            GetAbsolutePath(certDir, "controller_to_coordinator/tools/pmt/standby/ksfb"));
        manager.SetList({"tls_config", "request_coordinator_tls_items", "tls_crl"}, "");
        manager.Save();
    }

    // 读取metrics的预期结果
    std::string stressPDSeparateData = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsPDSeparateNodeData.bin");
    std::ifstream readFile(stressPDSeparateData);
    std::stringstream buf;
    buf << readFile.rdbuf();
    std::string metricsStressTestPDSeparate = buf.str();
    readFile.close();

    // 先清理到参与进程
    ExecuteCommand("pkill -9 ms_controller");
    // 获取进程路径
    auto controllerBinPath = GetMSControllerBinPath();

    // 启动仿真的mindie server
    std::vector<std::thread> threads;
    for (auto index = 0; index < numMindIEServer; index++) {
        std::cout << "ip = " << predictIPList[index] << ", predict port = " << predictPort <<
            ", manage port = " << managePort << ", metircs port = " << metricsPort << std::endl;
        threads.emplace_back(std::thread([&] {
            CreateSSLServer(predictIPList[index], std::to_string(predictPort),
                predictIPList[index], std::to_string(managePort), 4, std::to_string(metricsPort));  // 4 线程
        }));
        sleep(5);
    }

    // 启动ms_coordinator
    std::cout << "Start coordinator" << std::endl;
    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadCoordinator([&] {
        __main_coordinator__(1, argsCoordinator);
    });

    // 启动ms_controller
    std::thread threadController(RunCmdBackGround, controllerBinPath);

    ASSERT_TRUE(threadController.joinable());
    ASSERT_TRUE(threadCoordinator.joinable());

    // 等待ms_coordinator 处于ready状态
    auto strManagePort = std::to_string(managePort);
    TlsItems tlsItems;
    tlsItems.caCert = GetAbsolutePath(certDir, "controller_to_coordinator/ca.pem");
    tlsItems.tlsCert = GetAbsolutePath(certDir, "controller_to_coordinator/cert.pem");
    tlsItems.tlsKey = GetAbsolutePath(certDir, "controller_to_coordinator/cert.key.pem");
    tlsItems.tlsPasswd = GetAbsolutePath(certDir, "controller_to_coordinator/cert_passwd.txt");
    tlsItems.kmcKsfMaster = GetAbsolutePath(certDir, "controller_to_coordinator/tools/pmt/master/ksfa");
    tlsItems.kmcKsfStandby = GetAbsolutePath(certDir, "controller_to_coordinator/tools/pmt/standby/ksfb");
    WaitCoordinatorReadySSL(manageIP, strManagePort, tlsItems);

    std::cout << "1111111111111111111111111111" << std::endl;

    // 并发发送推理请求
    std::vector<std::thread> inferThreads;
    for (auto index = 0; index < numThreadInfer; index++) {
        inferThreads.emplace_back(std::thread([&] {
            reqTestFun(predictIP, std::to_string(predictPort));
            metricsFun(manageIP, std::to_string(managePort), metricsStressTestPDSeparate);
        }));
    }

    // 等待推理请求处理结束
    for (size_t i = 0; i < inferThreads.size(); ++i) {
        inferThreads[i].join();
    }

    // 确保业务是可用的
    std::cout << "---------------final infer------------------" << std::endl;
    sleep(1);
    reqTestFun(predictIP, std::to_string(predictPort));
    metricsFun(manageIP, std::to_string(managePort), metricsStressTestPDSeparate);

    // 结束 ms_controller 进程
    ExecuteCommand("pkill -9 ms_controller");

    // 仿真mindie server放置后台执行，避免线程中断
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    threadCoordinator.detach();
    threadController.detach();
    std::cout << "---------------------StressTetsPDSSLHelper Test end ----------------------" << std::endl;
    sleep(5); // 停5s，等待后台的所有操作结束
}

// 单机场景压测
void StressTetsSingleNodeHelper(uint32_t numMindIEServer, ReqTestFun reqTestFun, MetricsReqTestFun metricsFun,
    uint32_t numThreadInfer = 10)
{
    std::cout << "---------------------StressTetsSingleNodeHelper Test start ----------------------" << std::endl;

    ThreadSafeVector<std::string> predictIPList;
    std::string manageIP = "127.0.0.1";
    std::uint32_t managePort = 1026;
    std::string predictIP = "127.0.0.1";
    std::uint32_t predictPort = 1025;
    std::uint32_t metricsPort = 1028;
    std::string controllerIP = "127.0.0.1";
    std::uint32_t controllerPort = 1027;

    // 自动生成ip列表
    for (auto i = 1; i <= numMindIEServer; i++) {
        predictIPList.emplace_back("172.16.0." + std::to_string(i));
    }

    // 配置默认文件
    CopyDefaultConfig();
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    auto controllerJson = GetMSControllerTestJsonPath();
    auto rankTable = GetMSGlobalRankTableTestJsonPath();
    auto ldLibraryPath = GetLdLibraryPath();
    auto hseceasyPath = GetHSECEASYPATH();

    // 配置全局信息表
    SetRankTableByServerNum(numMindIEServer, rankTable);

    // 设置启动环境变量
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", rankTable.c_str(), 1);
    setenv("LD_LIBRARY_PATH", ldLibraryPath.c_str(), 1);
    setenv("HSECEASY_PATH", hseceasyPath.c_str(), 1);
    setenv("MIES_SERVICE_MONITOR_MODE", "1", 1);
    std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;
    std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
    std::cout << "GLOBAL_RANK_TABLE_FILE_PATH=" << rankTable << std::endl;
    std::cout << "LD_LIBRARY_PATH=" << ldLibraryPath << std::endl;
    std::cout << "HSECEASY_PATH=" << hseceasyPath << std::endl;

    // 修改启动配置文件
    {
        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"log_info", "log_level"}, "ERROR");
        manager.SetList({"http_config", "server_thread_num"}, 4);
        manager.SetList({"http_config", "client_thread_num"}, 4);
        manager.SetList({"http_config", "predict_ip"}, predictIP);
        manager.SetList({"http_config", "predict_port"}, std::to_string(predictPort));
        manager.SetList({"http_config", "manage_ip"}, manageIP);
        manager.SetList({"http_config", "manage_port"}, std::to_string(managePort));
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
    }

    {
        JsonFileManager manager(controllerJson);
        manager.Load();
        manager.SetList({"log_info", "log_level"}, "ERROR");
        manager.Set("deploy_mode", "single_node");
        manager.Set("mindie_server_control_port", managePort);
        manager.Set("mindie_server_port", predictPort);
        manager.Set("mindie_ms_coordinator_port", managePort);
        manager.Set("mindie_server_metric_port", metricsPort);
        manager.SetList({"http_server", "ip"}, controllerIP);
        manager.SetList({"http_server", "port"}, controllerPort);
        manager.SetList({"tls_config", "controller_server_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "mindie_client_tls_enable"}, false);
        manager.SetList({"tls_config", "external_tls_enable"}, false);
        manager.SetList({"tls_config", "status_tls_enable"}, false);
        manager.Save();
    }

    // 读取metrics的预期结果
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string stressSingleNodeData = GetAbsolutePath(
        parentPath, "tests/ms/common/.mindie_ms/ms_metricsStressSingleNodeData.bin");
    std::ifstream readFile(stressSingleNodeData);
    std::stringstream buf;
    buf << readFile.rdbuf();
    std::string metricsStressTestSingleNode = buf.str();
    readFile.close();

    // 先清理到参与进程
    ExecuteCommand("pkill -9 ms_controller");

    // 获取进程路径
    auto controllerBinPath = GetMSControllerBinPath();

    // 启动仿真的mindie server
    std::vector<std::thread> threads;
    for (auto index = 0; index < numMindIEServer; index++) {
        std::cout << "ip = " << predictIPList[index] << ", predict port = " << predictPort <<
            ", manage port = " << managePort << ", metircs port = " << metricsPort << std::endl;
        threads.emplace_back(std::thread([&] {
            CreateSingleServer(predictIPList[index], std::to_string(predictPort),
                predictIPList[index], std::to_string(managePort), 4, 0, std::to_string(metricsPort));  // 4 线程 0 编号
        }));
        sleep(5);
    }

    // 启动ms_controller
    std::thread threadController(RunCmdBackGround, controllerBinPath);

    // 启动ms_coordinator
    std::cout << "Start coordinator" << std::endl;
    char *argsCoordinator[1] = {"ms_coordinator"};
    std::thread threadCoordinator([&] {
        __main_coordinator__(1, argsCoordinator);
    });
    ASSERT_TRUE(threadController.joinable());
    ASSERT_TRUE(threadCoordinator.joinable());

    // 等待ms_coordinator 处于ready状态
    auto strManagePort = std::to_string(managePort);
    WaitCoordinatorReady(manageIP, strManagePort);

    // 并发发送推理请求
    std::vector<std::thread> inferThreads;
    for (auto index = 0; index < numThreadInfer; index++) {
        inferThreads.emplace_back(std::thread([&] {
            reqTestFun(predictIP, std::to_string(predictPort));
            metricsFun(manageIP, std::to_string(managePort), metricsStressTestSingleNode);
        }));
    }

    // 等待推理请求处理结束
    for (size_t i = 0; i < inferThreads.size(); ++i) {
        inferThreads[i].join();
    }

    // 确保业务是可用的
    std::cout << "---------------final infer------------------" << std::endl;
    sleep(1);
    reqTestFun(predictIP, std::to_string(predictPort));
    metricsFun(manageIP, std::to_string(managePort), metricsStressTestSingleNode);

    // 结束 ms_controller 进程
    ExecuteCommand("pkill -9 ms_controller");

    // 仿真mindie server放置后台执行，避免线程中断
    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].detach();
    }
    threadCoordinator.detach();
    threadController.detach();
    std::cout << "---------------------StressTetsSingleNodeHelper Test end ----------------------" << std::endl;
    sleep(5); // 停5s，等待后台的所有操作结束
}
#endif