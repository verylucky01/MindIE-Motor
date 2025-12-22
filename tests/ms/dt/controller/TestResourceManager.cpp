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
#include <memory>
#include <iostream>
#include "gtest/gtest.h"
#include "stub.h"
#include "RankTableLoader.h"
#include "ControllerConfig.h"
#include "JsonFileLoader.h"
#include "Helper.h"
#include "JsonFileManager.h"
#include "ResourceManager.h"

using namespace MINDIE::MS;

class TestResourceManager : public testing::Test {
public:
    void SetUp() override
    {
        InitNodeStatus();
        InitCoordinatorStore();
        InitRankTableLoader();

        CopyDefaultConfig();
        auto controllerJson = GetMSControllerTestJsonPath();
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
        ControllerConfig::GetInstance()->Init();
        ChangeFileMode(controllerJson);
        auto rankTable = GetMSGlobalRankTableTestJsonPath();
        ChangeFileMode(rankTable);
    }

    void InitNodeStatus()
    {
        nodeStatus = std::make_shared<NodeStatus>();
    }

    void InitCoordinatorStore()
    {
        coordinatorStore = std::make_shared<CoordinatorStore>();
    }

    void InitRankTableLoader()
    {
        rankTableLoader = std::make_shared<RankTableLoader>();
    }
    std::shared_ptr<RankTableLoader> rankTableLoader;
    std::shared_ptr<NodeStatus> nodeStatus;
    std::shared_ptr<CoordinatorStore> coordinatorStore;
};

TEST_F(TestResourceManager, TestInitResourceManager)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.SetList({ "multi_node_infer_config", "d_node_config", "enable_dist_dp_server" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetA2CrossNodeMSRankTableLoaderJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);

    std::vector<std::unique_ptr<InstanceInfo>> instanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->Init(instanceInfoList);

    std::vector<uint64_t> pInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : pInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> dInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    for (const auto instanceLogicId : dInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }
}

