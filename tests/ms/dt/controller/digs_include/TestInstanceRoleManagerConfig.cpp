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
#include <cstdlib>
#include <string>
#include <memory>
#include <pthread.h>
#include <thread>
#include <mutex>
#include "gtest/gtest.h"
#include "InstanceRoleManager.h"

class TestInstanceRoleManagerConfig : public testing::Test {
protected:
    void SetUp() override
    {
        InitAloConfig(config_);
    }

    void InitAloConfig(MINDIE::MS::roleManager::InstanceRoleManager::Config &config)
    {
        config["model_params"] = R"({
        "hidden_size": "8192",
        "initializer_range": "0.02",
        "intermediate_size": "28672",
        "max_position_embeddings": "4096",
        "num_attention_heads": "64",
        "num_hidden_layers": "80",
        "num_key_value_heads": "8",
        "torch_dtype": "float16"
        })";
        config["machine_params"] = R"({
        "BW_GB": "392",
        "BWeff": "0.5",
        "BW_RDMA_Gb": "200",
        "BW_RDMAeff": "0.5",
        "TFLOPS": "295",
        "TFLOPSeff": "0.5",
        "MBW_TB": "1.6",
        "MBW_TBeff": "0.3",
        "alpha": "2",
        "MEMCapacity": "64",
        "eta_OOM": 0.9,
        "staticTransferDelay": "0.00001"
        })";
        config["model_type"] = "llama2-70B";
        config["transfer_type"] = "D2DTransfer";
        config["prefill_slo"] = "1000";
        config["decode_slo"] = "50";
        config["time_period"] = "3";
        config["is_heterogeneous"] = "false";
        config["pp"] = "1";
        config["tp"] = "8";
    }

    void MockConfigForWrongModelParamJson(MINDIE::MS::roleManager::InstanceRoleManager::Config &config)
    {
        config["model_params"] = R"({
        "hidden_size": "8192",
        "initializer_range": "0.02",
        "intermediate_size": "28672",
        "max_position_embeddings": "4096",
        "num_attention_heads": "64",
        "num_hidden_layers": "80"xxx,
        "num_key_value_heads": "8",
        "torch_dtype": "float16"
        })";
    }

    void MockConfigForWrongModelParamNumStr(MINDIE::MS::roleManager::InstanceRoleManager::Config &config)
    {
        config["model_params"] = R"({
        "hidden_size": "xxx8192",
        "initializer_range": "0.02",
        "intermediate_size": "28672",
        "max_position_embeddings": "4096",
        "num_attention_heads": "64",
        "num_hidden_layers": "80",
        "num_key_value_heads": "8",
        "torch_dtype": "float16"
        })";
    }

    void MockConfigForWrongMachineParamJson(MINDIE::MS::roleManager::InstanceRoleManager::Config &config)
    {
        config["machine_params"] = R"({
        "BW_GB": "392"xxxx,
        "BWeff": "0.5",
        "BW_RDMA_Gb": "200",
        "BW_RDMAeff": "0.5",
        "TFLOPS": "295",
        "TFLOPSeff": "0.5",
        "MBW_TB": "1.6",
        "MBW_TBeff": "0.3",
        "alpha": "2",
        "MEMCapacity": "64",
        "eta_OOM": 0.9,
        "staticTransferDelay": "0.00001"
        })";
    }

    void MockConfigForWrongMachineParamNumStr(MINDIE::MS::roleManager::InstanceRoleManager::Config &config)
    {
        config["machine_params"] = R"({
        "BW_GB": "xxx392",
        "BWeff": "0.5",
        "BW_RDMA_Gb": "200",
        "BW_RDMAeff": "0.5",
        "TFLOPS": "295",
        "TFLOPSeff": "0.5",
        "MBW_TB": "1.6",
        "MBW_TBeff": "0.3",
        "alpha": "2",
        "MEMCapacity": "64",
        "eta_OOM": 0.9,
        "staticTransferDelay": "0.00001"
        })";
    }

    static int32_t NotifyRoleDecision(std::vector <MINDIE::MS::DIGSRoleDecision> decisions)
    {
        for (auto decision: decisions) {
            std::cout << "DIGSRoleDecision: ID:" << decision.id << " groupId:"
                      << decision.groupId << std::endl;
        }
        return 0;
    }

    // mock
    static int32_t InstancesCollector(
        std::vector <MINDIE::MS::DIGSInstanceInfo> &info,
        MINDIE::MS::DIGSRequestSummary &sum)
    {
        return 0;
    }

protected:
    std::unique_ptr<MINDIE::MS::roleManager::InstanceRoleManager> manager_;
    MINDIE::MS::roleManager::InstanceRoleManager::Config config_;
};

TEST_F(TestInstanceRoleManagerConfig, TestConfig01)
{
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager_);
    EXPECT_EQ(status == 0, true);
}

TEST_F(TestInstanceRoleManagerConfig, TestConfig02)
{
    MockConfigForWrongModelParamJson(config_);
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager_);
    EXPECT_EQ(status != 0, true);
}

TEST_F(TestInstanceRoleManagerConfig, TestConfig03)
{
    MockConfigForWrongModelParamNumStr(config_);
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager_);
    EXPECT_EQ(status != 0, true);
}

TEST_F(TestInstanceRoleManagerConfig, TestConfig04)
{
    MockConfigForWrongMachineParamJson(config_);
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager_);
    EXPECT_EQ(status != 0, true);
}

TEST_F(TestInstanceRoleManagerConfig, TestConfig05)
{
    MockConfigForWrongMachineParamNumStr(config_);
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager_);
    EXPECT_EQ(status != 0, true);
}

TEST_F(TestInstanceRoleManagerConfig, TestConfig06)
{
    config_["prefill_slo"] = "xxx1000";
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager_);
    EXPECT_EQ(status == 0, true);
    config_["time_period"] = "xxx1000";
    config_["prefill_slo"] = "1000";
    status = MINDIE::MS::roleManager::InstanceRoleManager::Create(
        config_,
        std::bind(NotifyRoleDecision, std::placeholders::_1),
        std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2),
        manager_);
    EXPECT_EQ(status == 0, true);
    config_["time_period"] = "1000";
    config_["pp"] = "xx1";
    status = MINDIE::MS::roleManager::InstanceRoleManager::Create(
        config_,
        std::bind(NotifyRoleDecision, std::placeholders::_1),
        std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2),
        manager_);
    EXPECT_EQ(status == 0, true);
}
