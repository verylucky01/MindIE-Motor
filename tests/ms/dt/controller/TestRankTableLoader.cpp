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

using namespace MINDIE::MS;

constexpr int32_t NUM_SIX = 6;

class TestRankTableLoader : public testing::Test {
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

/*
测试描述: 测试RankTableLoader，异构场景下加载集群信息表成功
测试步骤:
    1. 读取集群信息表。
    2. RankTableLoader调用LoadRankTable，预期结果1。
预期结果:
    1. LoadRankTable方法返回0表示成功。
    2. server节点的信息如下:
        - 节点1，ip 127.0.0.1, 800I A2(64G)
        - 节点2，ip 127.0.0.2, 800I A2(32G)
*/
TEST_F(TestRankTableLoader, TestHeterogeneousSuccess)
{
    auto controllerJson = GetMSControllerConfigJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    // 修改json文件
    if (ModifyJsonItem(testJson, "is_heterogeneous", "", true) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto rankTable = GetMSGlobalRankTableTestJsonPath();
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", rankTable.c_str(), 1);
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(serverNodes.size(), 2);
    EXPECT_EQ(serverNodes[0]->ip, "127.0.0.2");
    EXPECT_EQ(serverNodes[0]->roleState, ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN));
    EXPECT_EQ(serverNodes[0]->instanceInfo.staticInfo.nodeRes.hardwareType, "800I A2(64G)");
    EXPECT_EQ(serverNodes[0]->serverInfoList[0].deviceInfos.size(), 1);
    EXPECT_EQ(serverNodes[0]->serverInfoList[0].deviceInfos[0].id, "0");
    EXPECT_EQ(serverNodes[0]->serverInfoList[0].deviceInfos[0].ip, "127.0.0.2");

    EXPECT_EQ(serverNodes[1]->ip, "127.0.0.3");
    EXPECT_EQ(serverNodes[1]->roleState, ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN));
    EXPECT_EQ(serverNodes[1]->instanceInfo.staticInfo.nodeRes.hardwareType, "800I A2(32G)");
    EXPECT_EQ(serverNodes[1]->serverInfoList[0].deviceInfos.size(), 1);
    EXPECT_EQ(serverNodes[1]->serverInfoList[0].deviceInfos[0].id, "0");
    EXPECT_EQ(serverNodes[1]->serverInfoList[0].deviceInfos[0].ip, "127.0.0.3");
}

/*
测试描述: 测试RankTableLoader，异构场景下加载集群信息表，节点数量少于1
测试步骤:
    1. 覆写集群信息表，只保留一个server。
    2. RankTableLoader调用LoadRankTable，预期结果1。
预期结果:
    1. LoadRankTable方法返回0表示成功。
    2. server节点非空。
*/
TEST_F(TestRankTableLoader, TestHeterogeneousFailed)
{
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    // 修改json文件
    if (ModifyJsonItem(testJson, "is_heterogeneous", "", true) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0640);
    rawConfig["server_group_list"][2]["server_list"].erase(rawConfig["server_group_list"][2]["server_list"].begin()
        + 1);
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 1);
}

/*
测试描述: 测试RankTableLoader，同构场景下加载集群信息表成功
测试步骤:
    1. 读取集群信息表。
    2. RankTableLoader调用LoadRankTable，预期结果1。
预期结果:
    1. LoadRankTable方法返回0表示成功。
    2. server节点的信息如下:
        - 节点1，ip 127.0.0.1
        - 节点2，ip 127.0.0.2
*/
TEST_F(TestRankTableLoader, TestNonHeterogeneousSuccess)
{
    auto rankTable = GetMSGlobalRankTableTestJsonPath();
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", rankTable.c_str(), 1);
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);

    EXPECT_EQ(serverNodes.size(), 2);
    EXPECT_EQ(serverNodes[0]->ip, "127.0.0.2");
    EXPECT_EQ(serverNodes[0]->roleState, ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN));
    EXPECT_EQ(serverNodes[0]->instanceInfo.staticInfo.nodeRes.hardwareType, "");
    EXPECT_EQ(serverNodes[0]->serverInfoList[0].deviceInfos.size(), 1);
    EXPECT_EQ(serverNodes[0]->serverInfoList[0].deviceInfos[0].id, "0");
    EXPECT_EQ(serverNodes[0]->serverInfoList[0].deviceInfos[0].ip, "127.0.0.2");
    EXPECT_EQ(serverNodes[0]->serverInfoList[0].deviceInfos[0].logicalId, "0");

    EXPECT_EQ(serverNodes[1]->ip, "127.0.0.3");
    EXPECT_EQ(serverNodes[1]->roleState, ControllerConstant::GetInstance()->GetRoleState(RoleState::UNKNOWN));
    EXPECT_EQ(serverNodes[1]->instanceInfo.staticInfo.nodeRes.hardwareType, "");
    EXPECT_EQ(serverNodes[1]->serverInfoList[0].deviceInfos.size(), 1);
    EXPECT_EQ(serverNodes[1]->serverInfoList[0].deviceInfos[0].id, "0");
    EXPECT_EQ(serverNodes[1]->serverInfoList[0].deviceInfos[0].ip, "127.0.0.3");
    EXPECT_EQ(serverNodes[1]->serverInfoList[0].deviceInfos[0].logicalId, "0");
}

