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
#include <cstdlib>  // #include <stdlib.h>
#include <string>
#include <memory>
#include <map>
#include <random>
#include <unistd.h>
#include <variant>
#include <vector>
#include <pthread.h>
#include <algorithm>
#include "gtest/gtest.h"

#include "ClusterNodes.h"
#include "ControllerListener.h"
#include "Configure.h"
#include "Helper.h"
using namespace MINDIE::MS;

// Fixture
class TestFlexProcess : public ::testing::Test {
protected:
    void SetUp() {}
    void TearDown() {}
};

constexpr size_t MAX_TOTAL_BLOCK_NUM = 1024;
constexpr size_t MAX_TOTAL_SLOTS_NUM = 200;
constexpr size_t MAX_AVAILABLE_BLOCK_NUM = 1024;
constexpr size_t MAX_AVAILABLE_SLOTS_NUM = 200;

void addMPeerInfo(nlohmann::json &peerInfo, int totalInsNum, int pNum)
{
    for (auto i = 0; i < totalInsNum; i++) {
        if (i == pNum) {
            continue;
        }
        peerInfo.emplace_back(i);
    }
}
void addPPeerInfo(nlohmann::json &peerInfo, int totalInsNum, int pNum)
{
    for (auto i = pNum; i < totalInsNum; i++) {
        peerInfo.emplace_back(i);
    }
}
void addEPeerInfo(nlohmann::json &peerInfo, int totalInsNum, int pNum)
{
    for (auto i = 0; i < pNum; i++) {
        peerInfo.emplace_back(i);
    }
}

std::vector<nlohmann::json> CreatePeersInfo(int type, uint64_t pNum, uint64_t totalInsNum, bool hasFlex)
{
    auto peerInfo = nlohmann::json::array();
    if (type == int('P')) {
        addPPeerInfo(peerInfo, totalInsNum, pNum);
    } else if (type == int('F')) {
        if (hasFlex) {
            addMPeerInfo(peerInfo, totalInsNum, pNum);
        }
    } else {
        addEPeerInfo(peerInfo, totalInsNum, pNum);

        if (hasFlex) {
            peerInfo.emplace_back(pNum);
        }
    }
    return peerInfo;
}

nlohmann::json CreateInputJson(uint8_t pNum, uint8_t dNum, bool hasFlex, uint8_t pPercentage)
{
    nlohmann::json instancesInfo;
    instancesInfo["ids"] = nlohmann::json::array();
    instancesInfo["instances"] = nlohmann::json::array();
    auto totalInsNum = pNum + dNum + (hasFlex ? 1 : 0);

    bool hasFlexTemp = hasFlex;
    for (auto i = 0; i < totalInsNum; i++) {
        instancesInfo["ids"].emplace_back(i);
        nlohmann::json instance;
        instance["id"] = i;
        instance["ip"] = "127.0.0." + std::to_string(i);
        instance["port"] = std::to_string(65535 - i);  // 65535 -i: init port
        instance["metric_port"] = std::to_string(i);
        instance["model_name"] = "llama_65b";
        nlohmann::json static_info;
        nlohmann::json dynamic_info;
        static_info["group_id"] = 0;          // 0: init group_id
        static_info["max_seq_len"] = 2048;    // 2048: init max_seq_len
        static_info["max_output_len"] = 512;  // 512: init max_output_len
        static_info["total_slots_num"] = MAX_TOTAL_SLOTS_NUM;
        static_info["total_block_num"] = MAX_TOTAL_BLOCK_NUM;
        static_info["block_size"] = 128;  // 128: init block_size
        if (i < pNum) {
            static_info["label"] = 2;  // 2: init label
            static_info["role"] = int('P');
            dynamic_info["peers"] = CreatePeersInfo(int('P'), pNum, totalInsNum, hasFlex);
        } else if (hasFlexTemp) {
            static_info["p_percentage"] = pPercentage;
            static_info["label"] = 2;  // 2: init label
            static_info["role"] = int('F');
            dynamic_info["peers"] = CreatePeersInfo(int('F'), pNum, totalInsNum, hasFlex);
            hasFlexTemp = false;  // FLEX仅一个
        } else {
            static_info["label"] = 3;  // 3: init label
            static_info["role"] = int('D');
            dynamic_info["peers"] = CreatePeersInfo(int('D'), pNum, totalInsNum, hasFlex);
        }
        dynamic_info["avail_slots_num"] = MAX_AVAILABLE_SLOTS_NUM;
        dynamic_info["avail_block_num"] = MAX_AVAILABLE_BLOCK_NUM;

        instance["static_info"] = static_info;
        instance["dynamic_info"] = dynamic_info;
        instancesInfo["instances"].emplace_back(instance);
    }
    return instancesInfo;
}

