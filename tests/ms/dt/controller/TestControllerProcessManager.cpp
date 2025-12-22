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
#include <cstdlib> // #include <stdlib.h>
#include <string>
#include <memory>
#include <pthread.h>
#include "gtest/gtest.h"
#include "ProcessManager.h"
#include "NodeStatus.h"
#include "ControllerConfig.h"
#include "digs_instance.h"
#include "Helper.h"
#include "RankTableLoader.h"

using namespace MINDIE::MS;

constexpr int32_t TWO = 2;

class TestControllerProcessManager : public testing::Test {
protected:
    void SetUp() override
    {
        nodeStatus = std::make_shared<NodeStatus>();

        auto controllerJson = GetMSControllerConfigJsonPath();
        auto testJson = GetServerRequestHandlerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "tls_config", "request_coordinator_tls_enable", false);
        ModifyJsonItem(testJson, "tls_config", "request_server_tls_enable", false);
        ModifyJsonItem(testJson, "tls_config", "http_server_tls_enable", false);
        ControllerConfig::GetInstance()->Init();
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << testJson << std::endl;
    }

    std::shared_ptr<NodeStatus> nodeStatus;
};

/*
测试描述: 服务端配置文件测试，生成集群信息
测试步骤:
    1. 初始化两个节点的配置信息，包括IP地址、端口等。
    2. 调用ProcessManager的GenerateClusterInfo方法，生成包含节点信息的JSON。
预期结果:
    1. 生成的JSON包含两个节点的信息，且每个节点的字段与设置值一致。
    2. 验证节点间的peers和activePeers信息与预期值匹配。
*/
ServerInfo buildServer1()
{
    ServerInfo serverInfo1;
    serverInfo1.hostId = "192.168.99.1";
    serverInfo1.ip = "192.168.99.2";
    DeviceInfo deviceInfo1;
    deviceInfo1.id = "0";
    deviceInfo1.ip = "192.168.99.3";
    deviceInfo1.logicalId = "0";
    deviceInfo1.rankId = 0;
    serverInfo1.deviceInfos.emplace_back(deviceInfo1);
    DeviceInfo deviceInfo2;
    deviceInfo2.id = "0";
    deviceInfo2.ip = "192.168.99.4";
    deviceInfo2.logicalId = "0";
    deviceInfo2.rankId = 1;
    serverInfo1.deviceInfos.emplace_back(deviceInfo2);
    std::vector<DeviceInfo> deviceInfosList {deviceInfo1, deviceInfo2};
    serverInfo1.dpGroupInfos[0] = deviceInfosList;
    return serverInfo1;
}

ServerInfo buildServer2()
{
    ServerInfo serverInfo2;
    serverInfo2.hostId = "192.168.99.5";
    serverInfo2.ip = "192.168.99.6";
    DeviceInfo deviceInfo3;
    deviceInfo3.id = "0";
    deviceInfo3.ip = "192.168.99.7";
    deviceInfo3.logicalId = "0";
    deviceInfo3.rankId = 0;
    serverInfo2.deviceInfos.emplace_back(deviceInfo3);
    DeviceInfo deviceInfo4;
    deviceInfo4.id = "0";
    deviceInfo4.ip = "192.168.99.8";
    deviceInfo4.logicalId = "0";
    deviceInfo4.rankId = 1;
    serverInfo2.deviceInfos.emplace_back(deviceInfo4);
    std::vector<DeviceInfo> deviceInfosList2 {deviceInfo3, deviceInfo4};
    serverInfo2.dpGroupInfos[0] = deviceInfosList2;
    return serverInfo2;
}

