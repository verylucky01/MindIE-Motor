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
#include "ProportionCalculator.h"
#include "LlamaSimulator.h"

class TestRMProportionCalculator : public testing::Test {
protected:
    MINDIE::MS::DIGSGroupPDRatio ratio;
    MINDIE::MS::roleManager::ProportionCalculator calculator;
    void SetUp() override
    {
        ratio.pAbility = 10;  // 10: pAbility 初始
        ratio.dAbility = 6;   // 6: dAbility 初始
        ratio.tAbility = 9;   // 9: tAbility 初始
        ratio.pNum = 0;       // 0: pNum 初始
        ratio.dNum = 0;       // 0: dNum 初始
        ratio.flexPRatio = 0;  // 0: flexPRatio 初始
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
        config["time_period"] = "3";
        config["is_heterogeneous"] = "false";
        config["pp"] = "1";
        config["tp"] = "8";
    }

protected:
    std::map<std::string, std::string> config_;
};

TEST_F(TestRMProportionCalculator, TestCreateModelParams)
{
    std::unique_ptr<MINDIE::MS::roleManager::ProportionCalculator> proportionCalculator;
    config_["model_type"] = "pangu";
    auto status = MINDIE::MS::roleManager::ProportionCalculator::Create(config_, proportionCalculator);
    EXPECT_EQ(status, 0);

    config_["model_params"] = R"({
        "hidden_size": "xx8192",
        "intermediate_size": "28672",
        "num_attention_heads": "64",
        "num_hidden_layers": "80",
        "num_key_value_heads": "8",
        "torch_dtype": "float16"
        })";
    status = MINDIE::MS::roleManager::ProportionCalculator::Create(config_, proportionCalculator);
    EXPECT_EQ(status, -1);

    config_.erase("model_type");
    status = MINDIE::MS::roleManager::ProportionCalculator::Create(config_, proportionCalculator);
    EXPECT_EQ(status, -1);

    config_["model_params"] = R"({
        "hidden_size": "8192",
        "intermediate_size": "28672",
        "num_attention_heads": "64",
        "num_hidden_layers": "80",
        "num_key_value_heads": "8",
        "torch_dtype": "float16"
        })";
    status = MINDIE::MS::roleManager::ProportionCalculator::Create(config_, proportionCalculator);
    EXPECT_EQ(status, 0);
}

TEST_F(TestRMProportionCalculator, TestCreateHardwareParams)
{
    std::unique_ptr<MINDIE::MS::roleManager::ProportionCalculator> proportionCalculator;
    config_["machine_params"] = R"({
        "BW_GB": "xx392",
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
    auto status = MINDIE::MS::roleManager::ProportionCalculator::Create(config_, proportionCalculator);
    EXPECT_EQ(status, -1);
}

// 测试CalPdflexNum函数，检查计算后的pNum和dNum是否正确
TEST_F(TestRMProportionCalculator, TestCalPdflexNumNormalCase)
{
    uint64_t instanceNum = 16;  // 16：instanceNum 初始
    uint64_t flexNum = 1;        // 1： flexNum 初始

    auto status = calculator.CalPdflexNum(instanceNum, flexNum, ratio);

    EXPECT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::OK));

    // 在当前 ability 条件下，理论上 pNum = 6、dNum = 9、flexPRatio = 0.375 为最佳参数。
    // 验证实际计算结果与预期结果是否匹配。
    EXPECT_EQ(ratio.pNum, 6);  // 验证pNum = 6
    EXPECT_GE(ratio.dNum, 9);  // 验证dNum = 9
    EXPECT_EQ(ratio.flexNum, flexNum);
    EXPECT_NEAR(ratio.flexPRatio, 0.375, 1e-4);  // 0.375 1e-4 ：验证结果是否为最佳比
}

