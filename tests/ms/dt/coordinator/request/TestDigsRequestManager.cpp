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
#include "coordinator/request/request_manager.h"

using namespace testing;

class TestDigsRequestManager : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @tc.name  : create_ShouldReturnOK_WhenConfigIsValid
 * @tc.number: create_Test_001
 * @tc.desc  : Test when config is valid then Create returns OK
 */
TEST_F(TestDigsRequestManager, create_ShouldReturnOK_WhenConfigIsValid)
{
    std::shared_ptr<MINDIE::MS::RequestManager> reqMgr;
    MINDIE::MS::RequestConfig config;
    int32_t result = MINDIE::MS::RequestManager::Create(reqMgr, config);
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
    ASSERT_NE(reqMgr, nullptr);
}

/**
 * @tc.name  : pullRequest_ShouldReturnOK_WhenQueueIsEmpty
 * @tc.number: pullRequest_Test_001
 * @tc.desc  : Test when the waiting queue is empty, PullRequest should return OK
 */
TEST_F(TestDigsRequestManager, pullRequest_ShouldReturnOK_WhenQueueIsEmpty)
{
    MINDIE::MS::RequestConfig config;
    MINDIE::MS::RequestManager requestManager(config);
    size_t maxReqNum = 10;
    std::deque<std::shared_ptr<MINDIE::MS::DIGSRequest>> waitingReqs;
    int32_t result = requestManager.PullRequest(maxReqNum, waitingReqs);
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
}
