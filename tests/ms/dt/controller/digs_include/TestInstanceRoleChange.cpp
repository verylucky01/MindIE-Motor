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

constexpr size_t INIT_INPUT_LENGTH = 3000;
constexpr size_t INIT_OUTPUT_LENGTH = 200;
constexpr size_t INIT_ONE_THOUSAND = 1000;
static size_t g_periodInputLength = 1024;
static size_t g_periodOutputLength = 1024;
static size_t g_lengthInit = 1024;
static size_t g_numTwo = 2;
static size_t g_numThree = 3;
static size_t g_numFour = 4;
static size_t g_numFive = 5;
static size_t g_numSeven = 7;
static size_t g_numEight = 8;
static size_t g_numTen = 10;
static size_t g_numTwenty = 20;
static size_t g_numFifty = 50;
static std::vector<MINDIE::MS::DIGSInstanceInfo> g_dIGSInstanceInfo;
static std::mutex g_insMutex;

class TestInstanceRoleChange : public testing::Test {
protected:
    void SetUp() override
    {
        MINDIE::MS::roleManager::InstanceRoleManager::Config config;
        InitAloConfig(config);
        // ��ȡ��ǰ������������Ϣ
        const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string testName = test_info->name();
        std::string subStr = "TestHeterogeneousTC";
        // �ж��Ƿ����칹����
        if (testName.find(subStr) != std::string::npos) {
            config["is_heterogeneous"] = "true";
        }
        MINDIE::MS::roleManager::InstanceRoleManager::Create(config,
            std::bind(NotifyRoleDecision, std::placeholders::_1),
            std::bind(InstancesCollector, std::placeholders::_1, std::placeholders::_2), manager_);
    }

    void TearDown() override
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        g_periodInputLength = g_lengthInit;
        g_periodOutputLength = g_lengthInit;
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
        for (auto decision : decisions) {
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
            std::unique_lock<std::mutex> lock(g_insMutex);
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
        std::unique_lock<std::mutex> lock(g_insMutex);
        sum.inputLength = g_periodInputLength;
        sum.outputLength = g_periodOutputLength;
        info = g_dIGSInstanceInfo;
        return 0;
    }

    void InitInstance(size_t num)
    {
        size_t decodeHardwareNum = g_numTwo;
        for (size_t i = 0; i < num; i++) {
            MINDIE::MS::DIGSInstanceInfo digsInstanceInfo;
            digsInstanceInfo.staticInfo.id = i;
            digsInstanceInfo.staticInfo.role = MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE;
            digsInstanceInfo.staticInfo.nodeRes.hardwareType = "800I A2(64G)";
            if (i >= decodeHardwareNum) {
                digsInstanceInfo.staticInfo.nodeRes.hardwareType = "800I A2(32G)";
            }
            digsInstanceInfo.scheduleInfo.allocatedBlocks = 0;
            digsInstanceInfo.scheduleInfo.allocatedSlots = 0;
            digsInstanceInfo.staticInfo.groupId = 0;
            g_dIGSInstanceInfo.emplace_back(digsInstanceInfo);
        }
    }