TEST_F(TestResourceManager, TestMatchInstance)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.SetList({ "multi_node_infer_config", "d_node_config", "enable_dist_dp_server" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetA2CrossNodeMSRankTableLoaderJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);
    
    std::vector<std::unique_ptr<InstanceInfo>> instanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->Init(instanceInfoList);

    std::vector<uint64_t> pInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : pInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> dInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    for (const auto instanceLogicId : dInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<std::unique_ptr<InstanceInfo>> newInstanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->UpdateInstanceTable(newInstanceInfoList);
    std::vector<uint64_t> newPInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : newPInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> newDInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    for (const auto instanceLogicId : newDInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }
}

TEST_F(TestResourceManager, TestUnMatchInstance)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.SetList({ "multi_node_infer_config", "d_node_config", "enable_dist_dp_server" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetA2CrossNodeMSRankTableLoaderJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);
    
    std::vector<std::unique_ptr<InstanceInfo>> instanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->Init(instanceInfoList);

    std::vector<uint64_t> pInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : pInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> dInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    for (const auto instanceLogicId : dInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    auto rawConfig = FileToJsonObj(crossNodeRankTable, 0777); // 读取权限777
    auto& serverList = rawConfig["server_group_list"];
    serverList.erase(serverList.end()-1);
    auto deleteID = 1;
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);

    std::vector<std::unique_ptr<InstanceInfo>> newInstanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->UpdateInstanceTable(newInstanceInfoList);
    std::vector<uint64_t> newPInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : newPInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> newDInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    for (const auto instanceLogicId : newDInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        if (instanceLogicId == deleteID) {
            EXPECT_EQ(instanceStatus, InstanceStatus::FAULT);
        } else {
            EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
        }
    }
}

TEST_F(TestResourceManager, TestAddInstance)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.SetList({ "multi_node_infer_config", "d_node_config", "enable_dist_dp_server" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetA2CrossNodeMSRankTableLoaderJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);

    std::vector<std::unique_ptr<InstanceInfo>> instanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->Init(instanceInfoList);

    std::vector<uint64_t> pInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : pInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> dInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    for (const auto instanceLogicId : dInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    auto rawConfig = FileToJsonObj(crossNodeRankTable, 0777); // 读取权限777
    auto& serverList = rawConfig["server_group_list"];
    nlohmann::json instanceJson = {
        {"deploy_server", "1"},
        {"group_id", "4"},
        {"server_count", "2"},
        {"server_list",{
            {
                {"device", {
                    {
                        {"device_id", "0"},
                        {"device_ip", "10.0.0.6"},
                        {"device_logical_id", "0"},
                        {"rank_id", "0"},
                        {"super_device_id", "1234566"}
                    }
                }},
                {"server_id", "127.0.0.13"},
                {"server_ip", "127.0.0.14"}
            },
            {
                {"device",{ 
                    {
                        {"device_id", "0"},
                        {"device_ip", "10.0.0.7"},
                        {"device_logical_id", "0"},
                        {"rank_id", "0"},
                        {"super_device_id", "1234567"}
                    }
                }},
                {"server_id", "127.0.0.15"},
                {"server_ip", "127.0.0.16"}
            }
        }},
        {"super_pod_list",{
            {
                {"server_list", {
                    {
                        {"server_id", "127.0.0.13"}
                    }
                }},
                {"super_pod_id", "0"},
            }
        }}
    };
    serverList.emplace_back(instanceJson);
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);

    std::vector<std::unique_ptr<InstanceInfo>> newInstanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->UpdateInstanceTable(newInstanceInfoList);
    std::vector<uint64_t> newPInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : newPInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> newDInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    EXPECT_EQ(newDInstancesLogicIds.size(), 3);
    for (const auto instanceLogicId : newDInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }
}

TEST_F(TestResourceManager, TestInheritInstance01)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.SetList({ "multi_node_infer_config", "d_node_config", "enable_dist_dp_server" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetA2CrossNodeMSRankTableLoaderJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);

    std::vector<std::unique_ptr<InstanceInfo>> instanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->Init(instanceInfoList);

    std::vector<uint64_t> pInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : pInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> dInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    for (const auto instanceLogicId : dInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    auto rawConfig = FileToJsonObj(crossNodeRankTable, 0777); // 读取权限777
    auto& serverList = rawConfig["server_group_list"];
    serverList.erase(serverList.end()-1);
    auto deleteID = 1;
    nlohmann::json instanceJson = {
        {"deploy_server", "1"},
        {"group_id", "4"},
        {"server_count", "2"},
        {"server_list",{
            {
                {"device", {
                    {
                        {"device_id", "0"},
                        {"device_ip", "10.0.0.6"},
                        {"device_logical_id", "0"},
                        {"rank_id", "0"},
                        {"super_device_id", "1234566"}
                    }
                }},
                {"server_id", "127.0.0.13"},
                {"server_ip", "127.0.0.14"}
            },
            {
                {"device",{ 
                    {
                        {"device_id", "0"},
                        {"device_ip", "10.0.0.7"},
                        {"device_logical_id", "0"},
                        {"rank_id", "0"},
                        {"super_device_id", "1234567"}
                    }
                }},
                {"server_id", "127.0.0.15"},
                {"server_ip", "127.0.0.16"}
            }
        }},
        {"super_pod_list",{
            {
                {"server_list", {
                    {
                        {"server_id", "127.0.0.13"}
                    }
                }},
                {"super_pod_id", "0"},
            }
        }}
    };
    serverList.emplace_back(instanceJson);
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);

    std::vector<std::unique_ptr<InstanceInfo>> newInstanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->UpdateInstanceTable(newInstanceInfoList);
    std::vector<uint64_t> newPInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : newPInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> newDInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    EXPECT_EQ(newDInstancesLogicIds.size(), 2);
    for (const auto instanceLogicId : newDInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        if (instanceLogicId == deleteID) {
            auto it1 = std::find(serverNodes.begin(), serverNodes.end(), "127.0.0.13");
            EXPECT_NE(it1, serverNodes.end());
            auto it2 = std::find(serverNodes.begin(), serverNodes.end(), "127.0.0.15");
            EXPECT_NE(it2, serverNodes.end());                        
        } 
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }
}

TEST_F(TestResourceManager, TestInheritInstance02)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.SetList({ "multi_node_infer_config", "d_node_config", "enable_dist_dp_server" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetA2CrossNodeMSRankTableLoaderJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);

    std::vector<std::unique_ptr<InstanceInfo>> instanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->Init(instanceInfoList);

    std::vector<uint64_t> pInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : pInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> dInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    for (const auto instanceLogicId : dInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    auto rawConfig = FileToJsonObj(crossNodeRankTable, 0777); // 读取权限777
    auto& serverList = rawConfig["server_group_list"];
    serverList.erase(serverList.end()-1);
    serverList.erase(serverList.end()-1);
    auto deleteID = 0;
    auto inheritID = 1;
    
    nlohmann::json instanceJson = {
        {"deploy_server", "1"},
        {"group_id", "4"},
        {"server_count", "2"},
        {"server_list",{
            {
                {"device", {
                    {
                        {"device_id", "0"},
                        {"device_ip", "10.0.0.6"},
                        {"device_logical_id", "0"},
                        {"rank_id", "0"},
                        {"super_device_id", "1234566"}
                    }
                }},
                {"server_id", "127.0.0.13"},
                {"server_ip", "127.0.0.14"}
            },
            {
                {"device",{ 
                    {
                        {"device_id", "0"},
                        {"device_ip", "10.0.0.7"},
                        {"device_logical_id", "0"},
                        {"rank_id", "0"},
                        {"super_device_id", "1234567"}
                    }
                }},
                {"server_id", "127.0.0.15"},
                {"server_ip", "127.0.0.16"}
            }
        }},
        {"super_pod_list",{
            {
                {"server_list", {
                    {
                        {"server_id", "127.0.0.13"}
                    }
                }},
                {"super_pod_id", "0"},
            }
        }}
    };
    serverList.emplace_back(instanceJson);
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);

    std::vector<std::unique_ptr<InstanceInfo>> newInstanceInfoList = 
        rankTableLoader->GetInstanceInfoListByRankTable();
    ResourceManager::GetInstance()->UpdateInstanceTable(newInstanceInfoList);
    std::vector<uint64_t> newPInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    for (const auto instanceLogicId : newPInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
    }

    std::vector<uint64_t> newDInstancesLogicIds =
        ResourceManager::GetInstance()->GetInstancesLogicIds(MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    EXPECT_EQ(newDInstancesLogicIds.size(), 2);
    for (const auto instanceLogicId : newDInstancesLogicIds) {
        std::vector<std::string> serverNodes = 
            ResourceManager::GetInstance()->GetInstanceAllServerIP(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        auto instanceStatus = 
            ResourceManager::GetInstance()->GetInstanceStatus(
                MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE,
                instanceLogicId);
        EXPECT_EQ(serverNodes.size(), 2);
        if (instanceLogicId == deleteID) {
            EXPECT_EQ(instanceStatus, InstanceStatus::FAULT);
        } else {
            if (instanceLogicId == inheritID) {
                auto it1 = std::find(serverNodes.begin(), serverNodes.end(), "127.0.0.13");
                EXPECT_NE(it1, serverNodes.end());
                auto it2 = std::find(serverNodes.begin(), serverNodes.end(), "127.0.0.15");
                EXPECT_NE(it2, serverNodes.end());                        
            }
            EXPECT_EQ(instanceStatus, InstanceStatus::ACTIVE);
        }
    }
}