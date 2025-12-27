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
#include <atomic>
#include "gtest/gtest.h"
#include "RequestHelper.h"
#include "StressTetsHelper.h"
#include "HttpClientAsync.h"


using namespace MINDIE::MS;

class StressTestPD : public testing::Test {
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

        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "coordinator_to_mindie_server_manage/cert_passwd.txt"), S_IRUSR);

        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/ca.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert.key.pem"), S_IRUSR);
        ChangeCertsFileMode(GetAbsolutePath(certDir, "mindie_server_manage_to_coordinator/cert_passwd.txt"), S_IRUSR);
    }
};

static std::atomic<uint32_t> g_result;

static HttpClientAsync client;

static int32_t SendSSLInferRequestTest(std::string dstIp, std::string dstPort,
    std::string url, std::string reqBody, std::string &response)
{
    std::cout << "reqBody: " << reqBody << std::endl;
    std::mutex mtx;
    std::condition_variable cv;
    uint32_t connId;
    int32_t code;
    ClientHandler handler;
    handler.RegisterFun(ClientHandlerType::RES, [&](std::shared_ptr<ClientConnection> connection) {
        auto &res = connection->GetResMessage();
        response = boost::beast::buffers_to_string(res.body().data());
        if (res.result() != boost::beast::http::status::ok) {
            std::cout << "SendSSLInferRequestTest Error: http code is not ok" << std::endl;
            code = -1;
        } else {
            code = 0;
        }
        cv.notify_one();
    });
    handler.RegisterFun(ClientHandlerType::CHUNK_BODY_RES, [&](std::shared_ptr<ClientConnection> connection) {
        response += connection->GetResChunkedBody();
        if (connection->ResIsFinish()) {
            code = 0;
            cv.notify_one();
        }
    });
    auto ret = client.AddConnection(dstIp, dstPort, connId, handler);
    if (!ret) {
        std::cout << "SendSSLInferRequestTest Error: AddConnection failed" << std::endl;
        return -1;
    }
    auto conn = client.GetConnection(connId);
    if (conn == nullptr) {
        std::cout << "SendSSLInferRequestTest Error: GetConnection failed" << std::endl;
        return -1;
    }
    boost::beast::http::request<boost::beast::http::dynamic_body> req;
    req.version(HTTP_VERSION_11);
    req.method(boost::beast::http::verb::post);
    req.target(url);
    req.keep_alive(false);
    boost::beast::ostream(req.body()) << reqBody;
    req.prepare_payload();
    conn->SendReq(req);
    std::unique_lock<std::mutex> lock(mtx);
    if (cv.wait_for(lock, std::chrono::seconds(600)) == std::cv_status::no_timeout) { // 最多600秒
        std::cout << "response: " << response << std::endl;
        return code;
    } else {
        std::cout << "SendSSLInferRequestTest Error: Timeout" << std::endl;
        return -1;
    }
}

// OpenAI固定请求
static void OpenAIStatic(const std::string &predictIP, const std::string &predictPort)
{
    std::string response;
    TlsItems tlsItems;
    // OpenAI流式
    std::string inferString = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, true);
    std::string outputString = "My name is Olivier and IOK";
    std::string inferUrl = "/v1/chat/completions";
    auto code = SendInferRequest(predictIP, predictPort, tlsItems, inferUrl, inferString, response);
    ASSERT_EQ(code, 0);
    ASSERT_EQ(ProcessStreamRespond(response), outputString);
    // OpenAI非流式
    inferString = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    code = SendInferRequest(predictIP, predictPort, tlsItems, inferUrl, inferString, response);
    ASSERT_EQ(code, 0);
    ASSERT_EQ(response, outputString);
    g_result++;
}

// metrics请求
static void MetricsRequest(const std::string &manageIP, const std::string &managePort,
    const std::string &metricsStressTestPDSeparate)
{
    std::string response;
    TlsItems tlsItems;

    // 发送指标查询请求，默认超时时间60，最大重复请求次数3
    auto ret = GetMetricsRequest(manageIP, managePort, tlsItems, "/metrics", response, false);
    EXPECT_EQ(ret, 200);  // 返回值 200
    EXPECT_EQ(response, metricsStressTestPDSeparate);
}