    // ���ڵ㣬ɾ�����ݲ�ͬ��ʵ��
    void DeleteInstanceInThree()
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        if (g_dIGSInstanceInfo[1].staticInfo.role != g_dIGSInstanceInfo[0].staticInfo.role) {
            g_dIGSInstanceInfo.erase(g_dIGSInstanceInfo.begin());
            return;
        }
        g_dIGSInstanceInfo.erase(g_dIGSInstanceInfo.begin() + g_numTwo);
    }

    void CountPInstanceNum(std::map<uint64_t, size_t> &pNums)
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        for (auto insInfo : g_dIGSInstanceInfo) {
            if (insInfo.staticInfo.role == MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE) {
                pNums[insInfo.staticInfo.groupId]++;
            }
        }
    }

    void InitGroups(std::map<uint64_t, std::vector<MINDIE::MS::DIGSInstanceInfo>>& groupInfo)
    {
        InitInstance(g_numEight);

        // ����ڵ㣬�ڵ���3:5
        size_t group1 = g_numThree;
        size_t group2 = g_numFive;
        for (auto &info : g_dIGSInstanceInfo) {
            if (group1 > 0) {
                groupInfo[0].push_back(info);
                info.staticInfo.groupId = 0;
                group1--;
                continue;
            }
            groupInfo[1].push_back(info);
            info.staticInfo.groupId = 1;
            group2--;
        }

        MINDIE::MS::DIGSRequestSummary sum = {};
        sum.inputLength = INIT_INPUT_LENGTH;
        sum.outputLength = INIT_OUTPUT_LENGTH;
        size_t pRate = 0;
        size_t dRate = 0;
        manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    }

    bool CompareGroupInfo(std::map<uint64_t, size_t> oriNums, std::map<uint64_t, size_t> afterNums, bool sign)
    {
        bool res = true;
        for (auto num : oriNums) {
            // signΪtrueʱ��ԭPʵ��������
            if (sign) {
                res = (num.second > afterNums[num.first]);
            } else {
                res = (num.second < afterNums[num.first]);
            }
            if (!res) {
                return res;
            }
        }
        return res;
    }

protected:
    std::unique_ptr<MINDIE::MS::roleManager::InstanceRoleManager> manager_;
};

/*
��������: ���ٽڵ�
���Բ���:
    1. �����ʼ�����������������ڵ�(2P1D)
    2. �������ݵ����Ľڵ�(D�ڵ�)����������һ���ڵ�(P�ڵ�)��allocated slot��
Ԥ�ڽ��:
    1. ���ù�allocated slot���Ľڵ����ݻ�ת��
*/
TEST_F(TestInstanceRoleChange, TestChangeTC01)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    MINDIE::MS::DIGSInstanceRole originRole;
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        originRole = g_dIGSInstanceInfo[1].staticInfo.role;
        g_dIGSInstanceInfo[0].scheduleInfo.allocatedSlots = g_numTen;
        g_dIGSInstanceInfo[g_numTwo].scheduleInfo.allocatedSlots = g_numTen;
    }
    DeleteInstanceInThree();
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::unique_lock<std::mutex> lock(g_insMutex);
    EXPECT_EQ(originRole != g_dIGSInstanceInfo[1].staticInfo.role, true);
}

/*
��������: ���ٽڵ�
���Բ���:
    1. �����ʼ�����������������ڵ�(2P1D)
    2. �������ݵ����Ľڵ�(D�ڵ�)����������һ���ڵ�(P�ڵ�)��allocated block��
Ԥ�ڽ��:
    1. ����allocated block���Ľڵ����ݻ�ת��
*/
TEST_F(TestInstanceRoleChange, TestChangeTC02)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    MINDIE::MS::DIGSInstanceRole originRole;
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        originRole = g_dIGSInstanceInfo[1].staticInfo.role;
        g_dIGSInstanceInfo[0].scheduleInfo.allocatedBlocks = g_numFifty;
        g_dIGSInstanceInfo[g_numTwo].scheduleInfo.allocatedBlocks = g_numFifty;
    }
    DeleteInstanceInThree();
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::unique_lock <std::mutex> lock(g_insMutex);
    EXPECT_EQ(originRole != g_dIGSInstanceInfo[1].staticInfo.role, true);
}

