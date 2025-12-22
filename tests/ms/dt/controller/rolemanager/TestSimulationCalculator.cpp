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
#include "SimulationCalculator.h"
#include "LlamaSimulator.h"

class TestSimulationCalculator : public testing::Test {
    static constexpr int inputLength = 2048;
    static constexpr int outLength = 512;

protected:
    MINDIE::MS::DIGSGroupPDRatio ratio;
    MINDIE::MS::simulation::SimulationCalculator calculator;
    void SetUp() override
    {
        ratio.pAbility = 10;   // 10: pAbility 初始
        ratio.dAbility = 6;    // 6: dAbility 初始
        ratio.tAbility = 9;    // 9: tAbility 初始
        ratio.pNum = 0;        // 0: pNum 初始
        ratio.dNum = 0;        // 0: dNum 初始
        ratio.flexPRatio = 0;  // 0: flexPRatio 初始
        InitAloConfig(config_);
    }

    void TearDown() override {}

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
        config["time_period"] = "3";
        config["is_heterogeneous"] = "false";
        config["pp"] = "1";
        config["tp"] = "8";
    }

protected:
    std::map<std::string, std::string> config_;
};

TEST_F(TestSimulationCalculator, TestCreateSimulator)
{
    config_.erase("model_type");
    auto status = calculator.Create(config_);
    EXPECT_EQ(status, 0);

    config_["model_type"] = "pangu";
    config_["model_params"] = R"({
        "hidden_size": "8192",
        "intermediate_size": "28672",
        "num_attention_heads": "64",
        "num_hidden_layers": "80",
        "num_key_value_heads": "8",
        })";
    status = calculator.Create(config_);
    EXPECT_EQ(status, -1);

    config_.erase("model_type");
    status = calculator.Create(config_);
    EXPECT_EQ(status, -1);

    config_["model_type"] = "llama2-70B";
    config_["model_params"] = R"({
        "hidden_size": "8192",
        "intermediate_size": "28672",
        "num_attention_heads": "64",
        "num_hidden_layers": "80",
        "num_key_value_heads": "8",
        "torch_dtype": "float16"
        })";
    status = calculator.Create(config_);
    EXPECT_EQ(status, 0);
}