TEST_F(TestFlexProcess, TestFlexProcessNoFlex)
{
    uint8_t pNum = 2;          // 2: init pNum
    uint8_t dNum = 4;          // 4: init dNum
    uint8_t pPercentage = 30;  // 30: init pPercentage
    ClusterNodes nodes;
    nlohmann::json bodyJson = CreateInputJson(pNum, dNum, false, pPercentage);
    auto instances = bodyJson.at("instances");
    auto idsVec = bodyJson.at("ids").template get<std::vector<uint64_t>>();
    auto idsVecOri = idsVec;
    auto instancesOri = instances;
    EXPECT_EQ(idsVecOri.size(), pNum + dNum);
    EXPECT_EQ(instancesOri.size(), pNum + dNum);
    nodes.ProcessFlexInstance(idsVec, instances);
    EXPECT_EQ(idsVec.size(), idsVecOri.size());
    EXPECT_EQ(instances.size(), instancesOri.size());

    auto oriIter = instancesOri.begin();
    auto iter = instances.begin();
    for (uint8_t i = 0; i < idsVec.size(); i++) {
        EXPECT_EQ(idsVec[i], idsVecOri[i]);
        EXPECT_EQ(iter->at("id").template get<uint64_t>(), oriIter->at("id").template get<uint64_t>());
        EXPECT_EQ(iter->at("static_info").at("role").template get<DIGSInstanceRole>(),
                  oriIter->at("static_info").at("role").template get<DIGSInstanceRole>());
    }
}

TEST_F(TestFlexProcess, TestFlexProcessOnlyFlexTotalP)
{
    uint8_t pNum = 0;           // 0: init pNum
    uint8_t dNum = 0;           // 0: init dNum
    uint8_t pPercentage = 100;  // 100: init pPercentage
    bool hasFlex = true;
    ClusterNodes nodes;
    nlohmann::json bodyJson = CreateInputJson(pNum, dNum, hasFlex, pPercentage);
    auto instances = bodyJson.at("instances");
    auto idsVec = bodyJson.at("ids").template get<std::vector<uint64_t>>();
    uint8_t totalOriInsNum = pNum + dNum + (hasFlex ? 1 : 0);
    auto flexId = pNum;
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);
    nodes.ProcessFlexInstance(idsVec, instances);
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);

    auto idIter = std::find(idsVec.begin(), idsVec.end(), flexId);
    EXPECT_NE(idIter, idsVec.end());

    auto insIter = std::find_if(instances.begin(), instances.end(), [flexId](const nlohmann::json &ins) {
        return ins.at("id").template get<uint64_t>() == flexId;
    });

    EXPECT_NE(insIter, instances.end());
    EXPECT_EQ(insIter->at("static_info").at("role").template get<DIGSInstanceRole>(),
              DIGSInstanceRole::PREFILL_INSTANCE);
}

