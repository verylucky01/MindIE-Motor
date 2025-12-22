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
#include <thread>
#include <mutex>
#include "gtest/gtest.h"
#include "coordinator/digs_scheduler/digs_scheduler_impl.h"

class TestDIGSSchedulerImpl : public testing::Test {
protected:
    void SetUp() override
    {
        InitConfig(config_);
    }

    void TearDown() override
    {
    }

    void InitConfig(MINDIE::MS::DIGSScheduler::Config& config)
    {
        config["max_schedule_count"] = "1000";
        config["reordering_type"] = "1";
        config["select_type"] = "1";
        config["alloc_file"] = "";
        config["pool_type"] = "1";
        config["block_size"] = "128";
        config["load_score_limit"] = "1000, 100";
        config["affinity_score_limit"] = "1000, 50";

        config["export_type"] = "0";
        config["pull_request_timeout"] = "500";

        config["max_res_num"] = "5000";
        config["res_view_update_timeout"] = "500";
        config["res_limit_rate"] = "1.1";
        config["metaResource_names"] = "";
        config["load_cost_values"] = "1, 1";
    }

protected:
    std::map<std::string, std::string> config_;
};

TEST_F(TestDIGSSchedulerImpl, TestCreateFuncModelParams)
{
    size_t blockSize = 128;
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    std::vector<MINDIE::MS::DIGSInstanceScheduleInfo> schedulerInfo;
    scheduler->QueryInstanceScheduleInfo(schedulerInfo);

    scheduler->CloseInstance({0});

    scheduler->ActivateInstance({0});

    scheduler->ActivateInstance({0});

    scheduler->SetBlockSize(blockSize);

    std::string reqId = "test0";
    std::string prompt = "I am a teacher";
    auto status = scheduler->ProcReq(reqId, prompt, MINDIE::MS::ReqType::OPENAI);
    EXPECT_EQ(status, 0);
}

// Test scheduler constructor and destructor
TEST_F(TestDIGSSchedulerImpl, TestConstructorAndDestructor)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    ASSERT_NE(scheduler, nullptr);
}

// Test instance registration functionality
TEST_F(TestDIGSSchedulerImpl, TestRegisterInstance)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> instances;
    MINDIE::MS::DIGSInstanceStaticInfo instanceInfo;
    instanceInfo.id = 1;
    instanceInfo.groupId = 1;
    instanceInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
    instanceInfo.totalSlotsNum = 100;
    instanceInfo.totalBlockNum = 100;
    instances.push_back(instanceInfo);
    
    int32_t result = scheduler->RegisterInstance(instances);
    EXPECT_EQ(result, 0);
}

// Test instance update functionality
TEST_F(TestDIGSSchedulerImpl, TestUpdateInstance)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> staticInstances;
    MINDIE::MS::DIGSInstanceStaticInfo staticInfo;
    staticInfo.id = 1;
    staticInfo.groupId = 1;
    staticInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
    staticInfo.totalSlotsNum = 100;
    staticInfo.totalBlockNum = 100;
    staticInstances.push_back(staticInfo);
    scheduler->RegisterInstance(staticInstances);
    
    std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> dynamicInstances;
    MINDIE::MS::DIGSInstanceDynamicInfo dynamicInfo;
    dynamicInfo.id = 1;
    dynamicInfo.peers = {2, 3};
    dynamicInstances.push_back(dynamicInfo);
    
    int32_t result = scheduler->UpdateInstance(dynamicInstances);
    EXPECT_EQ(result, 0);
}

// Test instance closing functionality
TEST_F(TestDIGSSchedulerImpl, TestCloseInstance)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> instances;
    MINDIE::MS::DIGSInstanceStaticInfo instanceInfo;
    instanceInfo.id = 1;
    instanceInfo.groupId = 1;
    instanceInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
    instanceInfo.totalSlotsNum = 100;
    instanceInfo.totalBlockNum = 100;
    instances.push_back(instanceInfo);
    scheduler->RegisterInstance(instances);
    
    std::vector<uint64_t> closeInstances = {1};
    int32_t result = scheduler->CloseInstance(closeInstances);
    EXPECT_EQ(result, 0);
}

// Test instance activation functionality
TEST_F(TestDIGSSchedulerImpl, TestActivateInstance)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> instances;
    MINDIE::MS::DIGSInstanceStaticInfo instanceInfo;
    instanceInfo.id = 1;
    instanceInfo.groupId = 1;
    instanceInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
    instanceInfo.totalSlotsNum = 100;
    instanceInfo.totalBlockNum = 100;
    instances.push_back(instanceInfo);
    scheduler->RegisterInstance(instances);
    scheduler->CloseInstance({1});
    
    std::vector<uint64_t> activateInstances = {1};
    int32_t result = scheduler->ActivateInstance(activateInstances);
    EXPECT_EQ(result, 0);
}

// Test instance removal functionality
TEST_F(TestDIGSSchedulerImpl, TestRemoveInstance)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> instances;
    MINDIE::MS::DIGSInstanceStaticInfo instanceInfo;
    instanceInfo.id = 1;
    instanceInfo.groupId = 1;
    instanceInfo.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
    instanceInfo.totalSlotsNum = 100;
    instanceInfo.totalBlockNum = 100;
    instances.push_back(instanceInfo);
    scheduler->RegisterInstance(instances);
    
    std::vector<uint64_t> removeInstances = {1};
    int32_t result = scheduler->RemoveInstance(removeInstances);
    EXPECT_EQ(result, 0);
}

// Test ProcReq string version
TEST_F(TestDIGSSchedulerImpl, TestProcReqWithString)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::string reqId = "test_req_1";
    std::string prompt = "This is a test prompt";
    
    int32_t result = scheduler->ProcReq(reqId, prompt, MINDIE::MS::ReqType::OPENAI);
    EXPECT_EQ(result, 0);
}

// Test ProcReq length version
TEST_F(TestDIGSSchedulerImpl, TestProcReqWithLength)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::string reqId = "test_req_2";
    size_t promptLen = 100;
    
    int32_t result = scheduler->ProcReq(reqId, promptLen, MINDIE::MS::ReqType::OPENAI);
    EXPECT_EQ(result, 0);
}

// Test ProcReq token list version
TEST_F(TestDIGSSchedulerImpl, TestProcReqWithTokenList)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::string reqId = "test_req_3";
    std::vector<uint32_t> tokenList = {1, 2, 3, 4, 5};
    
    int32_t result = scheduler->ProcReq(reqId, tokenList);
    EXPECT_EQ(result, 0);
}

// Test request query functionality
TEST_F(TestDIGSSchedulerImpl, TestQueryRequestSummary)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    MINDIE::MS::DIGSRequestSummary summary;
    
    int32_t result = scheduler->QueryRequestSummary(summary);
    EXPECT_EQ(result, 0);
}

// Test schedule information query functionality
TEST_F(TestDIGSSchedulerImpl, TestQueryInstanceScheduleInfo)
{
    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler = std::make_unique<MINDIE::MS::DIGSSchedulerImpl>(config_);
    
    std::vector<MINDIE::MS::DIGSInstanceScheduleInfo> schedulerInfo;
    
    int32_t result = scheduler->QueryInstanceScheduleInfo(schedulerInfo);
    EXPECT_EQ(result, 0);
}