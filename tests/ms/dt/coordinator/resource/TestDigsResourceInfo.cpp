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
#include "gtest/gtest.h"
#include "coordinator/resource/resource_info.h"
#include "coordinator/resource/res_schedule_info.h"
using namespace testing;

class TestDigsResourceInfo : public ::testing::Test {
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

/**
* @tc.name  : ResourceInfo_ShouldConstruct_WhenValidParameters
* @tc.number: ResourceInfoTest_001
* @tc.desc  : Test if ResourceInfo can be constructed with valid parameters
*/
TEST_F(TestDigsResourceInfo, ResourceInfo_ShouldConstruct_WhenValidParameters)
{
    constexpr int totalSlotsNum = 10;
    constexpr int totalBlocksNum = 20;
    constexpr int nodeResCpuMen = 30;
    constexpr int nodeResNpuMem = 40;
    constexpr int nodeResNpuBW = 50;
    double maxResRate = 0.5;

    MINDIE::MS::DIGSInstanceStaticInfo insInfo;
    insInfo.id = 1;
    insInfo.totalSlotsNum = totalSlotsNum;
    insInfo.totalBlockNum = totalBlocksNum;
    insInfo.nodeRes.cpuMem = nodeResCpuMen;
    insInfo.nodeRes.npuMem = nodeResNpuMem;
    insInfo.nodeRes.npuBW = nodeResNpuBW;

    MINDIE::MS::ResourceInfo resourceInfo(std::move(insInfo), maxResRate);
    ASSERT_EQ(resourceInfo.instanceStaticInfo_.id, 1);
    ASSERT_EQ(resourceInfo.instanceStaticInfo_.totalSlotsNum, totalSlotsNum);
    ASSERT_EQ(resourceInfo.instanceStaticInfo_.totalBlockNum, totalBlocksNum);
    ASSERT_EQ(resourceInfo.instanceStaticInfo_.nodeRes.cpuMem, nodeResCpuMen);
    ASSERT_EQ(resourceInfo.instanceStaticInfo_.nodeRes.npuMem, nodeResNpuMem);
    ASSERT_EQ(resourceInfo.instanceStaticInfo_.nodeRes.npuBW, nodeResNpuBW);
    ASSERT_EQ(resourceInfo.maxResRate_, maxResRate);
    ASSERT_EQ(resourceInfo.dynamicResRate_, maxResRate);
}

/**
 * @tc.name  : AddDemand_ShouldReturnTrue_WhenDemandAndStageAreValid
 * @tc.number: ResourceInfoTest_001
 * @tc.desc  : Test when demand and stage are valid, AddDemand should return true
 */
TEST_F(TestDigsResourceInfo, AddDemand_ShouldReturnTrue_WhenDemandAndStageAreValid)
{
    // Arrange
    MINDIE::MS::DIGSInstanceStaticInfo insInfo;
    double maxResRate = 1.0;
    MINDIE::MS::ResourceInfo resourceInfo(insInfo, maxResRate);
    std::unique_ptr<MINDIE::MS::MetaResource> demand = std::make_unique<MINDIE::MS::MetaResource>();
    MINDIE::MS::DIGSReqStage stage = MINDIE::MS::DIGSReqStage::PREFILL;

    // Act
    resourceInfo.AddDemand(demand, stage);
    EXPECT_TRUE(true);
}

/**
 * @tc.name  : AddDemand_ShouldReturnFalse_WhenStageIsInvalid
 * @tc.number: ResourceInfoTest_003
 * @tc.desc  : Test when stage is invalid, AddDemand should return false
 */
TEST_F(TestDigsResourceInfo, AddDemand_ShouldReturnFalse_WhenStageIsInvalid)
{
    // Arrange
    std::unique_ptr<MINDIE::MS::MetaResource> demand = std::make_unique<MINDIE::MS::MetaResource>();
    MINDIE::MS::DIGSReqStage stage = MINDIE::MS::DIGSReqStage::DECODE;
    MINDIE::MS::DIGSInstanceStaticInfo insInfo;
    double maxResRate = 1.0;
    MINDIE::MS::ResourceInfo resourceInfo(insInfo, maxResRate);
    // Act
    resourceInfo.AddDemand(demand, stage);
    EXPECT_TRUE(true);
}

/**
 * @tc.name  : UpdateScheduleLoad_ShouldReturnFalse_WhenisDynamicMaxResEnabledIsFalse
 * @tc.number: ResourceInfoTest_001
 * @tc.desc  : Test when isDynamicMaxResEnabled is false, UpdateScheduleLoad should return false
 */
TEST_F(TestDigsResourceInfo, UpdateScheduleLoad_ShouldReturnFalse_WhenisDynamicMaxResEnabledIsFalse)
{
    MINDIE::MS::DIGSInstanceStaticInfo insInfo;
    double maxResRate = 1.0;
    MINDIE::MS::ResourceInfo resourceInfo(insInfo, maxResRate);
    resourceInfo.isDynamicMaxResEnabled_ = false;
    bool result = resourceInfo.UpdateScheduleLoad(true);
    EXPECT_FALSE(result);
}

/**
 * @tc.name  : UpdateScheduleLoad_ShouldReturnTrue_WhenisDynamicMaxResEnabledIsTrue
 * @tc.number: ResourceInfoTest_002
 * @tc.desc  : Test when isDynamicMaxResEnabled is true, UpdateScheduleLoad should return true
 */
TEST_F(TestDigsResourceInfo, UpdateScheduleLoad_ShouldReturnTrue_WhenisDynamicMaxResEnabledIsTrue)
{
    MINDIE::MS::DIGSInstanceStaticInfo insInfo;
    double maxResRate = 1.0;
    MINDIE::MS::ResourceInfo resourceInfo(insInfo, maxResRate);
    resourceInfo.isDynamicMaxResEnabled_ = true;
    bool result = resourceInfo.UpdateScheduleLoad(true);
    EXPECT_TRUE(result);
}

/**
* @tc.name  : ResourceInfo_ReviseMaxResource_Test_001
* @tc.number: ResourceInfo_ReviseMaxResource_001
* @tc.desc  : Test when isDynamicMaxResEnabled_ is false then ReviseMaxResource returns false
*/
TEST_F(TestDigsResourceInfo, ResourceInfo_ReviseMaxResource_Test_001)
{
    MINDIE::MS::DIGSInstanceStaticInfo insInfo;
    double maxResRate = 1.0;
    MINDIE::MS::ResourceInfo resourceInfo(insInfo, maxResRate);
    resourceInfo.isDynamicMaxResEnabled_ = false;
    ASSERT_EQ(resourceInfo.ReviseMaxResource(), false);
}

/**
* @tc.name  : UpdateStaticInfo_ShouldUpdateFields_WhenValuesAreDifferent
* @tc.number: ResourceInfo_UpdateStaticInfo_001
* @tc.desc  : Test that UpdateStaticInfo correctly updates totalSlotsNum and totalBlockNum when they differ from current values
*/
TEST_F(TestDigsResourceInfo, UpdateStaticInfo_ShouldUpdateFields_WhenValuesAreDifferent)
{
    // Arrange
    MINDIE::MS::DIGSInstanceStaticInfo staticInfo;
    staticInfo.id = 1;
    staticInfo.totalSlotsNum = 100;
    staticInfo.totalBlockNum = 200;
    
    double maxResRate = 1.0;
    MINDIE::MS::ResourceInfo resourceInfo(staticInfo, maxResRate);
    
    MINDIE::MS::DIGSInstanceDynamicInfo dynamicInfo;
    dynamicInfo.id = 1;
    dynamicInfo.totalSlotsNum = 150;  // Different from staticInfo
    dynamicInfo.totalBlockNum = 250;  // Different from staticInfo
    
    // Store original values for comparison
    size_t originalSlots = resourceInfo.StaticInfo().totalSlotsNum;
    size_t originalBlocks = resourceInfo.StaticInfo().totalBlockNum;
    
    // Act
    resourceInfo.UpdateStaticInfo(dynamicInfo);
    
    // Assert
    ASSERT_EQ(resourceInfo.StaticInfo().totalSlotsNum, dynamicInfo.totalSlotsNum);
    ASSERT_EQ(resourceInfo.StaticInfo().totalBlockNum, dynamicInfo.totalBlockNum);
    ASSERT_NE(originalSlots, resourceInfo.StaticInfo().totalSlotsNum);
    ASSERT_NE(originalBlocks, resourceInfo.StaticInfo().totalBlockNum);
}

/**
* @tc.name  : UpdateStaticInfo_ShouldNotUpdateFields_WhenValuesAreSame
* @tc.number: ResourceInfo_UpdateStaticInfo_002
* @tc.desc  : Test that UpdateStaticInfo does not update totalSlotsNum and totalBlockNum when they are the same as current values
*/
TEST_F(TestDigsResourceInfo, UpdateStaticInfo_ShouldNotUpdateFields_WhenValuesAreSame)
{
    // Arrange
    MINDIE::MS::DIGSInstanceStaticInfo staticInfo;
    staticInfo.id = 1;
    staticInfo.totalSlotsNum = 100;
    staticInfo.totalBlockNum = 200;
    
    double maxResRate = 1.0;
    MINDIE::MS::ResourceInfo resourceInfo(staticInfo, maxResRate);
    
    MINDIE::MS::DIGSInstanceDynamicInfo dynamicInfo;
    dynamicInfo.id = 1;
    dynamicInfo.totalSlotsNum = 100;  // Same as staticInfo
    dynamicInfo.totalBlockNum = 200;  // Same as staticInfo
    
    // Store original values for comparison
    size_t originalSlots = resourceInfo.StaticInfo().totalSlotsNum;
    size_t originalBlocks = resourceInfo.StaticInfo().totalBlockNum;
    
    // Act
    resourceInfo.UpdateStaticInfo(dynamicInfo);
    
    // Assert
    ASSERT_EQ(resourceInfo.StaticInfo().totalSlotsNum, dynamicInfo.totalSlotsNum);
    ASSERT_EQ(resourceInfo.StaticInfo().totalBlockNum, dynamicInfo.totalBlockNum);
    ASSERT_EQ(originalSlots, resourceInfo.StaticInfo().totalSlotsNum);
    ASSERT_EQ(originalBlocks, resourceInfo.StaticInfo().totalBlockNum);
}