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
#include "gtest/gtest.h"
#include "InstanceRoleManager.h"

class TestRMInstanceRoleManager : public testing::Test {
protected:
    void SetUp() override
    {
        InitAloConfig(config_);
    }

    void TearDown() override
    {
    }

    void InitAloConfig(std::map<std::string, std::string> &config)
    {
        config["model_params"] = R"({
        "hidden_size": "8192",
        "intermediate_size": "28672",
        "num_attention_heads": "64",
        "num_hidden_layers": "80",
        "num_key_value_heads": "8",
        "torch_dtype": "float16"
        })";
        config["machine_params"] = R"({
        "BW_GB": "392",
        "BWeff": "0.5",
        "BW_RDMA_Gb": "200",
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
        config["time_period"] = "1";
        config["is_heterogeneous"] = "false";
        config["is_auto_pd_role_switching"] = "true";
        config["pp"] = "1";
        config["tp"] = "8";
    }

    static int32_t NotifyRoleDecision(std::vector <MINDIE::MS::DIGSRoleDecision> decisions)
    {
        return 0;
    }

    static int32_t NotifyRoleDecisionReturnFalse(std::vector <MINDIE::MS::DIGSRoleDecision> decisions)
    {
        return -1;
    }

    // mock
    static int32_t InstancesCollector(
        std::vector <MINDIE::MS::DIGSInstanceInfo> &info,
        MINDIE::MS::DIGSRequestSummary &sum)
    {
        return 0;
    }

protected:
    std::map<std::string, std::string> config_;
};

TEST_F(TestRMInstanceRoleManager, TestCreateNullFunc)
{
    std::unique_ptr<MINDIE::MS::roleManager::InstanceRoleManager> manager;
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, nullptr, nullptr, manager);
    EXPECT_EQ(status, -1);

    status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), nullptr, manager);
    EXPECT_EQ(status, -1);

    config_["is_auto_pd_role_switching"] = "false";
    status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager);
    EXPECT_EQ(status, 0);
}

TEST_F(TestRMInstanceRoleManager, TestInit)
{
    std::unique_ptr<MINDIE::MS::roleManager::InstanceRoleManager> manager;
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecision,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager);
    EXPECT_EQ(status, 0);
    std::vector<MINDIE::MS::DIGSInstanceInfo> infos;
    MINDIE::MS::DIGSRequestSummary summary;
    summary.inputLength = 0;
    size_t pRate = 0;
    size_t dRate = 0;
    status = manager->Init(infos, summary, pRate, dRate);
    EXPECT_EQ(status, false);
}

TEST_F(TestRMInstanceRoleManager, TestInitReturnDecisionFalse)
{
    size_t inputLength = 1000;
    size_t outputLength = 1000;
    std::unique_ptr<MINDIE::MS::roleManager::InstanceRoleManager> manager;
    auto status = MINDIE::MS::roleManager::InstanceRoleManager::Create(config_, std::bind(NotifyRoleDecisionReturnFalse,
        std::placeholders::_1), std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager);
    EXPECT_EQ(status, 0);
    std::vector<MINDIE::MS::DIGSInstanceInfo> infos;
    MINDIE::MS::DIGSRequestSummary summary;
    summary.inputLength = inputLength;
    summary.outputLength = outputLength;
    size_t pRate = 2;
    size_t dRate = 3;
    status = manager->Init(infos, summary, pRate, dRate);
    EXPECT_EQ(status, true);
}
