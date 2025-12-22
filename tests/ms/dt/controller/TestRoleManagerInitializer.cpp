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
#include <cstdlib>
#include <string>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <cerrno>
#include "gtest/gtest.h"
#include "Helper.h"
#include "RoleManagerInitializer.h"
#include "JsonFileLoader.h"
#include "ControllerConfig.h"
using namespace MINDIE::MS;
class TestRoleManagerInitializer : public testing::Test {
protected:
    void SetUp()
    {
        CopyDefaultConfig();
        auto controllerJson = GetMSControllerTestJsonPath();
        ModifyJsonItem(controllerJson, "digs_model_config_path", "", GetModelConfigTestJsonPath());
        ModifyJsonItem(controllerJson, "digs_machine_config_path", "", GetMachineConfigTestJsonPath());
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
        ControllerConfig::GetInstance()->Init();
    }
};

/*
测试描述: 读取RoleManager需要的机器参数配置，配置为string，反例。
测试步骤:
    1. 必填参数不存在，预期结果1
    2. 必填参数为负数，预期结果1
    3. 效率相关参数大于1，预期结果1
预期结果:
    1. 配置内容为空
*/
TEST_F(TestRoleManagerInitializer, TestInvalidMachineParamString)
{
    auto machinePath = GetMachineConfigTestJsonPath();
    std::map<std::string, std::string> config;
    std::vector<std::string> keys {"BW_GB", "BW_RDMA_Gb", "TFLOPS", "MBW_TB", "alpha", "MEMCapacity",
        "staticTransferDelay", "BWeff", "TFLOPSeff", "MBW_TBeff", "eta_OOM"};

    // case 1 参数不存在
    std::cout << "case 1" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetMachineConfigJsonPath(), machinePath);
        auto rawConfig = FileToJsonObj(GetMachineConfigJsonPath(), 0777); // 读取权限0777
        rawConfig.erase(key);
        DumpStringToFile(machinePath, rawConfig.dump(4)); // 缩进为4
        config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 2 必填参数为负数
    std::cout << "case 2" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetMachineConfigJsonPath(), machinePath);
        ModifyJsonItem(machinePath, key, "", "-1");
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 3 效率相关参数大于1
    std::cout << "case 3" << std::endl;
    std::vector<std::string> effKeys {"BWeff", "TFLOPSeff", "MBW_TBeff", "eta_OOM"};
    for (auto &key : std::as_const(effKeys)) {
        CopyFile(GetMachineConfigJsonPath(), machinePath);
        ModifyJsonItem(machinePath, key, "", "1.1");
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 4 必填参数超大
    std::cout << "case 4" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetMachineConfigJsonPath(), machinePath);
        ModifyJsonItem(machinePath, key, "", "2147483647.1");
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 5 必填参数非常小
    std::cout << "case 5" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetMachineConfigJsonPath(), machinePath);
        ModifyJsonItem(machinePath, key, "", "1e-7"); // 1e-7为无效值
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }
}