/*
��������: �ڵ���������
���Բ���:
    1. �����ʼ�����������������������������ڵ�
    2. ͨ���ص�������������볤������ȴ�һ�������л����ڣ���֤�����л����
    3. ͨ���ص��������볤�����������ȴ�һ�������л����ڣ���֤�����л����
Ԥ�ڽ��:
    1. 2P1Dת��Ϊ1P2D
    2. 1P2Dת��Ϊ2P1D
*/
TEST_F(TestInstanceRoleChange, TestChangeTC03)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    std::map<uint64_t, size_t> oriNums;
    CountPInstanceNum(oriNums);
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::map<uint64_t, size_t> afterNums;
    CountPInstanceNum(afterNums);
    EXPECT_EQ(CompareGroupInfo(oriNums, afterNums, true), true);

    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        g_periodInputLength = INIT_INPUT_LENGTH;
        g_periodOutputLength = INIT_OUTPUT_LENGTH;
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    oriNums = afterNums;
    afterNums.clear();
    CountPInstanceNum(afterNums);
    EXPECT_EQ(CompareGroupInfo(oriNums, afterNums, false), true);
}

/*
��������: �ڵ���������
���Բ���:
    1. �����ʼ�����������������ڵ㣬��ʼ���ݱ���Ϊ2P1D
    2. �޸�����1P1D��allocated slot�����ȴ�������ûص���������ѱ�����Ϊ1P2D
Ԥ�ڽ��:
    1. δ����allocated slots�Ľڵ����ݸı�
*/
TEST_F(TestInstanceRoleChange, TestChangeTC04)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = g_numTwo;
    size_t dRate = 1;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        g_dIGSInstanceInfo[0].scheduleInfo.allocatedSlots = g_numTwenty;
        g_dIGSInstanceInfo[g_numTwo].scheduleInfo.allocatedSlots = g_numTwenty;
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::unique_lock<std::mutex> lock(g_insMutex);
    EXPECT_EQ(g_dIGSInstanceInfo[1].staticInfo.role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, true);
}

/*
��������: �ڵ���������
���Բ���:
    1. �����ʼ�����������������ڵ㣬��ʼ���ݱ���Ϊ2P1D
    2. �޸�����1P1D��allocated block�����ȴ�������ûص���������ѱ�����Ϊ1P2D
Ԥ�ڽ��:
    1. δ����allocated blocks�Ľڵ����ݸı�
*/
TEST_F(TestInstanceRoleChange, TestChangeTC05)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = g_numTwo;
    size_t dRate = 1;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        g_dIGSInstanceInfo[0].scheduleInfo.allocatedBlocks = g_numTwenty;
        g_dIGSInstanceInfo[g_numTwo].scheduleInfo.allocatedBlocks = g_numTwenty;
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::unique_lock<std::mutex> lock(g_insMutex);
    EXPECT_EQ(g_dIGSInstanceInfo[1].staticInfo.role == MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE, true);
}

/*
��������: ���ٽڵ�
���Բ���:
    1. �����ʼ�������������ĸ��ڵ�
    2. ����ʵ�����ݣ��ȴ�һ�����ݾ���
Ԥ�ڽ��:
    1. ���Ϊ1P1D
*/
TEST_F(TestInstanceRoleChange, TestChangeTC06)
{
    InitInstance(g_numFour);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        g_dIGSInstanceInfo.clear();
        InitInstance(g_numTwo);
        g_dIGSInstanceInfo[0].staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        g_dIGSInstanceInfo[1].staticInfo.role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::map<uint64_t, size_t> pNums;
    CountPInstanceNum(pNums);
    EXPECT_EQ(pNums[0] == 1, true);
}

/*
��������: ��ͬgroup�������л��໥����
���Բ���:
    1. �����ʼ������������������������8���ڵ�
    2. ��Ϊ���飬�ڵ���3:5
    2. ͨ���ص�������������볤������ı���ѱ���
Ԥ�ڽ��:
    1. ����ʵ�����ݶ����仯
*/
TEST_F(TestInstanceRoleChange, TestChangeTC07)
{
    std::map<uint64_t, std::vector<MINDIE::MS::DIGSInstanceInfo>> groupInfo;
    InitGroups(groupInfo);

    std::map<uint64_t, size_t> oriNums;
    CountPInstanceNum(oriNums);
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::map<uint64_t, size_t> afterNums;
    CountPInstanceNum(afterNums);
    EXPECT_EQ(CompareGroupInfo(oriNums, afterNums, true), true);

    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        g_periodInputLength = INIT_INPUT_LENGTH;
        g_periodOutputLength = INIT_OUTPUT_LENGTH;
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    oriNums = afterNums;
    afterNums.clear();
    CountPInstanceNum(afterNums);
    EXPECT_EQ(CompareGroupInfo(oriNums, afterNums, false), true);
}

