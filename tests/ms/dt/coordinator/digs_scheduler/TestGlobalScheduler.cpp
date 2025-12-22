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
#include "coordinator/request/request_manager.h"
#include "coordinator/resource/resource_manager.h"
#include "coordinator/digs_scheduler/global_scheduler.h"
#define private public
#define protected public

using namespace MINDIE::MS;

class TestGlobalScheduler : public testing::Test {
protected:
    void SetUp() override
    {
        InitConfig(config_);
    }

    void TearDown() override
    {
    }

    void InitConfig(std::map<std::string, std::string>& config)
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
        config["load_cost_coefficient"] = "0, 1, 1024, 24, 6, 0, 0, 1, 1";
    }

    void GenerateScheduleConfig(MINDIE::MS::SchedulerConfig& schedConfig)
    {
        size_t ulItem;
        int32_t iItem;

        if (MINDIE::MS::common::GetConfig("max_schedule_count", ulItem, config_)) {
            schedConfig.maxScheduleCount = ulItem;
        }

        if (MINDIE::MS::common::GetConfig("reordering_type", iItem, config_)) {
            schedConfig.reorderingType = iItem;
        }

        if (MINDIE::MS::common::GetConfig("select_type", iItem, config_)) {
            schedConfig.selectType = iItem;
        }

        if (MINDIE::MS::common::GetConfig("pool_type", iItem, config_)) {
            schedConfig.poolType = iItem;
        }

        if (MINDIE::MS::common::GetConfig("block_size", ulItem, config_)) {
            schedConfig.blockSize = ulItem;
        }
    }

    void GenerateRequestConfig(MINDIE::MS::RequestConfig& requestConfig)
    {
        uint32_t ulItem;
        size_t sizetItem;

        if (MINDIE::MS::common::GetConfig("pull_request_timeout", ulItem, config_)) {
            requestConfig.pullRequestTimeout = ulItem;
        }

        if (MINDIE::MS::common::GetConfig("max_summary_count", sizetItem, config_)) {
            requestConfig.maxSummaryCount = sizetItem;
        }
    }

    void GenerateResourceConfig(MINDIE::MS::ResourceConfig& resourceConfig)
    {
        std::string item;
        size_t ulItem;
        double dItem;

        if (MINDIE::MS::common::GetConfig("max_res_num", ulItem, config_)) {
            resourceConfig.maxResNum = ulItem;
        }

        if (MINDIE::MS::common::GetConfig("res_view_update_timeout", ulItem, config_)) {
            resourceConfig.resViewUpdateTimeout = ulItem;
        }

        if (MINDIE::MS::common::GetConfig("res_limit_rate", dItem, config_)) {
            resourceConfig.resLimitRate = dItem;
        }

        if (MINDIE::MS::common::GetConfig("metaResource_names", item, config_)) {
            resourceConfig.metaResourceNames = item;
        }

        if (MINDIE::MS::common::GetConfig("load_cost_values", item, config_)) {
            resourceConfig.metaResourceValues = item;
        }

        if (MINDIE::MS::common::GetConfig("dynamic_max_res", item, config_)) {
            MINDIE::MS::common::Str2Bool(item, resourceConfig.dynamicMaxResEnable);
        }

        if (MINDIE::MS::common::GetConfig("max_dynamic_res_rate_count", ulItem, config_)) {
            resourceConfig.maxDynamicResRateCount = ulItem;
        }

        if (MINDIE::MS::common::GetConfig("dynamic_res_rate_unit", dItem, config_)) {
            resourceConfig.dynamicResRateUnit = dItem;
        }
    }

protected:
    std::map<std::string, std::string> config_;
    std::unique_ptr<MINDIE::MS::GlobalScheduler> globalScheduler_;
    std::shared_ptr<MINDIE::MS::RequestManager> requestManager_;
    std::shared_ptr<MINDIE::MS::ResourceManager> resourceManager_;
};

TEST_F(TestGlobalScheduler, TestScheduler)
{
    size_t sleepTime = 50;
    SchedulerConfig schedConfig = {};
    GenerateScheduleConfig(schedConfig);
    common::SetBlockSize(schedConfig.blockSize);
    RequestConfig requestConfig = {};
    GenerateRequestConfig(requestConfig);
    ResourceConfig resourceConfig = {};
    GenerateResourceConfig(resourceConfig);
    RequestManager::Create(requestManager_, requestConfig);
    ResourceManager::Create(resourceManager_, resourceConfig);
    auto status = GlobalScheduler::Create(schedConfig, resourceManager_, requestManager_, globalScheduler_);
    EXPECT_EQ(status, 0);
    globalScheduler_->RegisterNotifyAllocation([this](const std::unique_ptr<ReqScheduleInfo>& info) -> int32_t {
            return -1;
        });
    // 验证Scheduling状态的请求会不会被放回到schedulingReqs_
    std::string reqId1 = "test0";
    auto emptyReq = std::make_shared<DIGSRequest>(reqId1, 0);
    emptyReq->UpdateState2Scheduling();
    globalScheduler_->notifyQueue_.Push(std::move(emptyReq));
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    EXPECT_EQ(globalScheduler_->schedulingReqs_.size(), 1);
    std::string reqId2 = "test1";
    auto alReq = std::make_shared<DIGSRequest>(reqId2, 0);
    alReq->UpdateState2Scheduling();
    alReq->UpdateState2Allocated();
    globalScheduler_->notifyQueue_.Push(std::move(alReq));
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    std::string reqId3 = "test2";
    auto req3 = std::make_shared<DIGSRequest>(reqId3, 0);
    req3->UpdateState2Scheduling();
    req3->UpdateState2Allocated();
    req3->UpdateState2PrefillEnd(0);
    globalScheduler_->notifyQueue_.Push(std::move(req3));
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    globalScheduler_->Stop();
}
