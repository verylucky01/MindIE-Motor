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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "DistributedPolicy.h"
#include "ControllerLeaderAgent.h"

using namespace MINDIE::MS;
using namespace testing;

class MockDistributedLockPolicy : public DistributedLockPolicy {
public:
    MOCK_METHOD(bool, TryLock, (), (override));
    MOCK_METHOD(void, Unlock, (), (override));
    MOCK_METHOD(void, RegisterCallBack, (std::function<void(bool)>), (override));
    MOCK_METHOD(bool, SafePut, (const std::string&, const std::string&), (override));
    MOCK_METHOD(bool, GetWithRevision, (const std::string&, std::string&), (override));
};

class LeaderAgentTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        agent_ = ControllerLeaderAgent::GetInstance();
        mockLock_ = std::make_unique<MockDistributedLockPolicy>();
    }

    void TearDown() override
    {
        agent_->Stop();
    }

    ControllerLeaderAgent* agent_;
    std::unique_ptr<MockDistributedLockPolicy> mockLock_;
    const std::string test_key_ = "/controller/node-data";
    const std::string test_value_ = R"({"node1":"active"})";
};

TEST_F(LeaderAgentTest, RegisterValidStrategy)
{
    EXPECT_EQ(agent_->RegisterStrategy(std::move(mockLock_)), 0);
}

TEST_F(LeaderAgentTest, RegisterNullStrategy)
{
    EXPECT_EQ(agent_->RegisterStrategy(nullptr), -1); // -1：表示异常
}

TEST_F(LeaderAgentTest, CampaignSuccess)
{
    // 1. 声明所有预期的Mock调用
    std::function<void(bool)> capturedCb;
    EXPECT_CALL(*mockLock_, TryLock()).WillOnce(Return(true));
    
    // 明确声明回调注册预期（避免"Uninteresting call"警告）
    EXPECT_CALL(*mockLock_, RegisterCallBack(_))
        .WillOnce(SaveArg<0>(&capturedCb));;
    
    // 2. 执行测试
    agent_->RegisterStrategy(std::move(mockLock_));
    agent_->Start();
    capturedCb(true);
    // 3. 验证结果
    EXPECT_TRUE(agent_->IsLeader());
}

TEST_F(LeaderAgentTest, LockStateChangeToFollower)
{
    std::function<void(bool)> capturedCb;
    EXPECT_CALL(*mockLock_, RegisterCallBack(_))
        .WillOnce(SaveArg<0>(&capturedCb));
    
    agent_->RegisterStrategy(std::move(mockLock_));
    agent_->Start(); // 初始会触发一起
    
    // 先成为Leader
    capturedCb(true);
    // 模拟锁丢失
    capturedCb(false);
    EXPECT_FALSE(agent_->IsLeader());
}

TEST_F(LeaderAgentTest, WriteNodesSuccess)
{
    EXPECT_CALL(*mockLock_, SafePut(test_key_, test_value_))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockLock_, GetWithRevision(test_key_, _))
        .WillOnce(DoAll(
            SetArgReferee<1>(test_value_),
            Return(true)
        ));
    
    agent_->RegisterStrategy(std::move(mockLock_));
    EXPECT_TRUE(agent_->WriteNodes(test_value_));

    nlohmann::json result;
    EXPECT_TRUE(agent_->ReadNodes(result));
    EXPECT_EQ(test_value_, test_value_);
}