/*
��������: �ڵ��������٣���������ı�
���Բ���:
    1. �����ʼ������������������������7���ڵ�
    2. ͨ���ص�������������볤�����ɾ��1���ڵ㣬�ȴ�һ�������л�����
    3. ͨ���ص��������볤����������ɾ��1���ڵ㣬�ȴ�һ�������л�����
Ԥ�ڽ��:
    1. ��һ�������л���P����
    2. �ڶ��������л���P���
*/
TEST_F(TestInstanceRoleChange, TestChangeTC08)
{
    InitInstance(g_numSeven);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    std::map<uint64_t, size_t> oriNums;
    CountPInstanceNum(oriNums);
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        g_dIGSInstanceInfo.erase(g_dIGSInstanceInfo.begin() + g_numFive);
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::map<uint64_t, size_t> afterNums;
    CountPInstanceNum(afterNums);
    EXPECT_EQ(CompareGroupInfo(oriNums, afterNums, true), true);

    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        g_periodInputLength = INIT_INPUT_LENGTH;
        g_periodOutputLength = INIT_OUTPUT_LENGTH;
        g_dIGSInstanceInfo.erase(g_dIGSInstanceInfo.begin());
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    oriNums = afterNums;
    afterNums.clear();
    CountPInstanceNum(afterNums);
    EXPECT_EQ(CompareGroupInfo(oriNums, afterNums, false), true);
}

/*
��������: �칹�������ڵ���٣��ı为�أ�PD���ݲ���
���Բ���:
    1. �����ʼ�����������������ڵ�
    2. ��������һ���ڵ㣬��������һ���ڵ��allocated slot��
Ԥ�ڽ��:
    1. PD���ݲ���
*/
TEST_F(TestInstanceRoleChange, TestHeterogeneousTC01)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    std::vector<MINDIE::MS::DIGSInstanceInfo> initRes;
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        initRes = g_dIGSInstanceInfo;  // ��ʼ���Ľ��
        g_dIGSInstanceInfo[0].scheduleInfo.allocatedSlots = g_numTen;
        g_dIGSInstanceInfo[g_numTwo].scheduleInfo.allocatedSlots = g_numTen;
        g_dIGSInstanceInfo.erase(g_dIGSInstanceInfo.begin() + g_numTwo);  // ɾ�����һ��ʵ��
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::unique_lock<std::mutex> lock(g_insMutex);
    EXPECT_EQ(initRes[0].staticInfo.role == g_dIGSInstanceInfo[0].staticInfo.role &&
              initRes[1].staticInfo.role == g_dIGSInstanceInfo[1].staticInfo.role, true);
}

/*
��������: �칹�������ڵ���٣��ı�����������ȣ�PD���ݲ���
���Բ���:
    1. �����ʼ�����������������ڵ�
    2. ��������һ���ڵ㣬����������ȱ仯
Ԥ�ڽ��:
    1. PD���ݲ���
*/
TEST_F(TestInstanceRoleChange, TestHeterogeneousTC02)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    std::vector<MINDIE::MS::DIGSInstanceInfo> initRes;
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        initRes = g_dIGSInstanceInfo;  // ��ʼ���Ľ��
        g_dIGSInstanceInfo.erase(g_dIGSInstanceInfo.begin()+ g_numTwo);  // ɾ�����һ��ʵ��
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    std::unique_lock<std::mutex> lock(g_insMutex);
    EXPECT_EQ(initRes[0].staticInfo.role == g_dIGSInstanceInfo[0].staticInfo.role &&
              initRes[1].staticInfo.role == g_dIGSInstanceInfo[1].staticInfo.role, true);
}

