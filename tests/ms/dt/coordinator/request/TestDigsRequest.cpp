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
#include "coordinator/request/digs_request.h"

using namespace testing;

class TestDigsRequest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 带参数的DIGSRequest构造函数验证
TEST_F(TestDigsRequest, TestConstructorWithValidInput)
{
    std::string reqId = "testReqId";
    size_t inputLen = 10;
    MINDIE::MS::DIGSRequest req(reqId, inputLen);
    EXPECT_EQ(req.inputLen_, inputLen);
    EXPECT_EQ(req.outputLen_, 0);
    EXPECT_EQ(req.scheduleInfo_, nullptr);
    EXPECT_EQ(req.state_, MINDIE::MS::DIGSReqState::WAITING);
    EXPECT_EQ(req.scheduleTime_, 0);
    EXPECT_EQ(req.startTime_, 0);
    EXPECT_EQ(req.prefillEndTime_, 0);
    EXPECT_EQ(req.decodeEndTime_, 0);
    EXPECT_EQ(req.prefillCostTime_, 0);
}

// 不带参数的DIGSRequest构造函数验证
TEST_F(TestDigsRequest, DIGSRequest_ShouldConstructWithInvalidId_WhenConstructed)
{
    MINDIE::MS::DIGSRequest digsRequest;
    EXPECT_EQ(digsRequest.reqId_, "invalid_ID");
    EXPECT_EQ(digsRequest.inputLen_, 0);
    EXPECT_EQ(digsRequest.outputLen_, 0);
    EXPECT_EQ(digsRequest.scheduleInfo_, nullptr);
    EXPECT_EQ(digsRequest.state_, MINDIE::MS::DIGSReqState::INVALID);
    EXPECT_EQ(digsRequest.scheduleTime_, 0);
    EXPECT_EQ(digsRequest.startTime_, 0);
    EXPECT_EQ(digsRequest.prefillEndTime_, 0);
    EXPECT_EQ(digsRequest.decodeEndTime_, 0);
    EXPECT_EQ(digsRequest.prefillCostTime_, 0);
}

/**
 * @tc.name  : InitScheduleInfo_ShouldSetScheduleInfo_WhenDemandIsNotNull
 * @tc.number: DIGSRequestTest_001
 * @tc.desc  : Test when demand is not null then scheduleInfo_ should be set
 */
TEST_F(TestDigsRequest, InitScheduleInfo_ShouldSetScheduleInfo_WhenDemandIsNotNull)
{
    // Arrange
    MINDIE::MS::DIGSRequest digsRequest;
    std::unique_ptr<MINDIE::MS::MetaResource> demand = std::make_unique<MINDIE::MS::MetaResource>();

    // Act
    digsRequest.InitScheduleInfo(std::move(demand));

    // Assert
    ASSERT_TRUE(digsRequest.scheduleInfo_ != nullptr);
}

/**
 * @tc.name  : InitScheduleInfo_ShouldSetScheduleTime_WhenDemandIsNotNull
 * @tc.number: DIGSRequestTest_002
 * @tc.desc  : Test when demand is not null then scheduleTime_ should be set
 */
TEST_F(TestDigsRequest, InitScheduleInfo_ShouldSetScheduleTime_WhenDemandIsNotNull)
{
    // Arrange
    MINDIE::MS::DIGSRequest digsRequest;
    std::unique_ptr<MINDIE::MS::MetaResource> demand = std::make_unique<MINDIE::MS::MetaResource>();

    // Act
    digsRequest.InitScheduleInfo(std::move(demand));

    // Assert
    ASSERT_TRUE(digsRequest.scheduleTime_ != 0);
}

/**
 * @tc.name  : updateState2PrefillEnd_ShouldReturnOK_WhenStateIsAllocated
 * @tc.number: DIGSRequestTest_001
 * @tc.desc  : Test when state is DIGSReqState::ALLOCATED, UpdateState2PrefillEnd should return OK
 */
