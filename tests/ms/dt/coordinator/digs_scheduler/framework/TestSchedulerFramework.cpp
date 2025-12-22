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
#include "coordinator/digs_scheduler/framework/schedule_framework.h"
#include "coordinator/digs_scheduler/poolpolicy/static_pool_policy.h"
#include "coordinator/digs_scheduler/selectpolicy/load_balance_policy.h"
#include "coordinator/digs_scheduler/selectpolicy/static_alloc_policy.h"
#include "coordinator/digs_scheduler/reorderingpolicy/fcfs_policy.h"
#include "coordinator/digs_scheduler/reorderingpolicy/sljf_policy.h"
#include "coordinator/digs_scheduler/reorderingpolicy/mprf_policy.h"
#define private public
#define protected public

using namespace MINDIE::MS;

class TestSchedulerFramework : public testing::Test {
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
        config["reordering_type"] = "5";
        config["select_type"] = "5";
        config["alloc_file"] = "";
        config["pool_type"] = "5";
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

protected:
    std::map<std::string, std::string> config_;
    std::unique_ptr<ScheduleFramework> scheduleFramework_;
};

TEST_F(TestSchedulerFramework, TestSchedulerFramework)
{
    size_t sleepTime = 50;
    size_t maxResNum = 50;
    SchedulerConfig schedConfig = {};
    GenerateScheduleConfig(schedConfig);

    scheduleFramework_ = std::make_unique<ScheduleFramework>(schedConfig, maxResNum);
    EXPECT_EQ(scheduleFramework_->maxResNum_, maxResNum);

    config_["reordering_type"] = "3";
    GenerateScheduleConfig(schedConfig);
    scheduleFramework_ = std::make_unique<ScheduleFramework>(schedConfig, maxResNum);
    EXPECT_EQ(scheduleFramework_->maxResNum_, maxResNum);

    config_["reordering_type"] = "4";
    GenerateScheduleConfig(schedConfig);
    scheduleFramework_ = std::make_unique<ScheduleFramework>(schedConfig, maxResNum);
    EXPECT_EQ(scheduleFramework_->maxResNum_, maxResNum);
}
