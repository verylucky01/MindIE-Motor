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

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include "StatusHandler.h"

using namespace MINDIE::MS;

class TestStatusHandler : public testing::Test {
};

/*
测试描述: 状态处理测试用例
测试步骤:
    1. 配置mountMap为空，预期结果1
预期结果:
    1. podSpec和containers中mount为空
*/
TEST(TestStatusHandler, UTGetStatus)
{
    StatusHandler statusHandler;
    nlohmann::json statusJson = {
        {"server_list", {{{"server_name", "server1"}}, {{"server_name", "server0"}}, {{"server_name", "server3"}}}}};
    const char *mStatusFile = "status.json";
    std::ofstream file(mStatusFile);
    file << statusJson.dump(4);
    file.close();
    chmod(mStatusFile, 0640);
    {
        statusHandler.SetStatusFile(mStatusFile);
        EXPECT_EQ(statusHandler.RemoveServerStatus("server1"), 0);
    }
    {
        statusHandler.SetStatusFile("nonexistent.json");
        EXPECT_EQ(statusHandler.RemoveServerStatus("server1"), -1);
    }
}

/*
测试描述: RemoveServerStatus测试
测试步骤:
    1. 创建一个测试用的StatusHandler对象
    2. 创建一个测试用的JSON对象
    3. 将测试用的JSON对象写入文件
    4. 设置StatusHandler对象的状态文件路径
    5. 测试RemoveServerStatus函数
    6. 验证状态文件中的内容
预期结果:
    1. 文件内容和预期一致
*/
TEST(TestStatusHandler, RemoveServerStatusTest)
{
    // 创建一个测试用的StatusHandler对象
    StatusHandler handler;

    // 创建一个测试用的JSON对象
    nlohmann::json statusJson = {
        {"server_list", {{{"server_name", "server1"}}, {{"server_name", "server2"}}, {{"server_name", "server3"}}}}};

    // 将测试用的JSON对象写入文件
    const char* statusFile = "test_status.json";
    std::ofstream file(statusFile);
    file << statusJson.dump(4);
    file.close();
    chmod(statusFile, 0640);

    // 设置StatusHandler对象的状态文件路径
    handler.SetStatusFile(statusFile);

    // 测试RemoveServerStatus函数
    int32_t ret = handler.RemoveServerStatus("server2");
    EXPECT_EQ(ret, 0);

    // 验证状态文件中的内容
    nlohmann::json newStatusJson;
    std::ifstream newFile(statusFile);
    newFile >> newStatusJson;
    EXPECT_EQ(newStatusJson["server_list"].size(), 2);
    EXPECT_EQ(newStatusJson["server_list"][0]["server_name"], "server1");
    EXPECT_EQ(newStatusJson["server_list"][1]["server_name"], "server3");
}

/*
测试描述: SaveStatusToFile测试，json文件中没有server_list
测试步骤:
    1. 创建一个测试用的StatusHandler对象
    2. 创建一个测试用的JSON对象
    3. 将测试用的JSON对象写入文件
    4. 设置StatusHandler对象的状态文件路径
    5. 测试SaveStatusToFile函数，预期结果1
预期结果:
    1. 运行失败
*/
TEST(TestStatusHandler, SaveStatusToFile_GetStatusFromPath_Failure) {
    StatusHandler handler;
    ServerSaveStatus status = {
        .replicas = 3,
        .nameSpace = "default",
        .serverName = "server1",
        .serverType = "type1",
        .useService = true
    };

    nlohmann::json statusJson;
    std::string statusFile = "test_status.json";
    std::ofstream file(statusFile);
    file << statusJson.dump(4);
    file.close();
    handler.SetStatusFile(statusFile);

    int32_t result = handler.SaveStatusToFile(status);
    EXPECT_EQ(result, -1);
}

/*
测试描述: SaveStatusToFile测试，json文件中没有server_name
测试步骤:
    1. 创建一个测试用的StatusHandler对象
    2. 创建一个测试用的JSON对象
    3. 将测试用的JSON对象写入文件
    4. 设置StatusHandler对象的状态文件路径
    5. 测试SaveStatusToFile函数，预期结果1
预期结果:
    1. 运行失败
*/
TEST(TestStatusHandler, SaveStatusToFile_MissingServerName) {
    StatusHandler handler;
    ServerSaveStatus status = {
        .replicas = 3,
        .nameSpace = "default",
        .serverName = "server1",
        .serverType = "type1",
        .useService = true
    };
    nlohmann::json statusJson = {
        {"server_list", {{{"server", "server0"}}}}};

    std::string statusFile = "test_status.json";
    std::ofstream file(statusFile);
    file << statusJson.dump(4);
    file.close();
    handler.SetStatusFile(statusFile);
    // 模拟一个缺少 server_name 的 JSON 文件
    int32_t result = handler.SaveStatusToFile(status);
    EXPECT_EQ(result, -1);
}

/*
测试描述: SaveStatusToFile测试，json文件中已有相同的server_name
测试步骤:
    1. 创建一个测试用的StatusHandler对象
    2. 创建一个测试用的JSON对象
    3. 将测试用的JSON对象写入文件
    4. 设置StatusHandler对象的状态文件路径
    5. 测试SaveStatusToFile函数，预期结果1
预期结果:
    1. 运行失败
*/
TEST(TestStatusHandler, SaveStatusToFile_DuplicateServerName) {
    StatusHandler handler;
    ServerSaveStatus status = {
        .replicas = 3,
        .nameSpace = "default",
        .serverName = "server1",
        .serverType = "type1",
        .useService = true
    };
    nlohmann::json statusJson = {
        {"server_list", {{{"server_name", "server1"}}, {{"server_name", "server2"}}, {{"server_name", "server3"}}}}};

    std::string statusFile = "test_status.json";
    std::ofstream file(statusFile);
    file << statusJson.dump(4);
    file.close();
    handler.SetStatusFile(statusFile);

    // 一个已经包含相同 server_name 的 JSON 文件
    int32_t result = handler.SaveStatusToFile(status);
    EXPECT_EQ(result, 0);
}