TEST_F(TestDigsRequest, updateState2PrefillEnd_ShouldReturnOK_WhenStateIsAllocated)
{
    MINDIE::MS::DIGSRequest request;
    request.state_.store(MINDIE::MS::DIGSReqState::ALLOCATED, std::memory_order_release);
    uint64_t prefillEndTime = 123456789;
    int32_t result = request.UpdateState2PrefillEnd(prefillEndTime);
    EXPECT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
}

/**
 * @tc.name  : updateState2PrefillEnd_ShouldReturnStateError_WhenStateIsNotAllocated
 * @tc.number: DIGSRequestTest_002
 * @tc.desc  : Test when state is not DIGSReqState::ALLOCATED, UpdateState2PrefillEnd should return STATE_ERROR
 */
TEST_F(TestDigsRequest, updateState2PrefillEnd_ShouldReturnStateError_WhenStateIsNotAllocated)
{
    MINDIE::MS::DIGSRequest request;
    request.state_.store(MINDIE::MS::DIGSReqState::PREFILL_END, std::memory_order_release);
    uint64_t prefillEndTime = 123456789;
    int32_t result = request.UpdateState2PrefillEnd(prefillEndTime);
    EXPECT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::STATE_ERROR));
}

/**
 * @tc.name  : updateState2DecodeEnd_ShouldUpdateState_WhenStateIsPrefillEndOrAllocated
 * @tc.number: updateState2DecodeEnd_Test_001
 * @tc.desc  : Test when state is DIGSReqState::PREFILL_END or DIGSReqState::ALLOCATED,
               it should update state to DIGSReqState::DECODE_END
 */
TEST_F(TestDigsRequest, updateState2DecodeEnd_ShouldUpdateState_WhenStateIsPrefillEndOrAllocated)
{
    MINDIE::MS::DIGSRequest digsRequest;
    digsRequest.state_.store(MINDIE::MS::DIGSReqState::PREFILL_END);
    uint64_t prefillEndTime = 100;
    uint64_t decodeEndTime = 200;
    size_t outputLength = 1000;
    int32_t result = digsRequest.UpdateState2DecodeEnd(prefillEndTime, decodeEndTime, outputLength);
    EXPECT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
    EXPECT_EQ(digsRequest.state_.load(), MINDIE::MS::DIGSReqState::DECODE_END);
}

/**
 * @tc.name  : updateState2DecodeEnd_ShouldUpdateState_WhenStateIsScheduling
 * @tc.number: updateState2DecodeEnd_Test_002
 * @tc.desc  : Test when state is DIGSReqState::SCHEDULING, it should update state to DIGSReqState::DECODE_END
 */
TEST_F(TestDigsRequest, updateState2DecodeEnd_ShouldUpdateState_WhenStateIsScheduling)
{
    MINDIE::MS::DIGSRequest digsRequest;
    digsRequest.state_.store(MINDIE::MS::DIGSReqState::SCHEDULING);
    uint64_t prefillEndTime = 100;
    uint64_t decodeEndTime = 200;
    size_t outputLength = 1000;
    int32_t result = digsRequest.UpdateState2DecodeEnd(prefillEndTime, decodeEndTime, outputLength);
    EXPECT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
    EXPECT_EQ(digsRequest.state_.load(), MINDIE::MS::DIGSReqState::DECODE_END);
}

/**
 * @tc.name  : updateState2DecodeEnd_ShouldReturnError_WhenStateIsNotPrefillEndOrAllocatedOrScheduling
 * @tc.number: updateState2DecodeEnd_Test_003
 * @tc.desc  : Test when state is not DIGSReqState::PREFILL_END, DIGSReqState::ALLOCATED or DIGSReqState::SCHEDULING,
               it should return STATE_ERROR
 */
TEST_F(TestDigsRequest, updateState2DecodeEnd_ShouldReturnError_WhenStateIsNotPrefillEndOrAllocatedOrScheduling)
{
    MINDIE::MS::DIGSRequest digsRequest;
    digsRequest.state_.store(MINDIE::MS::DIGSReqState::WAITING);
    uint64_t prefillEndTime = 100;
    uint64_t decodeEndTime = 200;
    size_t outputLength = 1000;
    int32_t result = digsRequest.UpdateState2DecodeEnd(prefillEndTime, decodeEndTime, outputLength);
    EXPECT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::STATE_ERROR));
}

