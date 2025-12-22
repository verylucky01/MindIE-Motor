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
#include <memory>
#include <gtest/gtest.h>
#define private public
#define protected public
#include "coordinator/digs_scheduler/selectpolicy/static_alloc_policy.h"
#include "coordinator/resource/resource_view_manager.h"
#include "coordinator/resource/resource_info.h"
#include "coordinator/request/digs_request.h"
#include "common/digs/digs_instance.h"
#include "coordinator/digs_scheduler/meta_resource.h"

using namespace MINDIE::MS;

class TestStaticAllocPolicy : public testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

// Test StaticAllocPolicy constructor
TEST_F(TestStaticAllocPolicy, TestConstructor)
{
    size_t maxResNum = 100;
    StaticAllocPolicy policy(maxResNum);
    
    // Verify constructor initializes correctly
    EXPECT_EQ(policy.staticPrefillPool_.size(), 0);
    EXPECT_EQ(policy.staticDecodePool_.size(), 0);
}

// Test LoadResourceView method - PREFILL stage
TEST_F(TestStaticAllocPolicy, TestLoadResourceViewPrefill)
{
    size_t maxResNum = 100;
    StaticAllocPolicy policy(maxResNum);
    
    // Create ResourceViewManager
    auto resourceViewManager = std::make_unique<ResourceViewManager>(maxResNum);
    
    // Create test ResourceInfo and ResourceLoad
    DIGSInstanceStaticInfo staticInfo;
    staticInfo.id = 1;
    staticInfo.groupId = 1;
    staticInfo.label = DIGSInstanceLabel::PREFILL_STATIC;
    staticInfo.totalSlotsNum = 100;
    staticInfo.totalBlockNum = 100;
    
    auto resourceInfo = std::make_shared<ResourceInfo>(staticInfo, 1.0);
    DIGSInstanceDynamicInfo dynamicInfo;
    auto resourceLoad = std::make_shared<ResourceLoad>(dynamicInfo);
    
    // Add to ResourceViewManager
    ResourceViewManager::ResInfo resInfo = std::make_pair(resourceInfo, resourceLoad);
    resourceViewManager->AddResInfo(resInfo);
    
    // Call LoadResourceView
    policy.LoadResourceView(resourceViewManager, DIGSReqStage::PREFILL);
    
    // Verify resource view is loaded
    EXPECT_GT(policy.staticPrefillPool_.size(), 0);
}

// Test LoadResourceView method - DECODE stage
TEST_F(TestStaticAllocPolicy, TestLoadResourceViewDecode)
{
    size_t maxResNum = 100;
    StaticAllocPolicy policy(maxResNum);
    
    // Create ResourceViewManager
    auto resourceViewManager = std::make_unique<ResourceViewManager>(maxResNum);
    
    // Create test ResourceInfo and ResourceLoad
    DIGSInstanceStaticInfo staticInfo;
    staticInfo.id = 1;
    staticInfo.groupId = 1;
    staticInfo.label = DIGSInstanceLabel::DECODE_STATIC;
    staticInfo.totalSlotsNum = 100;
    staticInfo.totalBlockNum = 100;
    
    auto resourceInfo = std::make_shared<ResourceInfo>(staticInfo, 1.0);
    DIGSInstanceDynamicInfo dynamicInfo;
    auto resourceLoad = std::make_shared<ResourceLoad>(dynamicInfo);
    
    // Add to ResourceViewManager
    ResourceViewManager::ResInfo resInfo = std::make_pair(resourceInfo, resourceLoad);
    resourceViewManager->AddResInfo(resInfo);
    
    // Call LoadResourceView
    policy.LoadResourceView(resourceViewManager, DIGSReqStage::DECODE);
    
    // Verify resource view is loaded
    EXPECT_GT(policy.staticDecodePool_.size(), 0);
    EXPECT_EQ(policy.staticDecodePool_.begin()->first, staticInfo.groupId);
}