TEST_F(TestFlexProcess, TestFlexProcessOnlyPFlexWithOtherInstance)
{
    uint8_t pNum = 2;           // 2: init pNum
    uint8_t dNum = 3;           // 3: init dNum
    uint8_t pPercentage = 100;  // 100: init pPercentage
    bool hasFlex = true;
    ClusterNodes nodes;
    nlohmann::json bodyJson = CreateInputJson(pNum, dNum, hasFlex, pPercentage);
    auto instances = bodyJson.at("instances");
    auto idsVec = bodyJson.at("ids").template get<std::vector<uint64_t>>();
    uint8_t totalOriInsNum = pNum + dNum + (hasFlex ? 1 : 0);
    auto flexId = pNum;
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);
    nodes.ProcessFlexInstance(idsVec, instances);
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);

    for (auto &iter : instances) {
        if (iter.at("static_info").at("role").template get<DIGSInstanceRole>() ==
            DIGSInstanceRole::PREFILL_INSTANCE) {
            auto actPeer = iter.at("dynamic_info").at("peers").template get<std::vector<uint64_t>>();
            std::vector<uint64_t> expPeer = {3, 4, 5};  // {3, 4, 5}: init expPeer
            EXPECT_EQ(actPeer, expPeer);
        }

        if (iter.at("static_info").at("role").template get<DIGSInstanceRole>() ==
            DIGSInstanceRole::DECODE_INSTANCE) {
            auto actPeer = iter.at("dynamic_info").at("peers").template get<std::vector<uint64_t>>();
            std::vector<uint64_t> expPeer = {0, 1, 2};  // {0, 1, 2}: init expPeer
            EXPECT_EQ(actPeer, expPeer);
        }
    }
}

TEST_F(TestFlexProcess, TestFlexProcessOnlyFlexTotalD)
{
    uint8_t pNum = 0;         // 0: init pNum
    uint8_t dNum = 0;         // 0: init dNum
    uint8_t pPercentage = 0;  // 100: init pPercentage
    bool hasFlex = true;
    ClusterNodes nodes;
    nlohmann::json bodyJson = CreateInputJson(pNum, dNum, hasFlex, pPercentage);
    auto instances = bodyJson.at("instances");
    auto idsVec = bodyJson.at("ids").template get<std::vector<uint64_t>>();
    uint8_t totalOriInsNum = pNum + dNum + (hasFlex ? 1 : 0);
    auto flexId = pNum;
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);
    nodes.ProcessFlexInstance(idsVec, instances);
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);

    auto idIter = std::find(idsVec.begin(), idsVec.end(), flexId);
    EXPECT_EQ(idIter, idsVec.end());
    auto idIter1 = std::find(idsVec.begin(), idsVec.end(), DECODE_INS_ID_TRANSFER_BY_FLEX);
    EXPECT_NE(idIter1, idsVec.end());

    auto insIter = std::find_if(instances.begin(), instances.end(), [flexId](const nlohmann::json &ins) {
        return ins.at("id").template get<uint64_t>() == flexId;
    });
    EXPECT_EQ(insIter, instances.end());

    auto insIter1 = std::find_if(
        instances.begin(), instances.end(), [DECODE_INS_ID_TRANSFER_BY_FLEX](const nlohmann::json &ins) {
            return ins.at("id").template get<uint64_t>() == DECODE_INS_ID_TRANSFER_BY_FLEX;
        });
    EXPECT_NE(insIter1, instances.end());
    EXPECT_EQ(insIter1->at("static_info").at("role").template get<DIGSInstanceRole>(),
              DIGSInstanceRole::DECODE_INSTANCE);
}