/**
* @tc.name  : update_state_to_invalid_001
* @tc.number: update_state_to_invalid_Test_001
* @tc.desc  : Test when state is WAITING then state is updated to INVALID and return OK
*/
TEST_F(TestDigsRequest, update_state_to_invalid_001)
{
    MINDIE::MS::DIGSRequest request;
    request.state_.store(MINDIE::MS::DIGSReqState::WAITING, std::memory_order_release);
    int32_t result = request.UpdateState2Invalid();
    EXPECT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
    EXPECT_EQ(request.state_.load(std::memory_order_acquire), MINDIE::MS::DIGSReqState::INVALID);
}

/**
* @tc.name  : update_state_to_invalid_002
* @tc.number: update_state_to_invalid_Test_002
* @tc.desc  : Test when state is not WAITING then state is not updated to INVALID and return STATE_ERROR
*/
TEST_F(TestDigsRequest, update_state_to_invalid_002)
{
    MINDIE::MS::DIGSRequest request;
    request.state_.store(MINDIE::MS::DIGSReqState::INVALID, std::memory_order_release);
    int32_t result = request.UpdateState2Invalid();
    EXPECT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::STATE_ERROR));
    EXPECT_EQ(request.state_.load(std::memory_order_acquire), MINDIE::MS::DIGSReqState::INVALID);
}

/**
* @tc.name  : updateState2Scheduling_ShouldReturnOK_WhenStateIsWaiting
* @tc.number: DIGSRequestTest_001
* @tc.desc  : Test when state is WAITING then UpdateState2Scheduling returns OK
*/
TEST_F(TestDigsRequest, updateState2Scheduling_ShouldReturnOK_WhenStateIsWaiting)
{
    MINDIE::MS::DIGSRequest request;
    request.state_.store(MINDIE::MS::DIGSReqState::WAITING, std::memory_order_release);
    int32_t result = request.UpdateState2Scheduling();
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
}

/**
* @tc.name  : updateState2Scheduling_ShouldReturnOK_WhenStateIsAllocated
* @tc.number: DIGSRequestTest_002
* @tc.desc  : Test when state is ALLOCATED then UpdateState2Scheduling returns OK
*/
TEST_F(TestDigsRequest, updateState2Scheduling_ShouldReturnOK_WhenStateIsAllocated)
{
    MINDIE::MS::DIGSRequest request;
    request.state_.store(MINDIE::MS::DIGSReqState::ALLOCATED, std::memory_order_release);
    int32_t result = request.UpdateState2Scheduling();
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
}

/**
* @tc.name  : updateState2Scheduling_ShouldReturnStateError_WhenStateIsNotWaitingOrAllocated
* @tc.number: DIGSRequestTest_003
* @tc.desc  : Test when state is not WAITING or ALLOCATED then UpdateState2Scheduling returns STATE_ERROR
*/
TEST_F(TestDigsRequest, updateState2Scheduling_ShouldReturnStateError_WhenStateIsNotWaitingOrAllocated)
{
    MINDIE::MS::DIGSRequest request;
    request.state_.store(MINDIE::MS::DIGSReqState::SCHEDULING, std::memory_order_release);
    int32_t result = request.UpdateState2Scheduling();
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::STATE_ERROR));
}

/**
* @tc.name  : updateState2Allocated_ShouldReturnOK_WhenStateIsScheduling
* @tc.number: DIGSRequestTest_001
* @tc.desc  : Test when state is SCHEDULING then UpdateState2Allocated returns OK
*/
TEST_F(TestDigsRequest, updateState2Allocated_ShouldReturnOK_WhenStateIsScheduling)
{
    MINDIE::MS::DIGSRequest digsRequest;
    digsRequest.state_ = MINDIE::MS::DIGSReqState::SCHEDULING;
    int32_t result = digsRequest.UpdateState2Allocated();
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::OK));
}