// Test SelectPrefillInst method - successfully select instance
TEST_F(TestStaticAllocPolicy, TestSelectPrefillInstSuccess)
{
    size_t maxResNum = 100;
    StaticAllocPolicy policy(maxResNum);
    
    // Create test request
    std::string reqId = "test_req_1";
    size_t inputLen = 100;
    auto request = std::make_shared<DIGSRequest>(reqId, inputLen);
    
    // Create resource demand
    std::vector<uint64_t> demandAttrs = {10, 10};
    auto demand = std::make_unique<MetaResource>(std::move(demandAttrs));
    request->InitScheduleInfo(std::move(demand));
    
    // Create ResourceViewManager and add resources
    auto resourceViewManager = std::make_unique<ResourceViewManager>(maxResNum);
    
    DIGSInstanceStaticInfo staticInfo;
    staticInfo.id = 1;
    staticInfo.groupId = 1;
    staticInfo.label = DIGSInstanceLabel::PREFILL_STATIC;
    staticInfo.totalSlotsNum = 100;
    staticInfo.totalBlockNum = 100;
    
    auto resourceInfo = std::make_shared<ResourceInfo>(staticInfo, 1.0);
    DIGSInstanceDynamicInfo dynamicInfo;
    auto resourceLoad = std::make_shared<ResourceLoad>(dynamicInfo);
    
    ResourceViewManager::ResInfo resInfo = std::make_pair(resourceInfo, resourceLoad);
    resourceViewManager->AddResInfo(resInfo);
    
    // Load resource view
    policy.LoadResourceView(resourceViewManager, DIGSReqStage::PREFILL);
    
    // Call SelectPrefillInst
    uint64_t prefillId = 0;
    uint64_t groupId = 0;
    int32_t result = policy.SelectPrefillInst(request, prefillId, groupId);
    
    // Verify results
    EXPECT_EQ(result, static_cast<int32_t>(common::Status::OK));
    EXPECT_EQ(prefillId, staticInfo.id);
    EXPECT_EQ(groupId, staticInfo.groupId);
}

// Test SelectPrefillInst method - no available resources
TEST_F(TestStaticAllocPolicy, TestSelectPrefillInstNoResource)
{
    size_t maxResNum = 100;
    StaticAllocPolicy policy(maxResNum);
    
    // Create test request
    std::string reqId = "test_req_1";
    size_t inputLen = 100;
    auto request = std::make_shared<DIGSRequest>(reqId, inputLen);
    
    // Create resource demand
    std::vector<uint64_t> demandAttrs = {10, 10};
    auto demand = std::make_unique<MetaResource>(std::move(demandAttrs));
    request->InitScheduleInfo(std::move(demand));
    
    // Create empty ResourceViewManager
    auto resourceViewManager = std::make_unique<ResourceViewManager>(maxResNum);
    
    // Load resource view
    policy.LoadResourceView(resourceViewManager, DIGSReqStage::PREFILL);
    
    // Call SelectPrefillInst
    uint64_t prefillId = 0;
    uint64_t groupId = 0;
    int32_t result = policy.SelectPrefillInst(request, prefillId, groupId);
    
    // Verify results
    EXPECT_EQ(result, static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE));
}

// Test SelectDecodeInst method - successfully select instance
TEST_F(TestStaticAllocPolicy, TestSelectDecodeInstSuccess)
{
    size_t maxResNum = 100;
    StaticAllocPolicy policy(maxResNum);
    
    // Create test request
    std::string reqId = "test_req_1";
    size_t inputLen = 100;
    auto request = std::make_shared<DIGSRequest>(reqId, inputLen);
    
    // Create resource demand
    std::vector<uint64_t> demandAttrs = {10, 10};
    auto demand = std::make_unique<MetaResource>(std::move(demandAttrs));
    request->InitScheduleInfo(std::move(demand));
    
    // Create ResourceViewManager and add resources
    auto resourceViewManager = std::make_unique<ResourceViewManager>(maxResNum);
    
    DIGSInstanceStaticInfo staticInfo;
    staticInfo.id = 2;
    staticInfo.groupId = 1;
    staticInfo.label = DIGSInstanceLabel::DECODE_STATIC;
    staticInfo.totalSlotsNum = 100;
    staticInfo.totalBlockNum = 100;
    
    auto resourceInfo = std::make_shared<ResourceInfo>(staticInfo, 1.0);
    DIGSInstanceDynamicInfo dynamicInfo;
    dynamicInfo.peers.push_back(1);
    auto resourceLoad = std::make_shared<ResourceLoad>(dynamicInfo);
    
    ResourceViewManager::ResInfo resInfo = std::make_pair(resourceInfo, resourceLoad);
    resourceViewManager->AddResInfo(resInfo);
    
    // Load resource view
    policy.LoadResourceView(resourceViewManager, DIGSReqStage::DECODE);
    
    // Call SelectDecodeInst
    uint64_t prefillId = 1;
    uint64_t groupId = 1;
    uint64_t decodeId = 0;
    int32_t result = policy.SelectDecodeInst(request, prefillId, groupId, decodeId);
    
    // Verify results
    EXPECT_EQ(result, static_cast<int32_t>(common::Status::OK));
    EXPECT_EQ(decodeId, staticInfo.id);
}