/*
测试描述: 测试RankTableLoader，同构场景下加载集群信息表失败，节点数量少于1
测试步骤:
    1. 覆写集群信息表，只保留一个server。
    2. RankTableLoader调用LoadRankTable，预期结果1。
预期结果:
    1. LoadRankTable方法返回0表示成功。
    2. server节点非空。
*/
TEST_F(TestRankTableLoader, TestNonHeterogeneousFailed)
{
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0640);
    rawConfig["server_group_list"][2]["server_list"].erase(rawConfig["server_group_list"][2]["server_list"].begin()
        + 1);
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 1);
}

/*
测试描述: 测试RankTableLoader，加载集群信息表，出现重复的ip
测试步骤:
    1. 覆写集群信息表，新增加一个重复的server。
    2. RankTableLoader调用LoadRankTable，预期结果1。
预期结果:
    1. LoadRankTable方法返回0表示失败。
    2. server节点为空。
*/
TEST_F(TestRankTableLoader, TestDupIpFailed)
{
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0640);
    rawConfig["server_group_list"][2]["server_list"].push_back(rawConfig["server_group_list"][2]["server_list"][0]);
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，同构场景下加载集群信息表失败，节点数量大于6 * 16
测试步骤:
    1. 覆写集群信息表，新增加95个server。
    2. RankTableLoader调用LoadRankTable，预期结果1。
预期结果:
    1. LoadRankTable方法返回0表示失败。
    2. server节点为空。