/*
测试描述: 读取RoleManager需要的机器参数配置，配置为double，反例。
测试步骤:
    1. 必填参数不存在，预期结果1
    2. 必填参数为负数，预期结果1
    3. 效率相关参数大于1，预期结果1
预期结果:
    1. 配置内容为空
*/
TEST_F(TestRoleManagerInitializer, TestInvalidMachineParamDouble)
{
    nlohmann::json machineParamJson = {
        {"BW_GB", 392},
        {"BW_RDMA_Gb", 200},
        {"BWeff", 0.5},
        {"BW_RDMAeff", 0.5},
        {"TFLOPS", 295},
        {"TFLOPSeff", 0.5},
        {"MBW_TB", 1.6},
        {"MBW_TBeff", 0.3},
        {"alpha", 2},
        {"MEMCapacity", 64},
        {"eta_OOM", 0.9},
        {"staticTransferDelay", 0.00001}
    };
    auto machinePath = GetMachineConfigTestJsonPath();
    std::map<std::string, std::string> config;
    std::vector<std::string> keys {"BW_GB", "BW_RDMA_Gb", "TFLOPS", "MBW_TB", "alpha", "MEMCapacity",
        "staticTransferDelay", "BWeff", "TFLOPSeff", "MBW_TBeff", "eta_OOM"};

    // case 1 参数不存在
    std::cout << "case 1" << std::endl;
    for (auto &key : std::as_const(keys)) {
        DumpStringToFile(machinePath, machineParamJson.dump(4)); // 缩进为4
        auto rawConfig = FileToJsonObj(machinePath, 0777); // 读取权限0777
        rawConfig.erase(key);
        DumpStringToFile(machinePath, rawConfig.dump(4)); // 缩进为4
        config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 2 必填参数为负数
    std::cout << "case 2" << std::endl;
    for (auto &key : std::as_const(keys)) {
        DumpStringToFile(machinePath, machineParamJson.dump(4)); // 缩进为4
        ModifyJsonItem(machinePath, key, "", -1);
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 3 效率相关参数大于1
    std::cout << "case 3" << std::endl;
    std::vector<std::string> effKeys {"BWeff", "TFLOPSeff", "MBW_TBeff", "eta_OOM"};
    for (auto &key : std::as_const(effKeys)) {
        DumpStringToFile(machinePath, machineParamJson.dump(4)); // 缩进为4
        ModifyJsonItem(machinePath, key, "", 1.1); // 非法相关参数1.1
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 4 必填参数超大
    std::cout << "case 4" << std::endl;
    for (auto &key : std::as_const(keys)) {
        DumpStringToFile(machinePath, machineParamJson.dump(4)); // 缩进为4
        ModifyJsonItem(machinePath, key, "", 2147483647.1); // 2147483647.1为非法超大值
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 5 必填参数非常小
    std::cout << "case 5" << std::endl;
    for (auto &key : std::as_const(keys)) {
        DumpStringToFile(machinePath, machineParamJson.dump(4)); // 缩进为4
        ModifyJsonItem(machinePath, key, "", 1e-7); // 1e-7为无效值
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }
}
/*
测试描述: 读取RoleManager需要的模型参数配置，配置为string，反例。
测试步骤:
    1. 必填参数不存在，预期结果1
    2. 必填参数为负数，预期结果1
    3. 效率相关参数大于1，预期结果1
预期结果:
    1. 配置内容为空
*/
TEST_F(TestRoleManagerInitializer, TestInvalidModelParamString)
{
    auto modelPath = GetModelConfigTestJsonPath();
    std::map<std::string, std::string> config;
    std::vector<std::string> keys {"hidden_size", "initializer_range", "intermediate_size", "max_position_embeddings",
        "num_attention_heads", "num_hidden_layers", "num_key_value_heads"};
    // case 1 参数不存在
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetModelConfigJsonPath(), modelPath);
        auto rawConfig = FileToJsonObj(modelPath, 0777); // 读取权限0777
        rawConfig.erase(key);
        DumpStringToFile(modelPath, rawConfig.dump(4)); // 缩进为4
        config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 2 必填参数为负数
    std::cout << "case 2" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetModelConfigJsonPath(), modelPath);
        ModifyJsonItem(modelPath, key, "", "-1");
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 3 必填参数超大
    std::cout << "case 3" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetModelConfigJsonPath(), modelPath);
        ModifyJsonItem(modelPath, key, "", "2147483647.1");
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 4 必填参数非常小
    std::cout << "case 4" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetModelConfigJsonPath(), modelPath);
        ModifyJsonItem(modelPath, key, "", "1e-7");
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }
}

/*
测试描述: 读取RoleManager需要的模型参数配置，配置为double，反例。
测试步骤:
    1. 必填参数不存在，预期结果1
    2. 必填参数为负数，预期结果1
    3. 效率相关参数大于1，预期结果1
预期结果:
    1. 配置内容为空
*/
TEST_F(TestRoleManagerInitializer, TestInvalidModelParamDouble)
{
    nlohmann::json modelParamJson = {
        {"hidden_size", 8192},
        {"initializer_range", 0.02},
        {"intermediate_size", 22016},
        {"max_position_embeddings", 128},
        {"num_attention_heads", 128},
        {"num_hidden_layers", 80},
        {"num_key_value_heads", 128},
        {"torch_dtype", "float16"}
    };
    auto modelPath = GetModelConfigTestJsonPath();
    std::map<std::string, std::string> config;
    std::vector<std::string> keys {"hidden_size", "initializer_range", "intermediate_size", "max_position_embeddings",
        "num_attention_heads", "num_hidden_layers", "num_key_value_heads"};
    // case 1 参数不存在
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetModelConfigJsonPath(), modelPath);
        auto rawConfig = FileToJsonObj(modelPath, 0777); // 读取权限0777
        rawConfig.erase(key);
        DumpStringToFile(modelPath, rawConfig.dump(4)); // 缩进为4
        config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 2 必填参数为负数
    std::cout << "case 2" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetModelConfigJsonPath(), modelPath);
        ModifyJsonItem(modelPath, key, "", -1); // -1为非法负数
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 3 必填参数超大
    std::cout << "case 3" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetModelConfigJsonPath(), modelPath);
        ModifyJsonItem(modelPath, key, "", 2147483647.1); // 2147483647.1为非法超大值
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }

    // case 4 必填参数非常小
    std::cout << "case 4" << std::endl;
    for (auto &key : std::as_const(keys)) {
        CopyFile(GetModelConfigJsonPath(), modelPath);
        ModifyJsonItem(modelPath, key, "", 1e-7); // 1e-7为无效值
        auto config = BuildInstanceRoleManagerConfig(1);
        EXPECT_TRUE(config.empty());
    }
}