// Test SelectDecodeInst method - no available resources
TEST_F(TestStaticAllocPolicy, TestSelectDecodeInstNoResource)
{
    size_t maxResNum = 100;
    StaticAllocPolicy policy(maxResNum);
    
    // Create test request
    std::string reqId = "test_req_1";
    size_t inputLen = 100;
    auto request = std::make_shared<DIGSRequest>(reqId, inputLen);
    
    // Create resource demand
    std::vector<uint64_t> demandAttrs = {10, 10};
    auto demand = std::make_unique<MetaResource>(std::move(demandAttrs));
    request->InitScheduleInfo(std::move(demand));
    
    // Create empty ResourceViewManager
    auto resourceViewManager = std::make_unique<ResourceViewManager>(maxResNum);
    
    // Load resource view
    policy.LoadResourceView(resourceViewManager, DIGSReqStage::DECODE);
    
    // Call SelectDecodeInst
    uint64_t prefillId = 1;
    uint64_t groupId = 1;
    uint64_t decodeId = 0;
    int32_t result = policy.SelectDecodeInst(request, prefillId, groupId, decodeId);
    
    // Verify results
    EXPECT_EQ(result, static_cast<int32_t>(common::Status::NO_SATISFIED_RESOURCE));
}

// Test OffloadResourceView method
TEST_F(TestStaticAllocPolicy, TestOffloadResourceView)
{
    size_t maxResNum = 100;
    StaticAllocPolicy policy(maxResNum);
    
    // Create ResourceViewManager and add resources
    auto resourceViewManager = std::make_unique<ResourceViewManager>(maxResNum);
    
    // Add prefill resource
    DIGSInstanceStaticInfo staticInfoPrefill;
    staticInfoPrefill.id = 1;
    staticInfoPrefill.groupId = 1;
    staticInfoPrefill.label = DIGSInstanceLabel::PREFILL_STATIC;
    staticInfoPrefill.totalSlotsNum = 100;
    staticInfoPrefill.totalBlockNum = 100;
    
    auto resourceInfoPrefill = std::make_shared<ResourceInfo>(staticInfoPrefill, 1.0);
    DIGSInstanceDynamicInfo dynamicInfoPrefill;
    auto resourceLoadPrefill = std::make_shared<ResourceLoad>(dynamicInfoPrefill);
    
    ResourceViewManager::ResInfo resInfoPrefill = std::make_pair(resourceInfoPrefill, resourceLoadPrefill);
    resourceViewManager->AddResInfo(resInfoPrefill);
    
    // Add decode resource
    DIGSInstanceStaticInfo staticInfoDecode;
    staticInfoDecode.id = 2;
    staticInfoDecode.groupId = 1;
    staticInfoDecode.label = DIGSInstanceLabel::DECODE_STATIC;
    staticInfoDecode.totalSlotsNum = 100;
    staticInfoDecode.totalBlockNum = 100;
    
    auto resourceInfoDecode = std::make_shared<ResourceInfo>(staticInfoDecode, 1.0);
    DIGSInstanceDynamicInfo dynamicInfoDecode;
    auto resourceLoadDecode = std::make_shared<ResourceLoad>(dynamicInfoDecode);
    
    ResourceViewManager::ResInfo resInfoDecode = std::make_pair(resourceInfoDecode, resourceLoadDecode);
    resourceViewManager->AddResInfo(resInfoDecode);
    
    // Load resource view
    policy.LoadResourceView(resourceViewManager, DIGSReqStage::PREFILL);
    policy.LoadResourceView(resourceViewManager, DIGSReqStage::DECODE);
    
    // Verify resource view is loaded
    EXPECT_GT(policy.staticPrefillPool_.size(), 0);
    EXPECT_GT(policy.staticDecodePool_.size(), 0);
    EXPECT_EQ(policy.staticDecodePool_.begin()->first, staticInfoDecode.groupId);
    
    // Call OffloadResourceView
    policy.OffloadResourceView();
    
    // Verify resource view is cleared
    EXPECT_EQ(policy.staticPrefillPool_.size(), 0);
    EXPECT_EQ(policy.staticDecodePool_.size(), 0);
}