*/
TEST_F(TestRankTableLoader, TestNonHeterogeneousMaxInstanceNum)
{
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限777
    nlohmann::json serverNodeJson = {
        {"server_id", "127.0.0.1"},
        {"server_ip", "127.0.0.1"},
        {"hardware_type", "800I A2(64G)"},
        {"device", {{
            {"device_id", "0"},
            {"device_ip", "127.0.0.1"},
            {"device_logical_id", "0"},
            {"rank_id", "0"}
        }}}
    };
    for (uint32_t i = 1; i <= 6 * 16 - 1; i++) {
        std::string ip = "127.0.0." + std::to_string(i + 3);
        serverNodeJson["server_ip"] = ip;
        rawConfig["server_group_list"][2]["server_list"].push_back(serverNodeJson);
    }
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ChangeFileMode(newRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，异构场景下加载集群信息表失败，节点数量大于6 * 16
测试步骤:
    1. 覆写集群信息表，新增加95个server。
    2. RankTableLoader调用LoadRankTable，预期结果1。
预期结果:
    1. LoadRankTable方法返回0表示失败。
    2. server节点为空。
*/
TEST_F(TestRankTableLoader, TestHeterogeneousMaxInstanceNum)
{
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    // 修改json文件
    if (ModifyJsonItem(testJson, "is_heterogeneous", "", true) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0640);
    nlohmann::json serverNodeJson = {
        {"server_id", "127.0.0.1"},
        {"server_ip", "127.0.0.1"},
        {"hardware_type", "800I A2(64G)"},
        {"device", {{
               {"device_id", "0"},
               {"device_ip", "127.0.0.1"},
               {"device_logical_id", "0"},
               {"rank_id", "0"}
           }
       }}
    };
    for (uint32_t i = 1; i <= 6 * 16 - 1; i++) {
        std::string ip = "127.0.0." + std::to_string(i + 3);
        serverNodeJson["server_ip"] = ip;
        rawConfig["server_group_list"][2]["server_list"].push_back(serverNodeJson);
    }
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ChangeFileMode(newRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，group数量非法
测试步骤:
    1. 覆写集群信息表，server_group_list不存在，预期结果1。
    2. 覆写集群信息表，server_group_list为空，预期结果1。
    3. 覆写集群信息表，server_group_list为4，预期结果1。
预期结果:
    1. LoadRankTable方法返回-1表示失败，server节点为空。
*/
TEST_F(TestRankTableLoader, TestInvalidGroupList)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;

    std::cout << "test 1" << std::endl;
    nlohmann::json rankTable = {};
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rankTable.dump(4));
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 2" << std::endl;
    rankTable["server_group_list"] = nlohmann::json::array();
    DumpStringToFile(newRankTable, rankTable.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 3" << std::endl;
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    nlohmann::json obj = {};
    rawConfig["server_group_list"].push_back(obj);
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，coordinator非法
测试步骤:
    1. 覆写集群信息表，group0不存在，预期结果1。
    2. 覆写集群信息表，group0包含1个节点，ip非法，预期结果1。
    3. 覆写集群信息表，group0包含2个节点，预期结果1。
预期结果:
    1. LoadRankTable方法返回-1表示失败，server节点为空。
*/
TEST_F(TestRankTableLoader, TestInvalidCoordinator)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    std::cout << "test 1" << std::endl;
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][0]["group_id"] = "4";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 2" << std::endl;
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][0]["server_list"][0]["server_ip"] = "0.0.0.0";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 3" << std::endl;
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    nlohmann::json obj = {};
    rawConfig["server_group_list"][0]["server_list"].push_back(obj);
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，server非法
测试步骤:
    1. 覆写集群信息表，group2不存在，预期结果1。
    2. 覆写集群信息表，group2节点1的ip非法，预期结果1。
    3. 覆写集群信息表，group2节点1的设备ip非法，预期结果1。
    4. 覆写集群信息表，group2节点1的设备id非法，预期结果1。
    5. 覆写集群信息表，group2节点1的设备有129个，预期结果1。
    6. 异构场景下，设备的标签不合法，预期结果1。
预期结果:
    1. LoadRankTable方法返回-1表示失败，server节点为空。
*/
TEST_F(TestRankTableLoader, TestInvalidServer)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    std::cout << "test 1" << std::endl;
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["group_id"] = "-1";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 2" << std::endl;
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"][0]["server_ip"] = "0.0.0.0";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 3" << std::endl;
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"][0]["device"][0]["device_ip"] = "0.0.0.0";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 4" << std::endl;
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"][0]["device"] = nlohmann::json::array();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"][0]["device"][0]["device_id"] = "-1";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"][0]["device"][0]["device_id"] = "1073741825";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"][0]["device"][0]["device_logical_id"] = "-1";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"][0]["device"][0]["device_logical_id"] = "1073741825";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 5" << std::endl;
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    nlohmann::json deviceNodeJson = {
        {"device_id", "0"},
        {"device_ip", "127.0.0.1"},
        {"device_logical_id", "0"}
    };
    for (uint32_t i = 2; i <= 129; i++) {
        std::string ip = "127.0.0." + std::to_string(i);
        deviceNodeJson["device_id"] = std::to_string(i);
        deviceNodeJson["device_ip"] = ip;
        deviceNodeJson["device_logical_id"] = std::to_string(i);
        rawConfig["server_group_list"][2]["server_list"][0]["device"].push_back(deviceNodeJson);
    }
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);

    std::cout << "test 6" << std::endl;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    // 修改json文件
    if (ModifyJsonItem(testJson, "is_heterogeneous", "", true) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"][0]["hardware_type"] = "error";
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，节点为空
测试步骤:
    1. 覆写集群信息表，group0包含0个节点，预期结果1。
    2. 覆写集群信息表，group2包含0个节点，预期结果1。
预期结果:
    1. LoadRankTable方法返回0表示成功，server节点为空。
*/
TEST_F(TestRankTableLoader, TestValidEmpty)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    std::cout << "test 1" << std::endl;
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][0]["server_list"] = nlohmann::json::array();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 2);

    std::cout << "test 2" << std::endl;
    rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    rawConfig["server_group_list"][2]["server_list"] = nlohmann::json::array();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，测试json文件中不包括server的端口信息的场景