TEST_F(TestFlexProcess, TestFlexProcessOnlyDFlexWithOtherInstance)
{
    uint8_t pNum = 2;         // 2: init pNum
    uint8_t dNum = 3;         // 3: init dNum
    uint8_t pPercentage = 0;  // 0: init pPercentage
    bool hasFlex = true;
    ClusterNodes nodes;
    nlohmann::json bodyJson = CreateInputJson(pNum, dNum, hasFlex, pPercentage);
    auto instances = bodyJson.at("instances");
    auto idsVec = bodyJson.at("ids").template get<std::vector<uint64_t>>();
    uint8_t totalOriInsNum = pNum + dNum + (hasFlex ? 1 : 0);
    auto flexId = pNum;
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);
    nodes.ProcessFlexInstance(idsVec, instances);
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);

    for (auto &iter : instances) {
        if (iter.at("static_info").at("role").template get<DIGSInstanceRole>() ==
            DIGSInstanceRole::PREFILL_INSTANCE) {
            auto actPeer = iter.at("dynamic_info").at("peers").template get<std::vector<uint64_t>>();
            std::vector<uint64_t> expPeer = {DECODE_INS_ID_TRANSFER_BY_FLEX, 3, 4,
                                             5};  // {x, 3, 4, 5}: init expPeer
            EXPECT_EQ(actPeer, expPeer);
        }

        if (iter.at("static_info").at("role").template get<DIGSInstanceRole>() ==
            DIGSInstanceRole::DECODE_INSTANCE) {
            auto actPeer = iter.at("dynamic_info").at("peers").template get<std::vector<uint64_t>>();
            std::vector<uint64_t> expPeer = {0, 1};  // {0, 1}: init expPeer
            EXPECT_EQ(actPeer, expPeer);
        }
    }
}

void VerifyDecodeInstance(const nlohmann::json &instances, uint64_t decodeId, double pPercentageRatio)
{
    auto insIter1 = std::find_if(
        instances.begin(),
        instances.end(),
        [DECODE_INS_ID_TRANSFER_BY_FLEX](const nlohmann::json &ins) {
            return ins.at("id").template get<uint64_t>() == DECODE_INS_ID_TRANSFER_BY_FLEX;
        });
    EXPECT_NE(insIter1, instances.end());
    EXPECT_EQ(insIter1->at("static_info").at("role").template get<DIGSInstanceRole>(),
              DIGSInstanceRole::DECODE_INSTANCE);
    EXPECT_EQ(insIter1->at("static_info").at("total_slots_num").template get<uint64_t>(),
              static_cast<uint64_t>(200 * (1 - pPercentageRatio)));  // 1 200: 乘积
    EXPECT_EQ(insIter1->at("static_info").at("total_block_num").template get<uint64_t>(),
              static_cast<uint64_t>(1024 * (1 - pPercentageRatio)));  // 1 1024: 乘积
    EXPECT_EQ(insIter1->at("dynamic_info").at("avail_slots_num").template get<uint64_t>(),
              static_cast<uint64_t>(200 * (1 - pPercentageRatio)));  // 1 200: 乘积
    EXPECT_EQ(insIter1->at("dynamic_info").at("avail_block_num").template get<uint64_t>(),
              static_cast<uint64_t>(1024 * (1 - pPercentageRatio)));  // 1 1024: 乘积
}

