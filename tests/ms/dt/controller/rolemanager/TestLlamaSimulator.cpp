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
#include <map>
#include <memory>
#include <string>
#include "LlamaSimulator.h"
#include "parse_json.h"


class TestLlamaSimulator : public testing::Test {
    static constexpr int inputLength = 2048;
    static constexpr int outLength = 512;

protected:
    void SetUp() override
    {
        InitValidConfig(config_);
        CreateSimulator(simulator_);
    }

    void InitValidConfig(std::map<std::string, std::string>& config)
    {
        config["model_params"] = R"({
            "hidden_size": 8192,
            "intermediate_size": 28672,
            "num_attention_heads": 64,
            "num_hidden_layers": 80,
            "num_key_value_heads": 8,
            "torch_dtype": "float16"
        })";

        config["machine_params"] = R"({
            "BW_GB": 392,
            "BWeff": 0.5,
            "BW_RDMA_Gb": 200,
            "TFLOPS": 295,
            "TFLOPSeff": 0.5,
            "MBW_TB": 1.6,
            "MBW_TBeff": 0.3,
            "alpha": 2,
            "MEMCapacity": 64,
            "eta_OOM": 0.9,
            "staticTransferDelay": 0.00001
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
        config["hardware_card_nums"] = "8";
    }

    void CreateSimulator(std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator>& simulator)
    {
        auto status = MINDIE::MS::simulation::LlamaSimulator::Create(config_, simulator);
        ASSERT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
        ASSERT_NE(simulator, nullptr);
    }

protected:
    std::map<std::string, std::string> config_;
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> simulator_;
};

TEST_F(TestLlamaSimulator, ValidConfiguration)
{
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> simulator;
    auto status = MINDIE::MS::simulation::LlamaSimulator::Create(config_, simulator);
    EXPECT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
    EXPECT_NE(simulator, nullptr);
}

TEST_F(TestLlamaSimulator, MissingModelParams)
{
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> simulator;
    config_.erase("model_params");
    auto status = MINDIE::MS::simulation::LlamaSimulator::Create(config_, simulator);
    EXPECT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

TEST_F(TestLlamaSimulator, InvalidJsonModelParams)
{
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> simulator;
    config_["model_params"] = "{invalid_json";
    auto status = MINDIE::MS::simulation::LlamaSimulator::Create(config_, simulator);
    EXPECT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

TEST_F(TestLlamaSimulator, MissingMachineParams)
{
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> simulator;
    config_.erase("machine_params");
    auto status = MINDIE::MS::simulation::LlamaSimulator::Create(config_, simulator);
    EXPECT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

TEST_F(TestLlamaSimulator, MissingRequiredModelKey)
{
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> simulator;
    auto modelJson = Json::parse(config_["model_params"]);
    modelJson.erase("hidden_size");
    config_["model_params"] = modelJson.dump();

    auto status = MINDIE::MS::simulation::LlamaSimulator::Create(config_, simulator);
    EXPECT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

TEST_F(TestLlamaSimulator, MissingRequiredHardwareKey)
{
    std::unique_ptr<MINDIE::MS::simulation::LlamaSimulator> simulator;
    auto hwJson = Json::parse(config_["machine_params"]);
    hwJson.erase("TFLOPS");
    config_["machine_params"] = hwJson.dump();

    auto status = MINDIE::MS::simulation::LlamaSimulator::Create(config_, simulator);
    EXPECT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

TEST_F(TestLlamaSimulator, CalAbility_Valid)
{
    MINDIE::MS::DIGSRequestSummary summary;
    summary.inputLength = inputLength;
    summary.outputLength = outLength;

    MINDIE::MS::DIGSGroupPDRatio ratio;
    simulator_->CalAbility(summary, ratio);

    EXPECT_GT(ratio.pAbility, 0);
    EXPECT_GT(ratio.dAbility, 0);
}

TEST_F(TestLlamaSimulator, CalTransferTime_Valid)
{
    auto tTime = simulator_->CalTransferTime(inputLength);

    EXPECT_GT(tTime, 0.0);
}

TEST_F(TestLlamaSimulator, CalTransferTime_InValid)
{
    auto tTime = simulator_->CalTransferTime(0.0);

    EXPECT_EQ(tTime, 0.0);
}

TEST_F(TestLlamaSimulator, CalTransferAbility_Valid)
{
    auto tTime = simulator_->CalTransferAbility(inputLength);

    EXPECT_GT(tTime, 0.0);
}

TEST_F(TestLlamaSimulator, CalTransferAbility_InValid)
{
    auto tTime = simulator_->CalTransferAbility(0.0);

    EXPECT_EQ(tTime, 0.0);
}