测试步骤:
    1. 解析默认global_ranktable.json，预期结果1。
预期结果:
    1. 集群信息解析成功，server节点的端口信息与Controller初始化配置文件中的一致。
*/
TEST_F(TestRankTableLoader, TestServerPortFromControllerConfig)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto rankTable = GetMSGlobalRankTableTestJsonPath();
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", rankTable.c_str(), 1);
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 2);
    EXPECT_EQ(serverNodes[0]->port, "1025");
    EXPECT_EQ(serverNodes[0]->mgmtPort, "1026");
    EXPECT_EQ(serverNodes[0]->metricPort, "1027");
    EXPECT_EQ(serverNodes[0]->interCommPort, ""); // 无集群间通信端口
}

/*
测试描述: 测试RankTableLoader，测试json文件中包括server端口信息的场景
测试步骤:
    1. 覆写集群信息表，添加server的端口信息，预期结果1。
预期结果:
    1. 集群信息解析成功，server节点的端口信息与ranktable中的一致。
*/
TEST_F(TestRankTableLoader, TestServerPortFromRankTable)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    auto serverList = rawConfig["server_group_list"][2]["server_list"];
    for (int i = 0; i < serverList.size(); i++) {
        auto server = serverList[i];
        server["predict_port"] = std::to_string(i * 4 + 2025);
        server["mgmt_port"] = std::to_string(i * 4 + 2026);
        server["metric_port"] = std::to_string(i * 4 + 2027);
        server["inter_comm_port"] = std::to_string(i * 4 + 2028);
        rawConfig["server_group_list"][2]["server_list"][i] = server;
    }
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);

    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);

    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 2);
    EXPECT_EQ(serverNodes[0]->port, "2025");
    EXPECT_EQ(serverNodes[0]->mgmtPort, "2026");
    EXPECT_EQ(serverNodes[0]->metricPort, "2027");
    EXPECT_EQ(serverNodes[0]->interCommPort, "2028");
}

/*
测试描述: 测试RankTableLoader，测试json文件中server的宿主机ip非法的场景
测试步骤:
    1. 覆写集群信息表，server节点的宿主机ip非法，预期结果1。
预期结果:
    1. 集群信息解析失败，集群中无server节点。
*/
TEST_F(TestRankTableLoader, TestServerHostIpInvalid)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    for (auto &server : rawConfig["server_group_list"][2]["server_list"]) {
        server["server_id"] = "server"; // Not a valid ipv4 address
    }
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);

    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);

    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，测试json文件中server的端口非法的场景
测试步骤:
    1. 覆写集群信息表，server节点的端口非法，预期结果1。
预期结果:
    1. 集群信息解析失败，集群中无server节点。
*/
TEST_F(TestRankTableLoader, TestServerPortInvalid)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto originRankTable = GetMSGlobalRankTableTestJsonPath();
    auto rawConfig = FileToJsonObj(originRankTable, 0777); // 读取权限0777
    for (auto &server : rawConfig["server_group_list"][2]["server_list"]) {
        server["predict_port"] = "1"; // Invalid port number
    }
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);

    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);

    EXPECT_NE(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，解析跨机场景集群信息表
测试步骤:
    1. 覆写controller配置文件，设置开启跨机特性。
预期结果:
    1. LoadRankTable方法返回0表示成功，server实例数量为2，每个实例包含的节点数正确。
*/
TEST_F(TestRankTableLoader, LoadCrossNodeServers)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetCrossNodeMSRankTableLoaderJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);

    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 2);
    EXPECT_EQ(serverNodes[0]->serverInfoList.size(), 2);
}

/*
测试描述: 测试RankTableLoader，构造非法多机集群信息，解析跨机场景集群信息表失败
测试步骤:
    1. 覆写controller配置文件，设置开启跨机特性。
    2. 修改节点ip非法，读取rank_table文件
预期结果:
    1. LoadRankTable方法返回1表示失败
*/
TEST_F(TestRankTableLoader, LoadCrossNodeServersFailed)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetCrossNodeMSRankTableLoaderJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);
    auto rawConfig = FileToJsonObj(crossNodeRankTable, 0777); // 读取权限0777
    for (auto &server : rawConfig["server_group_list"][3]["server_list"]) { // 3: third server group
        server["server_ip"] = "aaaaa"; // Invalid ip number
    }
    auto newRankTable = GetMSRankTableLoaderTestJsonPath();
    DumpStringToFile(newRankTable, rawConfig.dump(4));
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", newRankTable.c_str(), 1);
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 0);
}