TEST_F(TestFlexProcess, TestFlexProcessOnlyFlexBothPAndD)
{
    uint8_t pNum = 0;          // 0: init pNum
    uint8_t dNum = 0;          // 0: init dNum
    uint8_t pPercentage = 40;  // 40: init pPercentage
    bool hasFlex = true;
    ClusterNodes nodes;
    nlohmann::json bodyJson = CreateInputJson(pNum, dNum, hasFlex, pPercentage);
    auto instances = bodyJson.at("instances");
    auto idsVec = bodyJson.at("ids").template get<std::vector<uint64_t>>();
    uint8_t totalOriInsNum = pNum + dNum + (hasFlex ? 1 : 0);
    auto flexId = pNum;
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);
    nodes.ProcessFlexInstance(idsVec, instances);
    EXPECT_EQ(idsVec.size(), totalOriInsNum + 1);
    EXPECT_EQ(instances.size(), totalOriInsNum + 1);

    auto idIter = std::find(idsVec.begin(), idsVec.end(), flexId);
    EXPECT_NE(idIter, idsVec.end());
    auto idIter1 = std::find(idsVec.begin(), idsVec.end(), DECODE_INS_ID_TRANSFER_BY_FLEX);
    EXPECT_NE(idIter1, idsVec.end());

    double pPercentageRatio = static_cast<double>(pPercentage) / FLEX_INSTANCE_P_PERCENTAGE_MAX;
    auto insIter = std::find_if(instances.begin(), instances.end(), [flexId](const nlohmann::json &ins) {
        return ins.at("id").template get<uint64_t>() == flexId;
    });
    EXPECT_NE(insIter, instances.end());
    EXPECT_EQ(insIter->at("static_info").at("role").template get<DIGSInstanceRole>(),
              DIGSInstanceRole::PREFILL_INSTANCE);
    EXPECT_EQ(insIter->at("static_info").at("total_slots_num").template get<uint64_t>(),
              static_cast<uint64_t>(MAX_TOTAL_SLOTS_NUM * pPercentageRatio));
    EXPECT_EQ(insIter->at("static_info").at("total_block_num").template get<uint64_t>(),
              static_cast<uint64_t>(MAX_TOTAL_BLOCK_NUM * pPercentageRatio));
    EXPECT_EQ(insIter->at("dynamic_info").at("avail_slots_num").template get<uint64_t>(),
              static_cast<uint64_t>(MAX_AVAILABLE_SLOTS_NUM * pPercentageRatio));
    EXPECT_EQ(insIter->at("dynamic_info").at("avail_block_num").template get<uint64_t>(),
              static_cast<uint64_t>(MAX_AVAILABLE_BLOCK_NUM * pPercentageRatio));
    VerifyDecodeInstance(instances, DECODE_INS_ID_TRANSFER_BY_FLEX, pPercentageRatio);
}

TEST_F(TestFlexProcess, TestFlexProcessBothPAndDFlexWithOtherInstance)
{
    uint8_t pNum = 2;          // 2: init pNum
    uint8_t dNum = 2;          // 2: init dNum
    uint8_t pPercentage = 50;  // 50: init pPercentage
    bool hasFlex = true;
    ClusterNodes nodes;
    nlohmann::json bodyJson = CreateInputJson(pNum, dNum, hasFlex, pPercentage);
    auto instances = bodyJson.at("instances");
    auto idsVec = bodyJson.at("ids").template get<std::vector<uint64_t>>();
    uint8_t totalOriInsNum = pNum + dNum + (hasFlex ? 1 : 0);
    auto flexId = pNum;
    EXPECT_EQ(idsVec.size(), totalOriInsNum);
    EXPECT_EQ(instances.size(), totalOriInsNum);
    nodes.ProcessFlexInstance(idsVec, instances);
    EXPECT_EQ(idsVec.size(), totalOriInsNum + 1);
    EXPECT_EQ(instances.size(), totalOriInsNum + 1);

    for (auto &iter : instances) {
        if (iter.at("static_info").at("role").template get<DIGSInstanceRole>() ==
            DIGSInstanceRole::PREFILL_INSTANCE) {
            auto actPeer = iter.at("dynamic_info").at("peers").template get<std::vector<uint64_t>>();
            std::sort(actPeer.begin(), actPeer.end());
            std::vector<uint64_t> expPeer = {3, 4,
                                             DECODE_INS_ID_TRANSFER_BY_FLEX};  // {3, 4, x}: init expPeer
            EXPECT_EQ(actPeer, expPeer);
        }

        if (iter.at("static_info").at("role").template get<DIGSInstanceRole>() ==
            DIGSInstanceRole::DECODE_INSTANCE) {
            auto actPeer = iter.at("dynamic_info").at("peers").template get<std::vector<uint64_t>>();
            std::vector<uint64_t> expPeer = {0, 1, 2};  // {0, 1, 2}: init expPeer
            EXPECT_EQ(actPeer, expPeer);
        }
    }
}