TEST_F(TestControllerProcessManager, TestGenerateClusterInfo)
{
    // 模拟节点信息
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    auto nodeInfo2 = std::make_unique<NodeInfo>();

    nodeInfo1->ip = "172.16.0.1";
    nodeInfo1->port = "8080";
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
    nodeInfo1->serverInfoList.emplace_back(buildServer1());
    nodeInfo1->serverInfoList.emplace_back(buildServer2());

    nodeInfo2->ip = "172.16.0.2";
    nodeInfo2->port = "8081";
    nodeInfo2->isHealthy = true;
    nodeInfo2->isInitialized = false;
    nodeInfo2->roleState = "initializing";
    nodeInfo2->modelName = "model2";
    nodeInfo2->peers = { 16781740 };
    nodeInfo2->activePeers = { 16781740 };
    nodeInfo2->instanceInfo.staticInfo.id = 33558956;
    nodeInfo2->instanceInfo.staticInfo.groupId = 2;
    nodeInfo2->instanceInfo.staticInfo.maxSeqLen = 2048;
    nodeInfo2->instanceInfo.staticInfo.maxOutputLen = 512;
    nodeInfo2->instanceInfo.staticInfo.totalSlotsNum = 300;
    nodeInfo2->instanceInfo.staticInfo.totalBlockNum = 2048;
    nodeInfo2->instanceInfo.staticInfo.blockSize = 256;
    nodeInfo2->instanceInfo.staticInfo.label = MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER;
    nodeInfo2->instanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;

    // 调用 AddNode 时传递 std::unique_ptr<NodeInfo>
    nodeStatus->AddNode(std::move(nodeInfo1));
    nodeStatus->AddNode(std::move(nodeInfo2));

    // 调用 GenerateClusterInfo 方法
    nlohmann::json clusterInfo =  ProcessManager::GetInstance()->GenerateClusterInfo(nodeStatus);

    // 验证 JSON 结构
    EXPECT_TRUE(clusterInfo.is_object());
    EXPECT_TRUE(clusterInfo.contains("server"));
    EXPECT_EQ(clusterInfo["server"].size(), 2);

    // 验证第一个节点
    auto firstNode = clusterInfo["server"][0];
    EXPECT_EQ(firstNode["id"], 16781740);
    EXPECT_EQ(firstNode["ip"], "172.16.0.1");
    EXPECT_EQ(firstNode["is_initialized"], true);
    EXPECT_EQ(firstNode["static_info"]["group_id"], 1);
    EXPECT_EQ(firstNode["static_info"]["total_slots_num"], 200);
    EXPECT_EQ(firstNode["static_info"]["total_block_num"], 1024);
    EXPECT_EQ(firstNode["static_info"]["label"], MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER);
    EXPECT_EQ(firstNode["static_info"]["role"], MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    EXPECT_EQ(firstNode["peers"].size(), 1);
    EXPECT_EQ(firstNode["peers"][0], 33558956);
    EXPECT_EQ(firstNode["server_info_list"].size(), TWO);

    // 验证第二个节点
    auto secondNode = clusterInfo["server"][1];
    EXPECT_EQ(secondNode["id"], 33558956);
    EXPECT_EQ(secondNode["ip"], "172.16.0.2");
    EXPECT_EQ(secondNode["is_initialized"], false);
    EXPECT_EQ(secondNode["static_info"]["group_id"], TWO);
    EXPECT_EQ(secondNode["static_info"]["total_slots_num"], 300);
    EXPECT_EQ(secondNode["static_info"]["total_block_num"], 2048);
    EXPECT_EQ(secondNode["static_info"]["label"], MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER);
    EXPECT_EQ(secondNode["static_info"]["role"], MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    EXPECT_EQ(secondNode["peers"].size(), 1);
    EXPECT_EQ(secondNode["peers"][0], 16781740);
}

/*
测试描述: 测试 GetStatusFromPath 方法及其调用的 FillNodeInfo, ResizeGroups, 和 RecordEmptyGroups 函数。
测试步骤:
    1. 创建包含预期节点信息的 JSON 数据。
    2. 初始化服务器列表，包括 IP 地址和其他信息。
    3. 调用 GetStatusFromPath 方法，验证三个函数的正确调用。
    4. 验证返回的节点信息、分组信息和无效分组。
预期结果:
    1. FillNodeInfo 函数能正确更新节点信息。
    2. ResizeGroups 函数能正确调整分组并分配节点。
    3. RecordEmptyGroups 能正确识别空的或无效的分组。
*/
TEST_F(TestControllerProcessManager, TestGetStatusFromPathWithFunctions)
{
    nlohmann::json statusJson = {
        {"server", {
            {
                {"ip", "172.16.0.1"},
                {"isHealthy", true},
                {"static_info", {
                    {"id", 16781740},
                    {"role", MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE},
                    {"group_id", 1},
                    {"total_slots_num", 64},
                    {"total_block_num", 32},
                    {"label", MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER},
                    {"virtual_id", 16781740}
                }},
                {"is_initialized", true},
                {"model_name", "Model_X"},
                {"peers", {33558956}}
            },
            {
                {"ip", "172.16.0.2"},
                {"isHealthy", true},
                {"static_info", {
                    {"id", 33558956},
                    {"role", MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE},
                    {"group_id", 1},
                    {"total_slots_num", 128},
                    {"total_block_num", 64},
                    {"label", MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER},
                    {"virtual_id", 33558956}
                }},
                {"is_initialized", true},
                {"model_name", "Model_Y"},
                {"peers", {16781740}}
            }
        }}
    };

    std::vector<std::unique_ptr<NodeInfo>> servers;
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    nodeInfo1->ip = "172.16.0.1";
    nodeInfo1->instanceInfo.staticInfo.id = 16781740;
    nodeInfo1->instanceInfo.staticInfo.groupId = 1;
    nodeInfo1->instanceInfo.staticInfo.virtualId = 16781740; //  16781740: virtualId
    servers.emplace_back(std::move(nodeInfo1));

    auto nodeInfo2 = std::make_unique<NodeInfo>();
    nodeInfo2->ip = "172.16.0.2";
    nodeInfo2->instanceInfo.staticInfo.id = 33558956;
    nodeInfo2->instanceInfo.staticInfo.groupId = 1;
    nodeInfo2->instanceInfo.staticInfo.virtualId = 33558956; // 33558956: virtualId
    servers.emplace_back(std::move(nodeInfo2));

    std::vector<std::unique_ptr<NodeInfo>> availableServers;
    std::vector<std::unique_ptr<NodeInfo>> faultyServers;
    GroupInfo groupInfo;
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> &groups = groupInfo.groups;
    std::map<uint64_t, std::vector<uint64_t>> &flexGroup = groupInfo.flexGroups;

    int32_t status = ProcessManager::GetInstance()->GetStatusFromPath(statusJson, servers, availableServers,
        faultyServers, groupInfo);

    ASSERT_EQ(availableServers.size(), 2);
    EXPECT_EQ(availableServers[0]->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(availableServers[0]->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(availableServers[0]->instanceInfo.staticInfo.totalSlotsNum, 64);
    EXPECT_EQ(availableServers[0]->instanceInfo.staticInfo.totalBlockNum, 32);
    EXPECT_EQ(availableServers[0]->instanceInfo.staticInfo.blockSize, 128);
    EXPECT_EQ(availableServers[0]->instanceInfo.staticInfo.label, MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER);
    EXPECT_EQ(availableServers[0]->peers.size(), 1);
    EXPECT_EQ(availableServers[0]->peers[0], 33558956);
    
    EXPECT_EQ(availableServers[1]->instanceInfo.staticInfo.maxSeqLen, 2048);
    EXPECT_EQ(availableServers[1]->instanceInfo.staticInfo.maxOutputLen, 512);
    EXPECT_EQ(availableServers[1]->instanceInfo.staticInfo.totalSlotsNum, 128);
    EXPECT_EQ(availableServers[1]->instanceInfo.staticInfo.totalBlockNum, 64);
    EXPECT_EQ(availableServers[1]->instanceInfo.staticInfo.blockSize, 128);
    EXPECT_EQ(availableServers[1]->instanceInfo.staticInfo.label, MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER);
    EXPECT_EQ(availableServers[1]->peers.size(), 1);
    EXPECT_EQ(availableServers[1]->peers[0], 16781740);

    EXPECT_EQ(groups[1].first.size(), 1);
    EXPECT_EQ(groups[1].first[0], 16781740); // PREFILL_INSTANCE
    EXPECT_EQ(groups[1].second.size(), 1);
}

/*
测试描述: 测试 GetNodeInfoFromPath 获取异常。
测试步骤:
    1. 创建包含预期节点信息的 JSON 数据。
    2. 初始化服务器列表，包括 IP 地址和其他信息。
    3. 调用 GetNodeInfoFromPath 方法，验证三个函数的正确调用。
    4. 验证返回的节点信息、分组信息和无效分组。
预期结果:
    1. FillNodeInfo 函数能正确更新节点信息。
    2. ResizeGroups 函数能正确调整分组并分配节点。
    3. RecordEmptyGroups 能正确识别空的或无效的分组。
*/
TEST_F(TestControllerProcessManager, TestGetNodeInfoFromPathFailedWithFunctions)
{
    nlohmann::json statusJson = {
        {
            "server", {
            {
                {"ip", "172.16.0.1"},
                {"delete_time", 0},
                {"is_initialized", true},
                {"model_name", "Model_X"},
                {"peers", {33558957}},
                {"active_peers", {9513933698241}},
                {"current_role", 80},
                {"delete_time", 0},
                {"dp_group_peers", {}},
                {"host_id", "192.168.0.0"},
                {"inference_type", 0},
                {"inter_comm_port", "1029"},
                {"is_distribute", false},
                {"is_faulty", false},
                {"is_healthy", true},
                {"is_initialized", true},
                {"metric_port", "1030"},
                {"mgmt_port", "1031"},
                {"port", "1032"},
                {"role_state", "RoleReady"},
                {"server_info_list", {
                    {
                        {"hostId", "172.16.0.5"}
                    }
                }}
            }
        }
        },
        {
            "instance_start_id_number", 0
        }
    };

    std::vector<std::unique_ptr<NodeInfo>> servers;
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    nodeInfo1->ip = "172.16.0.1";
    nodeInfo1->mgmtPort = "1031";
    nodeInfo1->instanceInfo.staticInfo.id = 1;
    nodeInfo1->instanceInfo.staticInfo.groupId = 1;
    nodeInfo1->instanceInfo.staticInfo.virtualId = 1;
    uint64_t id1 = 0;
    AllocateNodeId(nodeInfo1, id1);
    servers.emplace_back(std::move(nodeInfo1));
    std::vector<std::unique_ptr<NodeInfo>> availableServers;
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::map<uint64_t, std::vector<uint64_t>> flexGroup;
    int32_t status =
        ProcessManager::GetInstance()->GetNodeInfoFromPath(statusJson, servers, availableServers, groups, flexGroup);

    ASSERT_EQ(availableServers.size(), 0);
}

/*
测试描述: 测试 GetNodeInfoFromPath 获取成功方法。
测试步骤:
    1. 创建包含预期节点信息的 JSON 数据。
    2. 初始化服务器列表，包括 IP 地址和其他信息。
    3. 调用 GetNodeInfoFromPath 方法，验证三个函数的正确调用。
    4. 验证返回的节点信息、分组信息和无效分组。
预期结果:
    1. FillNodeInfo 函数能正确更新节点信息。
    2. ResizeGroups 函数能正确调整分组并分配节点。
    3. RecordEmptyGroups 能正确识别空的或无效的分组。
*/
TEST_F(TestControllerProcessManager, TestGetNodeInfoFromPathSuccessWithFunctions)
{
    nlohmann::json statusJson = {
        {
            "server", {
            {
                {"ip", "172.16.0.1"},
                {"delete_time", 0},
                {"static_info", {
                    {"id", 16781740},
                    {"role", MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE},
                    {"group_id", 1},
                    {"total_slots_num", 64},
                    {"total_block_num", 32},
                    {"label", MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER},
                    {"block_size", 128},
                    {"max_output_len", 2048},
                    {"max_seq_len", 2560},
                    {"virtual_id", 16781740}
                }},
                {"is_initialized", true},
                {"model_name", "Model_X"},
                {"peers", {33558956}},
                {"active_peers", {9513933698240}},
                {"current_role", 80},
                {"delete_time", 0},
                {"dp_group_peers", {}},
                {"dynamic_info", {
                    {"avail_block_num", 37028},
                    {"avail_slots_num", 200}
                }},
                
                {"host_id", "192.168.0.0"},
                {"inference_type", 0},
                {"inter_comm_port", "1026"},
                {"is_distribute", false},
                {"is_faulty", false},
                {"is_healthy", true},
                {"is_initialized", true},
                {"metric_port", "1027"},
                {"mgmt_port", "1026"},
                {"port", "1025"},
                {"role_state", "RoleReady"},
                {"server_info_list", {
                    {
                        {"hostId", "172.16.0.4"},
                        {"ip", "172.16.0.5"},
                        {"is_master", true},
                        {"sp_size", 1},
                        {"cp_size", 1},
                        {"device_infos", {
                            {
                                {"id", "0"},
                                {"ip", "172.16.0.3"},
                                {"logical_id", "0"},
                                {"rank_id", 0}
                            }
                        }},
                        {"dp_group_infos", {
                            {
                                {"id", 9691240320000},
                                {"device_infos", {
                                    {
                                        {"id", "0"},
                                        {"ip", "172.16.0.3"},
                                        {"logical_id", "0"},
                                        {"rank_id", 0}
                                    }
                                }}
                            }
                        }}
                    }
                }}
            },
            {
                {"ip", "172.16.0.2"},
                {"delete_time", 0},
                {"static_info", {
                    {"id", 33558956},
                    {"role", MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE},
                    {"group_id", 1},
                    {"total_slots_num", 128},
                    {"total_block_num", 64},
                    {"label", MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER},
                    {"block_size", 128},
                    {"max_output_len", 2048},
                    {"max_seq_len", 2560},
                    {"virtual_id", 33558956}
                }},
                {"is_initialized", true},
                {"model_name", "Model_Y"},
                {"peers", {16781740}},
                {"active_peers", {9513933698240}},
                {"current_role", 80},
                {"delete_time", 0},
                {"dp_group_peers", {}},
                {"dynamic_info", {
                    {"avail_block_num", 37028},
                    {"avail_slots_num", 200}
                }},
                {"host_id", "192.168.0.0"},
                {"inference_type", 0},
                {"inter_comm_port", "1026"},
                {"is_distribute", false},
                {"is_faulty", false},
                {"is_healthy", true},
                {"is_initialized", true},
                {"metric_port", "1027"},
                {"mgmt_port", "1026"},
                {"port", "1025"},
                {"role_state", "RoleReady"},
                {"server_info_list", {
                    {
                        {"hostId", "172.16.0.4"},
                        {"ip", "172.16.0.5"},
                        {"is_master", true},
                        {"sp_size", 1},
                        {"cp_size", 1},
                        {"device_infos", {
                            {
                                {"id", "0"},
                                {"ip", "172.16.0.3"},
                                {"logical_id", "0"},
                                {"rank_id", 0}
                            }
                        }},
                        {"dp_group_infos", {
                            {
                                {"id", 9691240320000},
                                {"device_infos", {
                                    {
                                        {"id", "0"},
                                        {"ip", "172.16.0.3"},
                                        {"logical_id", "0"},
                                        {"rank_id", 0}
                                    }
                                }}
                            }
                        }}
                    }
                }}
            }
        }
        },
        {
            "instance_start_id_number", 0
        }
    };

    std::vector<std::unique_ptr<NodeInfo>> servers;
    auto nodeInfo1 = std::make_unique<NodeInfo>();
    nodeInfo1->ip = "172.16.0.1";
    nodeInfo1->mgmtPort = "1031";
    nodeInfo1->instanceInfo.staticInfo.id = 1;
    nodeInfo1->instanceInfo.staticInfo.groupId = 1;
    nodeInfo1->instanceInfo.staticInfo.virtualId = 1;
    uint64_t id1 = 0;
    AllocateNodeId(nodeInfo1, id1);
    servers.emplace_back(std::move(nodeInfo1));

    auto nodeInfo2 = std::make_unique<NodeInfo>();
    nodeInfo2->ip = "172.16.0.2";
    nodeInfo2->mgmtPort = "1026";
    nodeInfo2->instanceInfo.staticInfo.id = 2; // 2：重启后分配的id
    nodeInfo2->instanceInfo.staticInfo.groupId = 1;
    nodeInfo2->instanceInfo.staticInfo.virtualId = 2; // 2：重启后分配的id
    uint64_t id2 = 1;
    AllocateNodeId(nodeInfo2, id2);
    servers.emplace_back(std::move(nodeInfo2));

    std::vector<std::unique_ptr<NodeInfo>> availableServers;
    std::map<uint64_t, std::pair<std::vector<uint64_t>, std::vector<uint64_t>>> groups;
    std::map<uint64_t, std::vector<uint64_t>> flexGroup;
    int32_t status =
        ProcessManager::GetInstance()->GetNodeInfoFromPath(statusJson, servers, availableServers, groups, flexGroup);

    ASSERT_EQ(availableServers.size(), 1);
    EXPECT_EQ(availableServers[0]->peers.size(), 1);
    EXPECT_EQ(availableServers[0]->instanceInfo.staticInfo.id, 33558956); // 33558956: nodestatus文件中保存的原始ID
    EXPECT_EQ(availableServers[0]->peers[0], 16781740); // 16781740： peers的ID
    EXPECT_EQ(availableServers[0]->serverInfoList.size(), 1);
}