/*
测试描述: 测试RankTableLoader，构造多机集群信息，D实例为分布式，解析文件成功
测试步骤:
    1. 覆写controller配置文件，设置开启D节点分布式特性。
预期结果:
    1. LoadRankTable方法返回0表示读取成功
*/
TEST_F(TestRankTableLoader, LoadDistributeNodeServersSuccess)
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
    auto crossNodeRankTable = GetCrossNodeMSRankTableLoaderJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 2);
}

/*
测试描述: 测试RankTableLoader，构造A3场景多机集群信息，D实例为分布式，解析文件成功
测试步骤:
    1. 覆写controller配置文件，设置开启D节点分布式特性。
    2. 读取ranktable文件
预期结果:
    1. LoadRankTable方法返回0表示读取成功
    2. 读取到节点数量为3，1个集中式Prefill节点，2个分布式decode节点
*/
TEST_F(TestRankTableLoader, LoadA3DistributeNodeServersSuccess)
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
    auto crossNodeRankTable = GetA3CrossNodeMSRankTableLoaderJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);
    auto ret = rankTableLoader->LoadRankTable(serverNodes, coordinatorStore);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(serverNodes.size(), 3); // 3 = 1 + 2, 1: prefill node, 2: decode node
}

/*
测试描述: 测试RankTableLoader，解析跨机场景集群ranktable，获取podInfo信息
测试步骤:
    1. 覆写controller配置文件，设置开启跨机特性。
    2. 调用GetPDPodInfoList方法，获取podInfo信息。
    3. 查看获取的podInfo个数是否正确。
预期结果:
    1. GetPDPodInfoList方法返回的vector非空，且size大小等于server数量，预期结果为2。
*/
TEST_F(TestRankTableLoader, GetPDPodInfoListForCrossNodeServers)
{
    std::vector<std::unique_ptr<NodeInfo>> serverNodes;
    auto controllerJson = GetMSControllerTestJsonPath();
    auto testJson = GetServerRequestHandlerTestJsonPath();
    CopyFile(controllerJson, testJson);
    JsonFileManager manager(testJson);
    manager.Load();
    manager.SetList({ "multi_node_infer_config", "multi_node_infer_enable" }, true);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
    ControllerConfig::GetInstance()->Init();
    auto crossNodeRankTable = GetCrossNodeMSRankTableLoaderJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);
    // 调用GetPDPodInfoList方法，获取podInfo信息
    std::vector<std::unique_ptr<PodInfo>> podInfoList;
    podInfoList = rankTableLoader->GetPDPodInfoList();
    // 预期podInfoList非空，且size大小等于server数量
    EXPECT_NE(podInfoList.size(), 0);
    EXPECT_EQ(podInfoList.size(), NUM_SIX);
}

/*
测试描述: 测试RankTableLoader，构造多机集群信息，D实例为分布式，获取podInfo信息
测试步骤:
    1. 覆写controller配置文件，设置开启D节点分布式特性。
    2. 调用GetPDPodInfoList方法，获取podInfo信息。
    3. 查看获取的podInfo个数是否正确。
预期结果:
    1. GetPDPodInfoList方法返回的vector非空，且size大小等于server数量，预期结果为2。
*/
TEST_F(TestRankTableLoader, GetPDPodInfoListForDistributeNodeServers)
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
    auto crossNodeRankTable = GetCrossNodeMSRankTableLoaderJsonPath();
    ChangeFileMode600(crossNodeRankTable);
    setenv("GLOBAL_RANK_TABLE_FILE_PATH", crossNodeRankTable.c_str(), 1);
    // 调用GetPDPodInfoList方法，获取podInfo信息
    std::vector<std::unique_ptr<PodInfo>> podInfoList;
    podInfoList = rankTableLoader->GetPDPodInfoList();
    // 预期podInfoList非空，且size大小等于server数量
    EXPECT_NE(podInfoList.size(), 0);
    EXPECT_EQ(podInfoList.size(), NUM_SIX);
}