TEST_F(TestFlexProcess, TestClusterFlexInstanceInfoInterface)
{
    ClusterNodes nodes;
    uint64_t flexId = 2333;     // 2333: init flexId
    uint64_t pPercentage = 50;  // 50: init pPercentage
    EXPECT_EQ(nodes.IsFlexSplitedIntoTwoInstance(), false);

    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);
    EXPECT_EQ(nodes.IsFlexSplitedIntoTwoInstance(), true);
    EXPECT_EQ(nodes.IsBothPAndDFromFlex(flexId, DECODE_INS_ID_TRANSFER_BY_FLEX), true);
    EXPECT_EQ(nodes.IsBothPAndDFromFlex(flexId, 2111), false);  // 2111: dId
    EXPECT_EQ(nodes.IsInstanceFromFlex(flexId), true);
    EXPECT_EQ(nodes.IsInstanceFromFlex(DECODE_INS_ID_TRANSFER_BY_FLEX), true);
    EXPECT_EQ(nodes.IsInstanceFromFlex(2111), false);  // 2111: id
    uint64_t flexId1 = flexId;
    nodes.ProcTaskQuaryDInstanceIdUnderFlexSituation(flexId1);
    EXPECT_EQ(flexId1, DECODE_INS_ID_TRANSFER_BY_FLEX);

    EXPECT_EQ(nodes.GetInsNumMax(), 4097);  // 97: 待比较值

    nodes.ClearClusterFlexInstanceInfo();
    EXPECT_EQ(nodes.IsFlexSplitedIntoTwoInstance(), false);
    pPercentage = 100;  // 100: init pPercentage
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);
    EXPECT_EQ(nodes.IsFlexSplitedIntoTwoInstance(), false);
    EXPECT_EQ(nodes.GetInsNumMax(), 4096);  // 96: 待比较值

    std::vector<uint64_t> containVec = {flexId, 23, 69};  // {flexId, 23, 69}: init containVec
    std::vector<uint64_t> notContainVec = {11, 23, 69};  // {11, 23, 69}: init notContainVec

    EXPECT_EQ(nodes.IsVecContainsFlex(containVec), true);
    EXPECT_EQ(nodes.IsVecContainsFlex(notContainVec), false);
}

static void VerifySchedulerFlexProcess(ClusterNodes &nodes)
{
    uint64_t flexId = 2333;
    uint64_t pPercentage = 100;
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);

    std::vector<DIGSInstanceScheduleInfo> schedulerInfo2 = {
        {flexId, 500, 300},  // {flexId, 500, 300}: init schedulerInfo2
        {9, 200, 30},        // {9, 200, 30}: init schedulerInfo2
    };
    nodes.ProcSchedulerInfoUnderFlexSituation(schedulerInfo2);
    EXPECT_EQ(schedulerInfo2.size(), 2);  // 2: 待比较值
    auto find2 = std::find_if(schedulerInfo2.begin(), schedulerInfo2.end(),
                              [flexId](const DIGSInstanceScheduleInfo &info) { return info.id == flexId; });
    EXPECT_NE(find2, schedulerInfo2.end());
    EXPECT_EQ(find2->allocatedSlots, 500);   // 500: 待比较值
    EXPECT_EQ(find2->allocatedBlocks, 300);  // 300: 待比较值
    nodes.ClearClusterFlexInstanceInfo();
}