/**
* @tc.name  : updateState2Allocated_ShouldReturnStateError_WhenStateIsNotScheduling
* @tc.number: DIGSRequestTest_002
* @tc.desc  : Test when state is not SCHEDULING then UpdateState2Allocated returns STATE_ERROR
*/
TEST_F(TestDigsRequest, updateState2Allocated_ShouldReturnStateError_WhenStateIsNotScheduling)
{
    MINDIE::MS::DIGSRequest digsRequest;
    digsRequest.state_ = MINDIE::MS::DIGSReqState::ALLOCATED;
    int32_t result = digsRequest.UpdateState2Allocated();
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::STATE_ERROR));
}

/**
* @tc.name  : getRequestControlCallback_ShouldReturnUpdateState2PrefillEnd_WhenRequestOperationIsREQUEST_UPDATE
* @tc.number: getRequestControlCallback_Test_001
* @tc.desc  : Test when DIGSReqOperation is REQUEST_UPDATE then return UpdateState2PrefillEnd
*/
TEST_F(TestDigsRequest, getRequestControlCallback_WhenRequestOperationIsREQUEST_UPDATE)
{
    MINDIE::MS::DIGSRequest digsRequest;
    MINDIE::MS::DIGSRequest::ControlCallback callback;
    digsRequest.GetRequestControlCallback(callback);
    int32_t prefillEndTime = 100;
    int32_t decodeEndTime = 200;
    size_t outputLength = 10;
    int32_t result = callback(
        MINDIE::MS::DIGSReqOperation::REQUEST_UPDATE, prefillEndTime,
        decodeEndTime,
        outputLength);
    ASSERT_EQ(result, digsRequest.UpdateState2PrefillEnd(prefillEndTime));
}

/**
* @tc.name  : getRequestControlCallback_ShouldReturnUpdateState2DecodeEnd_WhenRequestOperationIsREQUEST_REMOVE
* @tc.number: getRequestControlCallback_Test_002
* @tc.desc  : Test when DIGSReqOperation is REQUEST_REMOVE then return UpdateState2DecodeEnd
*/
TEST_F(TestDigsRequest, getRequestControlCallback_WhenRequestOperationIsREQUEST_REMOVE)
{
    MINDIE::MS::DIGSRequest digsRequest;
    MINDIE::MS::DIGSRequest::ControlCallback callback;
    digsRequest.GetRequestControlCallback(callback);
    int32_t prefillEndTime = 100;
    int32_t decodeEndTime = 200;
    size_t outputLength = 10;
    int32_t result = callback(
        MINDIE::MS::DIGSReqOperation::REQUEST_REMOVE,
        prefillEndTime,
        decodeEndTime,
        outputLength);
    ASSERT_EQ(result, digsRequest.UpdateState2DecodeEnd(prefillEndTime, decodeEndTime, outputLength));
}

/**
* @tc.name  : getRequestControlCallback_ShouldReturnIllegalParameter_WhenRequestOperationIsOther
* @tc.number: getRequestControlCallback_Test_003
* @tc.desc  : Test when DIGSReqOperation is other then return ILLEGAL_PARAMETER
*/
TEST_F(TestDigsRequest, getRequestControlCallback_ShouldReturnIllegalParameter_WhenRequestOperationIsOther)
{
    MINDIE::MS::DIGSRequest digsRequest;
    MINDIE::MS::DIGSRequest::ControlCallback callback;
    digsRequest.GetRequestControlCallback(callback);
    int32_t prefillEndTime = 100;
    int32_t decodeEndTime = 200;
    size_t outputLength = 10;
    int32_t result = callback(
        static_cast<MINDIE::MS::DIGSReqOperation>(3),
        prefillEndTime,
        decodeEndTime,
        outputLength);
    ASSERT_EQ(result, static_cast<int32_t>(MINDIE::MS::common::Status::ILLEGAL_PARAMETER));
}
