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
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "HttpClientAsync.h"
#include "ConnectionPool.h"

using namespace testing;
using namespace MINDIE::MS;

class RequestRepeater {
public:
    ~RequestRepeater() = default;

    virtual bool CheckLinkWithDNode(boost::beast::string_view ip, boost::beast::string_view port)
    {
        auto connIds = httpClient.FindId(std::string(ip), std::string(port));
        for (auto &connId : connIds) {
            std::shared_ptr<MINDIE::MS::ClientConnection> conn = httpClient.GetConnection(connId);
            if (conn != nullptr) {
                continue;
            } else {
                return true;
            }
        }
        return false;
    };

    virtual MINDIE::MS::HttpClientAsync& GetHttpClientAsync()
    {
        return httpClient;
    };

    MINDIE::MS::HttpClientAsync httpClient;
};


class MockDRequestRepeater : public RequestRepeater {
public:
    MOCK_METHOD(bool, CheckLinkWithDNode, (boost::beast::string_view ip, boost::beast::string_view port), (override));
    MOCK_METHOD(MINDIE::MS::HttpClientAsync&, GetHttpClientAsync, (), (override));
};

class RequestRepeaterTest : public Test {
protected:
    void SetUp() override
    {
        auto mock = std::make_shared<MockDRequestRepeater>();
        mockPtr = mock.get();
        repeater = std::move(mock);
    }

    std::shared_ptr<RequestRepeater> repeater;
    MockDRequestRepeater* mockPtr;
};

TEST_F(RequestRepeaterTest, CheckLinkWithDNode)
{
    EXPECT_CALL(*this->mockPtr, CheckLinkWithDNode(_, _)).WillOnce(Return(false));
    EXPECT_FALSE(repeater->CheckLinkWithDNode("192.168.1.1", "8080"));
}

TEST_F(RequestRepeaterTest, CheckLinkNoConnection)
{
    EXPECT_CALL(*this->mockPtr, CheckLinkWithDNode(_, _)).WillOnce(Return(true));
    EXPECT_TRUE(repeater->CheckLinkWithDNode("192.168.1.1", "8080"));
}

TEST_F(RequestRepeaterTest, GetHttpClientReturnsCorrectReference)
{
    EXPECT_CALL(*this->mockPtr, GetHttpClientAsync()).WillOnce(ReturnRef(mockPtr->httpClient));
    MINDIE::MS::HttpClientAsync& ref = repeater->GetHttpClientAsync();
    EXPECT_EQ(&ref, &mockPtr->httpClient);
}