TEST_F(TestFlexProcess, TestSchedulerFlexProcess)
{
    ClusterNodes nodes;
    uint64_t flexId = 2333;     // 2333: init flexId
    uint64_t pPercentage = 50;  // 50: init pPercentage
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);

    std::vector<DIGSInstanceScheduleInfo> schedulerInfo = {
        {flexId, 200, 200},                          // {flexId, 200, 200}: init schedulerInfo
        {DECODE_INS_ID_TRANSFER_BY_FLEX, 100, 100},  // {DECODE_INS_ID_TRANSFER_BY_FLEX, 100, 100}: init schedulerInfo
        {2, 200, 30},                                //  {2, 200, 30}: init schedulerInfo
    };

    nodes.ProcSchedulerInfoUnderFlexSituation(schedulerInfo);
    EXPECT_EQ(schedulerInfo.size(), 2);  // 2: 待比较值
    auto find = std::find_if(schedulerInfo.begin(), schedulerInfo.end(),
                             [flexId](const DIGSInstanceScheduleInfo &info) { return info.id == flexId; });
    EXPECT_NE(find, schedulerInfo.end());
    EXPECT_EQ(find->allocatedSlots, 300);   // 300: 待比较值
    EXPECT_EQ(find->allocatedBlocks, 300);  // 300: 待比较值
    nodes.ClearClusterFlexInstanceInfo();
    flexId = 4555;    // 4555: init flexId
    pPercentage = 0;  // 0: init pPercentage
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);

    std::vector<DIGSInstanceScheduleInfo> schedulerInfo1 = {
        {DECODE_INS_ID_TRANSFER_BY_FLEX, 100, 200},  // {x, 100, 200}: init schedulerInfo
        {2, 200, 30},                                // {2, 200, 30}: init schedulerInfo
    };
    nodes.ProcSchedulerInfoUnderFlexSituation(schedulerInfo1);
    EXPECT_EQ(schedulerInfo1.size(), 2);  // 2: 待比较值
    auto find1 = std::find_if(schedulerInfo1.begin(), schedulerInfo1.end(),
                              [flexId](const DIGSInstanceScheduleInfo &info) { return info.id == flexId; });
    EXPECT_NE(find1, schedulerInfo1.end());
    EXPECT_EQ(find1->allocatedSlots, 100);   // 100: 待比较值
    EXPECT_EQ(find1->allocatedBlocks, 200);  // 200: 待比较值
    find1 = std::find_if(schedulerInfo1.begin(), schedulerInfo1.end(),
                         [DECODE_INS_ID_TRANSFER_BY_FLEX](const DIGSInstanceScheduleInfo &info) {
                             return info.id == DECODE_INS_ID_TRANSFER_BY_FLEX;
                         });
    EXPECT_EQ(find1, schedulerInfo1.end());
    nodes.ClearClusterFlexInstanceInfo();
    VerifySchedulerFlexProcess(nodes);
}

TEST_F(TestFlexProcess, TestIdsFlexProcess)
{
    ClusterNodes nodes;
    uint64_t flexId = 2333;     // 2333: init flexId
    uint64_t pPercentage = 50;  // 50: init pPercentage
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);

    std::vector<uint64_t> ids = {1, 0, flexId};

    nodes.ProcInstanceIdsUnderFlexSituation(ids);
    EXPECT_EQ(ids.size(), 4);  // 4: 待比较值
    auto find = std::find(ids.begin(), ids.end(), flexId);
    EXPECT_NE(find, ids.end());
    find = std::find(ids.begin(), ids.end(), DECODE_INS_ID_TRANSFER_BY_FLEX);
    EXPECT_NE(find, ids.end());
    nodes.ClearClusterFlexInstanceInfo();

    flexId = 4555;    // 4555: init flexId
    pPercentage = 0;  // 0: init pPercentage
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);

    std::vector<uint64_t> ids1 = {1, flexId, 5};  // {1, flexId, 5}: init ids1
    nodes.ProcInstanceIdsUnderFlexSituation(ids1);
    EXPECT_EQ(ids1.size(), 3);  // 3: 待比较值
    auto find1 = std::find(ids1.begin(), ids1.end(), flexId);
    EXPECT_EQ(find1, ids1.end());
    find1 = std::find(ids1.begin(), ids1.end(), DECODE_INS_ID_TRANSFER_BY_FLEX);
    EXPECT_NE(find1, ids1.end());
    nodes.ClearClusterFlexInstanceInfo();

    flexId = 7897;      // 7897: init flexId
    pPercentage = 100;  // 100: init flexId
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);

    std::vector<uint64_t> ids2 = {20, flexId, 5};  // {20, flexId, 5}: init ids2
    nodes.ProcInstanceIdsUnderFlexSituation(ids2);
    EXPECT_EQ(ids2.size(), 3);  // 3: 待比较值
    auto find2 = std::find(ids2.begin(), ids2.end(), flexId);
    EXPECT_NE(find2, ids2.end());
    find2 = std::find(ids2.begin(), ids2.end(), DECODE_INS_ID_TRANSFER_BY_FLEX);
    EXPECT_EQ(find2, ids2.end());
    nodes.ClearClusterFlexInstanceInfo();
}