// 测试CalPdflexNum函数，处理速度异常的情况（小于等于0），返回非法参数错误
TEST_F(TestRMProportionCalculator, TestCalPdflexNumZeroAbility)
{
    ratio.dAbility = 0;         // 0: dAbility 初始
    uint64_t instanceNum = 16;  // 16: instanceNum 初始
    uint64_t flexNum = 1;        // 1: flexNum 初始

    auto status_1 = calculator.CalPdflexNum(instanceNum, flexNum, ratio);
    EXPECT_EQ(status_1, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));

    ratio.pAbility = -1;  // pAbility 初始为-1
    auto status_2 = calculator.CalPdflexNum(instanceNum, flexNum, ratio);
    EXPECT_EQ(status_2, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

// 测试CalPdflexNum函数，当dAbility极小的情况，dNum是否会小于0
TEST_F(TestRMProportionCalculator, TestCalPdflexNumdNegative)
{
    ratio.dAbility = 0.01;      // 0.01: dAbility 初始
    uint64_t instanceNum = 16;  // 16: instanceNum 初始
    uint64_t flexNum = 1;        // 1: flexNum 初始

    auto status = calculator.CalPdflexNum(instanceNum, flexNum, ratio);
    EXPECT_EQ(status, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
    EXPECT_GE(ratio.dNum, 0);  // 判断 dNum 为0
    EXPECT_EQ(instanceNum, ratio.pNum + ratio.dNum + flexNum);
}

// 测试CalflexPRatioX函数，检查计算后的x是否正确
TEST_F(TestRMProportionCalculator, TestCalFlexPRatioXNormalCase)
{
    ratio.pNum = 6;  // pNum初始数量为6
    ratio.dNum = 9;  // dNum初始数量为9
    double bestThroughput;
    int result = calculator.CalFlexPRatioX(ratio, bestThroughput);

    // 检查返回值
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));

    // 检查计算出的 flexPRatio 和吞吐量是否符合预期
    ASSERT_NEAR(ratio.flexPRatio, 0.375, 1e-6);  // 0.375, 1e-6: 根据实际情况来修改容差
    ASSERT_GT(bestThroughput, 0.0);             // 0.0: 最佳吞吐量应该大于零
}

TEST_F(TestRMProportionCalculator, TestCalFlexPRatioXAbilityZero)
{
    ratio.pNum = 6;  // pNum初始数量为6
    ratio.dNum = 9;  // dNum初始数量为9
    // 测试能力为零的情况
    ratio.pAbility = 0.0;  // pAbility 初始为0.0

    double bestThroughput;
    int result = calculator.CalFlexPRatioX(ratio, bestThroughput);

    // 检查返回值
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

TEST_F(TestRMProportionCalculator, TestCalFlexPRatioXAbilityNegative)
{
    ratio.pNum = 6;  // pNum初始数量为6
    ratio.dNum = 9;  // dNum初始数量为9
    // 测试能力为负数的情况
    ratio.pAbility = -100.0;  // pAbility 初始为-100.0

    double bestThroughput;
    int result = calculator.CalFlexPRatioX(ratio, bestThroughput);

    // 检查返回值
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}

TEST_F(TestRMProportionCalculator, TestJudgeNeedPdSwtichUseThrputDNumSmaller)
{
    auto type = MINDIE::MS::DIGSRatioType::DIGS_ROLE_PD_RATIO;
    MINDIE::MS::DIGSGroupPDRatio ratio = {};
    ratio.pNum = 1;        // pNum初始数量为1
    ratio.dNum = 4;        // dNum初始数量为4
    ratio.pAbility = 2.0;  // pAbility 初始为2.0
    ratio.dAbility = 1.0;  // dAbility 初始为1.0
    ratio.tAbility = 1e9;  // 初始化时tAbility为0无法计算

    calculator.SaveRatio(ratio, type);
    ratio.pNum = 2;  // pNum数量增加为2
    ratio.dNum = 3;  // dNum数量减少为3
    auto status = calculator.JudgeNeedPdSwtichUseThrput(ratio);
    EXPECT_TRUE(status);
}

TEST_F(TestRMProportionCalculator, JudgeNeedPdSwtichUseThrputPNumSmaller)
{
    auto type = MINDIE::MS::DIGSRatioType::DIGS_ROLE_PD_RATIO;
    MINDIE::MS::DIGSGroupPDRatio ratio = {};
    ratio.pNum = 4;        // pNum初始数量为4
    ratio.dNum = 1;        // dNum初始数量为1
    ratio.pAbility = 1.0;  // pAbility 初始为1.0
    ratio.dAbility = 2.0;  // dAbility 初始为2.0
    ratio.tAbility = 1e9;  // 初始化时tAbility为0无法计算

    calculator.SaveRatio(ratio, type);
    ratio.pNum = 3;  // pNum数量减少为3
    ratio.dNum = 2;  // dNum数量增加为2
    auto status = calculator.JudgeNeedPdSwtichUseThrput(ratio);
    EXPECT_TRUE(status);
}