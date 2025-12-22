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
#include "gtest/gtest.h"
#include "InstanceRoleManager.h"

using namespace MINDIE::MS;
using namespace MINDIE::MS::roleManager;

constexpr size_t INIT_INPUT_LENGTH = 3000;
constexpr size_t INIT_OUTPUT_LENGTH = 200;
constexpr size_t PERIOD_INPUT_LENGTH = 1024;
constexpr size_t PERIOD_OUTPUT_LENGTH = 1024;
constexpr size_t ZERO = 0;
constexpr size_t ONE = 1;
constexpr size_t TWO = 2;
constexpr size_t THREE = 3;
constexpr size_t FIVE = 5;
constexpr size_t SIX = 6;
constexpr size_t SEVEN = 7;
constexpr size_t EIGHT = 8;
constexpr size_t NINE = 9;
constexpr size_t FIFTEEN = 15;
constexpr size_t SIXTEEN = 16;
constexpr size_t SEVENTEEN = 17;
static std::vector <MINDIE::MS::DIGSInstanceInfo> g_dIGSInstanceInfo;
static std::mutex g_insMutex;

class TestInstanceRoleManager : public testing::Test {
protected:
    void SetUp() override
    {
        MINDIE::MS::roleManager::InstanceRoleManager::Config config;
        InitAloConfig(config);
        // 获取当前测试用例的信息
        const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string testName = test_info->name();
        std::string subStr = "TestInitHeterogeneousTC";
        // 判断是否是异构场景
        if (testName.find(subStr) != std::string::npos) {
            config["is_heterogeneous"] = "true";
        }
        MINDIE::MS::roleManager::InstanceRoleManager::Create(config,
            std::bind(NotifyRoleDecision, std::placeholders::_1),
            std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager_);
    }
    void TearDown() override
    {
        g_dIGSInstanceInfo.clear();
    }
    static std::size_t RunStub()
    {
        std::cout << "run_stub" << std::endl;
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

    static int32_t NotifyRoleDecision(std::vector <MINDIE::MS::DIGSRoleDecision> decisions)
    {
        for (auto decision: decisions) {
            std::string role;
            MINDIE::MS::DIGSInstanceLabel label;
            switch (decision.role) {
                case MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE:
                    role = "Decode";
                    label = MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER;
                    break;
                case MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE:
                    role = "Prefill";
                    label = MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER;
                    break;
                case MINDIE::MS::DIGSInstanceRole::FLEX_INSTANCE:
                    role = "Flex";
                    label = MINDIE::MS::DIGSInstanceLabel::FLEX_STATIC;
                    break;
                default:
                    role = "Undefine";
                    label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
            }
            for (auto &instance: g_dIGSInstanceInfo) {
                if (instance.staticInfo.id == decision.id) {
                    instance.staticInfo.role = decision.role;
                    instance.staticInfo.label = label;
                    continue;
                }
            }
            std::cout << "DIGSRoleDecision: ID:" << decision.id << " role:" << role << " groupId:"
                      << decision.groupId << std::endl;
        }
        return 0;
    }

    // mock
    static int32_t InstancesCollector(
        std::vector <MINDIE::MS::DIGSInstanceInfo> &info,
        MINDIE::MS::DIGSRequestSummary &sum)
    {
        sum.inputLength = PERIOD_INPUT_LENGTH;
        sum.outputLength = PERIOD_OUTPUT_LENGTH;
        info = g_dIGSInstanceInfo;
        return 0;
    }

    void InitInstance(size_t num)
    {
        size_t decodeHardwareNum = TWO;
        for (size_t i = 0; i < num; i++) {
            MINDIE::MS::DIGSInstanceInfo digsInstanceInfo;
            digsInstanceInfo.staticInfo.id = i;
            digsInstanceInfo.staticInfo.virtualId = i;
            digsInstanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE;
            digsInstanceInfo.staticInfo.nodeRes.hardwareType = "800I A2(64G)";
            if (i >= decodeHardwareNum) {
                digsInstanceInfo.staticInfo.nodeRes.hardwareType = "800I A2(32G)";
            }
            digsInstanceInfo.scheduleInfo.allocatedBlocks = ZERO;
            digsInstanceInfo.scheduleInfo.allocatedSlots = ZERO;
            g_dIGSInstanceInfo.emplace_back(digsInstanceInfo);
        }
    }

    bool CheckInstance(size_t pNum, size_t dNum)
    {
        for (auto &instance: g_dIGSInstanceInfo) {
            if (instance.staticInfo.role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
                pNum--;
            }
            if (instance.staticInfo.role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
                dNum--;
            }
        }
        if (pNum == ZERO && dNum == ZERO) {
            return true;
        }
        return false;
    }

    // 比较生成的PD实例的个数
    bool IsPNumGreaterThanDNum()
    {
        size_t pNum = ZERO;
        size_t dNum = ZERO;
        for (auto &instance: g_dIGSInstanceInfo) {
            if (instance.staticInfo.role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
                pNum++;
            }
            if (instance.staticInfo.role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE) {
                dNum++;
            }
        }
        if (pNum > dNum) {
            return true;
        }
        return false;
    }

protected:
    std::unique_ptr<MINDIE::MS::roleManager::InstanceRoleManager> manager_;
};

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数P:D=1:1、实例数2，内容有效，预期结果1P1D
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC01)
{
    InitInstance(TWO);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = ONE;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(ONE, ONE), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数P:D=1:1、实例数17，内容有效，预期结果9P8D
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC02)
{
    InitInstance(SEVENTEEN);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = ONE;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(NINE, EIGHT), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数P:D=1:1、节点数 = 3 ，内容有效，预期结果2P1D
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC03)
{
    InitInstance(THREE);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = ONE;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(TWO, ONE), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数P:D=1:1、实例数6  预期结果3P3D ，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC04)
{
    InitInstance(SIX);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = ONE;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(THREE, THREE), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数P:D=3:2 实例数5  预期结果3P2D ，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC05)
{
    InitInstance(FIVE);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = THREE;
    size_t dRate = TWO;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(THREE, TWO), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数P:D=1:15 实例数5  预期结果1P1D ，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC06)
{
    InitInstance(TWO);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = FIFTEEN;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(ONE, ONE), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数P:D=1:15 实例数6  预期结果1P5D ，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC07)
{
    InitInstance(SIX);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = FIFTEEN;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(ONE, FIVE), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数P:D=1:15 实例数17 预期结果1P16D 或2P15D ，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC08)
{
    InitInstance(SEVENTEEN);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = FIFTEEN;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(TWO, FIFTEEN) || CheckInstance(1, SIXTEEN), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数，长输入断输出  节点数量小于16 预期结果P实例大于D实例数，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC10)
{
    constexpr size_t shortInputLength = 3000;
    constexpr size_t longOutputLength = 200;
    InitInstance(FIVE);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ZERO;
    size_t dRate = ZERO;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(IsPNumGreaterThanDNum(), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数，长输入断输出  节点数量大于16 预期结果P实例大于D实例数，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC11)
{
    constexpr size_t shortInputLength = 3000;
    constexpr size_t longOutputLength = 200;
    InitInstance(SEVENTEEN);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ZERO;
    size_t dRate = ZERO;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(IsPNumGreaterThanDNum(), true);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数，短输入长输出  节点数量小于16 预期结果P实例小于D实例数，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC12)
{
    constexpr size_t shortInputLength = 400;
    constexpr size_t longOutputLength = 1000;
    InitInstance(FIVE);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = shortInputLength;
    sum.outputLength = longOutputLength;
    size_t pRate = ZERO;
    size_t dRate = ZERO;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(IsPNumGreaterThanDNum(), false);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数，短输入长输出  节点数量大于16 预期结果P实例小于D实例数，内容有效
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC13)
{
    constexpr size_t shortInputLength = 400;
    constexpr size_t longOutputLength = 1000;
    InitInstance(SEVENTEEN);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = shortInputLength;
    sum.outputLength = longOutputLength;
    size_t pRate = ZERO;
    size_t dRate = ZERO;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(IsPNumGreaterThanDNum(), false);
}

/*
测试描述: 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数，内容有效，修改time_period，预期结果：PD比例不变
预期结果:
    1. 解析配置成功
*/
TEST_F(TestInstanceRoleManager, TestInitTC14)
{
    MINDIE::MS::roleManager::InstanceRoleManager::Config config;
    InitAloConfig(config);
    config["time_period"] = "2";
    InitInstance(FIVE);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ZERO;
    size_t dRate = ZERO;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(IsPNumGreaterThanDNum(), true);
}

/*
测试描述: 异构场景，身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数，内容有效，初始化实例数9 P:D = 1:8 设置config["is_heterogeneous"] = "true"
预期结果:
    1. 解析配置成功
    2. 预期结果 7P2D
*/
TEST_F(TestInstanceRoleManager, TestInitHeterogeneousTC01)
{
    InitInstance(NINE);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = EIGHT;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(SEVEN, TWO), true);
}

/*
测试描述: 异构场景， 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数，内容有效，初始化实例数17 P:D = 1:8 设置config["is_heterogeneous"] = "true"
预期结果:
    1. 解析配置成功
    2. 预期结果 8P9D
*/
TEST_F(TestInstanceRoleManager, TestInitHeterogeneousTC02)
{
    InitInstance(SEVENTEEN);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = EIGHT;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(FIFTEEN, TWO), true);
}

/*
测试描述: 异构场景， 身份决策引擎测试，创建身份决策引擎
测试步骤:
    1. 传入初始化参数，内容有效，异构场景，初始化实例数7 P:D = 1:8 设置config["is_heterogeneous"] = "true"
预期结果:
    1. 解析配置成功
    2.预期结果 5P2D
*/
TEST_F(TestInstanceRoleManager, TestInitHeterogeneousTC03)
{
    InitInstance(SEVEN);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = ONE;
    size_t dRate = EIGHT;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    EXPECT_EQ(CheckInstance(FIVE, TWO), true);
}