TEST_F(TestFlexProcess, TestTaskNumFlexProcess)
{
    ClusterNodes nodes;
    uint64_t flexId = 2333;  // 2333: init flexId
    uint64_t flexDId = DECODE_INS_ID_TRANSFER_BY_FLEX;
    uint64_t otherInsId = 6;           // 6: init otherInsId
    size_t flexTaskNum = 5;             // 5: init flexTaskNum
    size_t flexDTaskNum = 7;            // 7: init flexDTaskNum
    size_t otherInsTaskNum = 3;        // 3: init otherInsTaskNum
    size_t flexReqIdBase = 11223344;    // 11223344: init flexReqIdBase
    size_t flexDReqIdBase = 55667788;   // 55667788: init flexDReqIdBase
    size_t otherReqIdBase = 99990000;  // 99990000: init otherReqIdBase
    nodes.AddInstance(flexId, "127.0.0.1", "1025", DIGSInstanceRole::PREFILL_INSTANCE, "llama2-70B");
    nodes.AddInstance(flexDId, "127.0.0.1", "1025", DIGSInstanceRole::DECODE_INSTANCE, "llama2-70B");
    nodes.AddInstance(otherInsId, "127.0.0.3", "1028", DIGSInstanceRole::DECODE_INSTANCE, "llama2-70B");
    for (size_t i = 0; i < flexTaskNum; i++) {
        nodes.AddTask(flexId, std::to_string(flexReqIdBase + i));
    }
    for (size_t i = 0; i < flexDTaskNum; i++) {
        nodes.AddTask(flexDId, std::to_string(flexDReqIdBase + i));
    }
    for (size_t i = 0; i < otherInsTaskNum; i++) {
        nodes.AddTask(otherInsId, std::to_string(otherReqIdBase + i));
    }

    uint64_t pPercentage = 50;  // 50: init pPercentage
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);
    EXPECT_EQ(nodes.GetInstanceTaskNumUnderFlexSituation(flexId), flexTaskNum + flexDTaskNum);
    EXPECT_EQ(nodes.GetInstanceTaskNumUnderFlexSituation(otherInsId), otherInsTaskNum);

    nodes.ClearClusterFlexInstanceInfo();
    pPercentage = 0;  // 0: init pPercentage
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);
    EXPECT_EQ(nodes.GetInstanceTaskNumUnderFlexSituation(flexId), flexDTaskNum);
    EXPECT_EQ(nodes.GetInstanceTaskNumUnderFlexSituation(otherInsId), otherInsTaskNum);

    nodes.ClearClusterFlexInstanceInfo();
    pPercentage = 100;  // 100: init pPercentage
    nodes.UpdateClusterFlexInstanceInfo(flexId, pPercentage);
    EXPECT_EQ(nodes.GetInstanceTaskNumUnderFlexSituation(flexId), flexTaskNum);
    EXPECT_EQ(nodes.GetInstanceTaskNumUnderFlexSituation(otherInsId), otherInsTaskNum);
}