/*
��������: �칹���������ٽڵ㣬PD���ݲ���
���Բ���:
    1. �����ʼ�����������������ڵ�
    2. 4̨�ڵ����Ϊ2̨
Ԥ�ڽ��:
    1. PD���ݲ���
*/
TEST_F(TestInstanceRoleChange, TestHeterogeneousTC03)
{
    InitInstance(g_numFour);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    std::vector<MINDIE::MS::DIGSInstanceInfo> initRes;
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        initRes = g_dIGSInstanceInfo;  // ��ʼ���Ľ��
        g_dIGSInstanceInfo.erase(g_dIGSInstanceInfo.begin() + g_numTwo);
        g_dIGSInstanceInfo.erase(g_dIGSInstanceInfo.begin() + g_numTwo);  // ɾ�����һ��ʵ��
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    EXPECT_EQ(initRes[0].staticInfo.role == g_dIGSInstanceInfo[0].staticInfo.role &&
              initRes[1].staticInfo.role == g_dIGSInstanceInfo[1].staticInfo.role, true);
}

/*
��������: �칹�������ڵ㲻�䣬�ı为�أ�PD���ݲ���
���Բ���:
    1. �����ʼ�����������������ڵ�
    2. ��������һ���ڵ��allocated slot��
Ԥ�ڽ��:
    1. PD���ݲ���
*/
TEST_F(TestInstanceRoleChange, TestHeterogeneousTC04)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    std::vector<MINDIE::MS::DIGSInstanceInfo> initRes;
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        initRes = g_dIGSInstanceInfo;  // ��ʼ���Ľ��
        g_dIGSInstanceInfo[0].scheduleInfo.allocatedSlots = g_numTwenty;
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    EXPECT_EQ(initRes[0].staticInfo.role == g_dIGSInstanceInfo[0].staticInfo.role &&
              initRes[1].staticInfo.role == g_dIGSInstanceInfo[1].staticInfo.role &&
              initRes[g_numTwo].staticInfo.role == g_dIGSInstanceInfo[g_numTwo].staticInfo.role, true);
}

/*
��������: �칹�������ڵ㲻�䣬��������仯��PD���ݲ���
���Բ���:
    1. �����ʼ�����������������ڵ�
    2. ����������ȱ仯
Ԥ�ڽ��:
    1. PD���ݲ���
*/
TEST_F(TestInstanceRoleChange, TestHeterogeneousTC05)
{
    InitInstance(g_numThree);
    MINDIE::MS::DIGSRequestSummary sum = {};
    sum.inputLength = INIT_INPUT_LENGTH;
    sum.outputLength = INIT_OUTPUT_LENGTH;
    size_t pRate = 0;
    size_t dRate = 0;
    manager_->Init(g_dIGSInstanceInfo, sum, pRate, dRate);

    std::vector<MINDIE::MS::DIGSInstanceInfo> initRes;
    {
        std::unique_lock<std::mutex> lock(g_insMutex);
        initRes = g_dIGSInstanceInfo;  // ��ʼ���Ľ��
        g_periodInputLength = INIT_OUTPUT_LENGTH;
        g_periodOutputLength = INIT_ONE_THOUSAND;
    }
    std::this_thread::sleep_for(std::chrono::seconds(g_numFive));

    EXPECT_EQ(initRes[0].staticInfo.role == g_dIGSInstanceInfo[0].staticInfo.role &&
              initRes[1].staticInfo.role == g_dIGSInstanceInfo[1].staticInfo.role &&
              initRes[g_numTwo].staticInfo.role == g_dIGSInstanceInfo[g_numTwo].staticInfo.role, true);
}