/*
测试描述: PD分离 openai协议固定请求体内容, 5000高并发压测
测试步骤:
    1. 配置部署4P4D，8个server，预期结果1
    2. 5000并发发送openai协议固定请求，预期结果2
预期结果:
    1. 部署成功
    2. 推理结果正确
*/
TEST_F(StressTestPD, TestPDOpenAIStressTC01)
{
    uint32_t pNum = 4;
    uint32_t dNum = 4;
    uint32_t numThreadInfer = 5000;
    g_result = 0;
    StressTetsPDHelper(pNum + dNum, OpenAIStatic, MetricsRequest, numThreadInfer, pNum, dNum);
    ASSERT_EQ(g_result, numThreadInfer + 1);
}

static void OpenAIStaticTLS(const std::string &predictIP, const std::string &predictPort)
{
    std::string response;
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");
    TlsItems tlsItems;
    tlsItems.caCert = GetAbsolutePath(certDir, "user_to_coordinator/ca.pem");
    tlsItems.tlsCert = GetAbsolutePath(certDir, "user_to_coordinator/cert.pem");
    tlsItems.tlsKey = GetAbsolutePath(certDir, "user_to_coordinator/cert.key.pem");
    tlsItems.tlsPasswd = GetAbsolutePath(certDir, "user_to_coordinator/cert_passwd.txt");
    // OpenAI流式
    std::string inferString = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, true);
    std::string outputString = "My name is Olivier and IOK";
    std::string inferUrl = "/v1/chat/completions";
    auto code = SendSSLInferRequest(predictIP, predictPort, tlsItems, inferUrl, inferString, response);
    ASSERT_EQ(code, 0);
    ASSERT_EQ(ProcessStreamRespond(response), outputString);
    // OpenAI非流式
    inferString = CreateOpenAIRequest({"My name is Olivier and I", "OK"}, false);
    code = SendSSLInferRequest(predictIP, predictPort, tlsItems, inferUrl, inferString, response);
    ASSERT_EQ(code, 0);
    ASSERT_EQ(response, outputString);
    g_result++;
}

// metrics ssl请求
static void MetricsSSLRequest(const std::string &manageIP, const std::string &managePort,
    const std::string &metricsStressTestPDSeparate)
{
    std::string response;
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    std::string certDir = GetAbsolutePath(parentPath, "tests/ms/dt/coordinator/cert_files");
    TlsItems tlsItems;
    tlsItems.caCert = GetAbsolutePath(certDir, "coordinator_to_controller/ca.pem");
    tlsItems.tlsCert = GetAbsolutePath(certDir, "coordinator_to_controller/cert.pem");
    tlsItems.tlsKey = GetAbsolutePath(certDir, "coordinator_to_controller/cert.key.pem");
    tlsItems.tlsPasswd = GetAbsolutePath(certDir, "coordinator_to_controller/cert_passwd.txt");
    // 发送指标查询请求，默认超时时间60，最大重复请求次数3
    auto ret = GetMetricsRequest(manageIP, managePort, tlsItems, "/metrics", response, true);
    EXPECT_EQ(ret, 200);  // 返回值 200
    EXPECT_EQ(response, metricsStressTestPDSeparate);
}

/*
测试描述: PD分离TLS openai协议固定请求体内容, 5000高并发压测
测试步骤:
    1. 配置部署4P4D，8个server，预期结果1
    2. 打开TLS
    3. 5000并发发送openai协议固定请求，预期结果2
预期结果:
    1. 部署成功
    2. 推理结果正确
*/
TEST_F(StressTestPD, TestPDOpenAIStressTC02)
{
    uint32_t pNum = 4;
    uint32_t dNum = 4;
    uint32_t numThreadInfer = 1000;
    g_result = 0;
    StressTetsPDTLSHelper(pNum + dNum, OpenAIStaticTLS, MetricsSSLRequest, numThreadInfer, pNum, dNum);
    ASSERT_EQ(g_result, numThreadInfer + 1);
}

// 五大类推理，待补充...