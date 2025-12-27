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
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <map>
#include <algorithm>
#include "gtest/gtest.h"
#include "RequestHelper.h"
#include "StressTetsHelper.h"


using namespace MINDIE::MS;

class StressTestSingleNode : public testing::Test {
    void SetUp()
    {
        std::string exePath = GetExecutablePath();
        std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
        std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");
        // change the permission of cert files
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "user_to_coordinator/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_user/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_user/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_user/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_user/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_to_coordinator/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_to_coordinator/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "controller_to_coordinator/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "controller_to_coordinator/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "controller_to_coordinator/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "controller_to_coordinator/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_controller/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_controller/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_controller/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_controller/cert_passwd.txt"), S_IRUSR);
    }
};

// OpenAI固定请求, 激活cache_affinity
static void OpenAIStaticCacheAffinity(const std::string &predictIP, const std::string &predictPort)
{
    std::string response;
    TlsItems tlsItems;
    std::string inferUrl = "/v1/chat/completions";
    std::string outputString = "hello1hello2hello3";
    // 非流式
    // 先发送一条单轮会话
    std::string tiggerReq = CreateOpenAIRequest({"hello1"}, false);
    auto code = SendInferRequest(predictIP, predictPort, tlsItems, inferUrl, tiggerReq, response);
    ASSERT_EQ(code, 0);
    // 再发一条多轮会话
    std::string inferString = CreateOpenAIRequest({"hello1", "hello2", "hello3"}, false);
    code = SendInferRequest(predictIP, predictPort, tlsItems, inferUrl, inferString, response);
    ASSERT_EQ(code, 0);
    ASSERT_EQ(response, outputString);

    // 流式
    tiggerReq = CreateOpenAIRequest({"hello1"}, true);
    code = SendInferRequest(predictIP, predictPort, tlsItems, inferUrl, tiggerReq, response);
    ASSERT_EQ(code, 0);
    // 再发一条多轮会话
    inferString = CreateOpenAIRequest({"hello1", "hello2", "hello3"}, true);
    code = SendInferRequest(predictIP, predictPort, tlsItems, inferUrl, inferString, response);
    ASSERT_EQ(code, 0);
    ASSERT_EQ(ProcessStreamRespond(response), outputString);
}

// metrics请求
static void MetricsRequest(const std::string &manageIP, const std::string &managePort,
    const std::string &metricsStressTestSingleNode)
{
    std::string response;
    TlsItems tlsItems;

    // 发送指标查询请求，默认超时时间60，最大重复请求次数3
    auto ret = GetMetricsRequest(manageIP, managePort, tlsItems, "/metrics", response, false);
    EXPECT_EQ(ret, 200);  // 返回值 200
    EXPECT_EQ(response, metricsStressTestSingleNode);
}

/*
测试描述: 单机场景 openai协议, 5000高并发压测, cache_affinity算法
测试步骤:
    1. 配置部署8个server，预期结果1
    2. 5000并发发送openai请求任务，预期结果2
预期结果:
    1. 部署成功
    2. 推理结果正确
*/
TEST_F(StressTestSingleNode, TestSingleNodeOpenAIStressTC01)
{
    uint32_t mindieServerNum = 8;
    uint32_t numThreadInfer = 5000;
    StressTetsSingleNodeHelper(mindieServerNum, OpenAIStaticCacheAffinity, MetricsRequest, numThreadInfer);
}

// OpenAI固定请求, 激活cache_affinity, 激活round_robin
static void OpenAIStaticRoundRobin(const std::string &predictIP, const std::string &predictPort)
{
    std::string response;
    TlsItems tlsItems;
    std::string inferUrl = "/v1/chat/completions";
    std::string outputString = "My name is Olivier and IOK";
    // 非流式
    std::string inferString = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    auto code = SendInferRequest(predictIP, predictPort, tlsItems, inferUrl, inferString, response);
    ASSERT_EQ(code, 0);
    ASSERT_EQ(response, outputString);

    // 流式
    inferString = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, true);
    code = SendInferRequest(predictIP, predictPort, tlsItems, inferUrl, inferString, response);
    ASSERT_EQ(code, 0);
    ASSERT_EQ(ProcessStreamRespond(response), outputString);
}

/*
测试描述: 单机场景 openai协议, 5000高并发压测, round_robin算法
测试步骤:
    1. 配置部署8个server，预期结果1
    2. 5000并发发送openai请求任务，预期结果2
预期结果:
    1. 部署成功
    2. 推理结果正确
*/
TEST_F(StressTestSingleNode, TestSingleNodeOpenAIStressTC02)
{
    uint32_t mindieServerNum = 8;
    uint32_t numThreadInfer = 5000;
    StressTetsSingleNodeHelper(mindieServerNum, OpenAIStaticRoundRobin, MetricsRequest, numThreadInfer);
}