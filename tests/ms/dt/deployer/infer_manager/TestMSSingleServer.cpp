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
#include <string>
#include <memory>
#include <pthread.h>
#include "gtest/gtest.h"
#include "ConfigParams.h"
#include "Helper.h"
#include "stub.h"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "SingleNodeServer.cpp"
#include "StatusHandler.h"
#include "Util.h"

using namespace MINDIE::MS;

static int32_t g_currentCode = 200;
static std::string g_currentResponse = R"({
                                            "items": [
                                                {
                                                    "metadata": {
                                                        "name": "mindie-server"
                                                    },
                                                    "status": {
                                                        "phase": "Running",
                                                        "podIP": "127.0.0.1"
                                                    }
                                                }
                                            ]
                                        })";

class TestMSSingleServer : public testing::Test {
protected:
    static int32_t SendStubSingle(MINDIE::MS::HttpClient selfObj, const Request &request, int timeoutSeconds,
        int retries, std::string &responseBody, int32_t &code)
    {
        std::cout << "Run_Stub" << std::endl;
        code = 200;  // 200 成功
        if (request.target.find("configmaps") != std::string::npos) {
            responseBody = R"({
                "metadata": {
                    "name": "rings-config-hanxueli-deployment-0",
                    "namespace": "mindie",
                    "uid": "9f5e8d09-1b3f-4ea9-be51-140080f07a68",
                    "resourceVersion": "111449",
                    "creationTimestamp": "2024-08-17T04:19:52Z",
                    "labels": {
                        "ring-controller.atlas": "ascend-910b"
                    }
                },
                "data": {
                    "hccl.json": "{\"status\":\"completed\"}"
                }
            })";
        }
        return 0;
    }
    static int32_t SendCreateClientReqStub(void *obj, const std::string &url, boost::beast::http::verb type,
        const std::string &inputStr, std::string &response, HttpClient &client)
    {
        std::cout << "SendCreateClientReqStub" << std::endl;
        response = g_currentResponse;
        return g_currentCode;
    }

    void SetUp()
    {
        CopyDefaultConfig();
        auto inferJson = GetMSDeployTestJsonPath();
        // 设置单机
        JsonFileManager manager(inferJson);
        manager.Load();
        manager.Set("server_type", "mindie_single_node");  // 127 非法配置
        manager.Save();
        stub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SendStubSingle);
    }

    void TearDown()
    {
        stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
        g_currentCode = 200;
        g_currentResponse = R"({
                                "items": [
                                    {
                                        "metadata": {
                                            "name": "mindie-server"
                                        },
                                        "status": {
                                            "phase": "Running",
                                            "podIP": "127.0.0.1"
                                        }
                                    }
                                ]
                            })";
    }

    Stub stub;
};

HttpClientParams SetClientParams()
{
    HttpClientParams params;

    // Default values for HttpClientParams
    params.k8sIP = "127.0.0.1";   // Default IP address
    params.k8sPort = 8080;        // 8080 Default Kubernetes API Server port
    params.prometheusPort = 9090; // 9090 Default Prometheus port

    // Default values for TlsItems - TLS is enabled by default
    // Note: The constructor initializes tlsEnable to true, so we don't need to set it again here.
    params.k8sClientTlsItems.caCert = "path/to/k8s-ca-cert.pem";      // Example CA certificate path
    params.k8sClientTlsItems.tlsCert = "path/to/k8s-client-cert.pem"; // Example client certificate path
    params.k8sClientTlsItems.tlsKey = "path/to/k8s-client-key.pem";   // Example client key path
    params.k8sClientTlsItems.tlsPasswd = "";                          // No password by default
    params.k8sClientTlsItems.tlsCrl = "path/to/k8s-crl.pem";          // Example CRL path

    params.mindieClientTlsItems.caCert = "path/to/mindie-ca-cert.pem";      // Example CA certificate path
    params.mindieClientTlsItems.tlsCert = "path/to/mindie-client-cert.pem"; // Example client certificate path
    params.mindieClientTlsItems.tlsKey = "path/to/mindie-client-key.pem";   // Example client key path
    params.mindieClientTlsItems.tlsPasswd = "";                             // No password by default
    params.mindieClientTlsItems.tlsCrl = "path/to/mindie-crl.pem";          // Example CRL path

    params.promClientTlsItems.caCert = "path/to/prom-ca-cert.pem";      // Example CA certificate path
    params.promClientTlsItems.tlsCert = "path/to/prom-client-cert.pem"; // Example client certificate path
    params.promClientTlsItems.tlsKey = "path/to/prom-client-key.pem";   // Example client key path
    params.promClientTlsItems.tlsPasswd = "";                           // No password by default
    params.promClientTlsItems.tlsCrl = "path/to/prom-crl.pem";          // Example CRL path

    return params;
}

/*
测试描述: 单机部署，正确调用Init方法
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2. 配置有效的 `HttpClientParams` 参数。
    3. 设置状态文件路径。
    4. 调用 `Init` 方法，预期结果1
预期结果:
    1. `Init` 方法返回 `(0, "Init client success")`。
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerInitSucess)
{
    SingleNodeServer nodeServer;
    HttpClientParams httpClientConfig;
    httpClientConfig.k8sIP = "127.0.0.1";
    httpClientConfig.k8sPort = 8080;
    httpClientConfig.prometheusPort = 9090;
    httpClientConfig.k8sClientTlsItems.tlsEnable = false;
    httpClientConfig.mindieClientTlsItems.tlsEnable = false;
    std::string statusPath = "ms_status.json";

    auto result = nodeServer.Init(httpClientConfig, statusPath);

    EXPECT_EQ(result.first, 0);
    EXPECT_EQ(result.second, "Init client success");

    nodeServer.Unload();
}

/*
测试描述: 单机部署，错误调用Init方法
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2. 配置无效的 `HttpClientParams` 参数。
    3. 设置状态文件路径。
    4. 调用 `Init` 方法，预期结果1
预期结果:
    1. `Init` 方法返回 `(-1, "Init client failed")`。
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerInitFail)
{
    SingleNodeServer nodeServer;
    HttpClientParams httpClientConfig = SetClientParams();
    std::string statusPath = "ms_status.json";

    auto result = nodeServer.Init(httpClientConfig, statusPath);

    EXPECT_EQ(result.first, -1);
    EXPECT_EQ(result.second, "Init client failed");

    nodeServer.Unload();
}

/*
测试描述: 单机部署，成功调用Deploy方法
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2. 获取部署测试 JSON 路径。
    3. 读取 JSON 文件内容。
    4. 调用 `Deploy` 方法，预期结果1
预期结果:
    1. `Deploy` 方法返回 `(0, "creating the server!")`。
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerDeploySucess)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    // 修改 enable_tls 字段为 true
    json j = json::parse(config);
    j["mindie_server_config"]["enable_tls"] = true;
    std::string modifiedConfig = j.dump();

    stub.set(ADDR(MINDIE::MS::SingleNodeServer, HandleLoadReq), ReturnZeroStub);

    auto msg = nodeServer.Deploy(modifiedConfig);

    EXPECT_EQ(msg.first, 0);
    EXPECT_EQ(msg.second, "Creating the server!");

    stub.reset(ADDR(MINDIE::MS::SingleNodeServer, HandleLoadReq));
    nodeServer.Unload();
}

/*
测试描述: 单机部署，构造场景测试 `CreateDeployJson` 函数。
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2. 获取部署测试 JSON 文件路径。
    3. 读取 JSON 文件内容并解析。
    4. 调用 `Deploy` 方法，预期结果1。
预期结果:
    1. `Deploy` 方法返回 `(-1, "deploy deployments failed")`，表示部署失败。
*/

TEST_F(TestMSSingleServer, TestSingleNodeServerDeployFail)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    // 修改 enable_tls 字段为 true
    json j = json::parse(config);
    j["mindie_server_config"]["enable_tls"] = true;
    std::string modifiedConfig = j.dump();

    stub.set(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq), ReturnZeroStub);

    auto msg = nodeServer.Deploy(modifiedConfig);

    EXPECT_EQ(msg.first, -1);
    EXPECT_EQ(msg.second, "Deploy deployments failed");

    stub.reset(ADDR(MINDIE::MS::SingleNodeServer, HandleLoadReq));
    nodeServer.Unload();
}

/*
测试描述: 单机部署正常流程
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC01)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    auto msg = nodeServer.Deploy(config);
    nodeServer.Unload();
}

/*
测试描述: 单机部署，异常json文件
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC02)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    auto msg = nodeServer.Deploy("config");

    nodeServer.Unload();
}

/*
测试描述: 单机部署，异常json文件
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC03)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    auto msg = nodeServer.Deploy("config");

    nodeServer.Unload();
}

/*
测试描述: 单机部署，异常json server_name 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC04)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    {
        json config = json::parse(inferCtx);
        config.erase("server_name");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("server_name") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("server_name") = "";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);

        nodeServer.Unload();
    }
    {
        json config = json::parse(inferCtx);
        config.at("server_name") = "111111111111111111111111111111111111111111111111111111111111111111111111111111";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json init_delay 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerInitDelayTC01)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    {
        json config = json::parse(inferCtx);
        config.erase("init_delay");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("init_delay") = 0;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("init_delay") = 1801;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json liveness_timeout 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerLivenessTimeoutTC01)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    {
        json config = json::parse(inferCtx);
        config.erase("liveness_timeout");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("liveness_timeout") = 0;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("liveness_timeout") = 301;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json liveness_failure_threshold 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerLivenessFailureThresholdTC01)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    {
        json config = json::parse(inferCtx);
        config.erase("liveness_failure_threshold");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("liveness_failure_threshold") = 0;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("liveness_failure_threshold") = 11;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json readiness_timeout 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerReadinessTimeoutTC01)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 readiness_timeout
    {
        json config = json::parse(inferCtx);
        config.erase("readiness_timeout");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 删除 readiness_timeout
    {
        json config = json::parse(inferCtx);
        config.at("readiness_timeout") = 0;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 删除 readiness_timeout
    {
        json config = json::parse(inferCtx);
        config.at("readiness_timeout") = 301;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json readiness_failure_threshold 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerReadinessFailureThresholdTC01)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    {
        json config = json::parse(inferCtx);
        config.erase("readiness_failure_threshold");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("readiness_failure_threshold") = 0;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("readiness_failure_threshold") = 11;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json replicas 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerReplicasTC04)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    {
        json config = json::parse(inferCtx);
        config.erase("replicas");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("replicas") = "1";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    {
        json config = json::parse(inferCtx);
        config.at("replicas") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json scheduler 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC05)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 scheduler
    {
        json config = json::parse(inferCtx);
        config.erase("scheduler");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 scheduler
    {
        json config = json::parse(inferCtx);
        config.at("scheduler") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 scheduler
    {
        json config = json::parse(inferCtx);
        config.at("scheduler") = "default111";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json service_type 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC06)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 service_type
    {
        json config = json::parse(inferCtx);
        config.erase("service_type");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 service_type
    {
        json config = json::parse(inferCtx);
        config.at("service_type") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 service_type
    {
        json config = json::parse(inferCtx);
        config.at("service_type") = "NodePort11";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json service_port 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC07)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 service_port
    {
        json config = json::parse(inferCtx);
        config.erase("service_port");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 service_port
    {
        json config = json::parse(inferCtx);
        config.at("service_port") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 service_port
    {
        json config = json::parse(inferCtx);
        config.at("service_port") = "5555";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json resource_requests 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC08)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 resource_requests
    {
        json config = json::parse(inferCtx);
        config.erase("resource_requests");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config.at("resource_requests") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json resource_requests memory 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC09)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].erase("memory");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("memory") = 0.25;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("memory") = 999;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("memory") = 1000001;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json resource_requests cpu_core 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC10)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].erase("cpu_core");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("cpu_core") = 0.25;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("cpu_core") = 999;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("cpu_core") = 256001;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json resource_requests npu_type 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC11)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].erase("npu_type");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("npu_type") = 0.25;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("npu_type") = "Ascend911";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json resource_requests npu_chip_num 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC12)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].erase("npu_chip_num");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("npu_chip_num") = "8";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config["resource_requests"].at("npu_chip_num") = 74;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json mindie_server_config 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC13)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config.erase("mindie_server_config");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 resource_requests
    {
        json config = json::parse(inferCtx);
        config.at("mindie_server_config") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json mindie_server_config mies_install_path 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC14)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].erase("mies_install_path");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].at("mies_install_path") = 0.25;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].at("mies_install_path") = "Ascend911";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json mindie_server_config infer_port 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC15)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].erase("infer_port");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].at("infer_port") = 1023;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].at("infer_port") = "1025";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json mindie_server_config management_port 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC16)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].erase("management_port");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].at("management_port") = 1023;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].at("management_port") = "1025";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json mindie_server_config enable_tls 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC17)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].erase("enable_tls");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].at("enable_tls") = "1025";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 mindie_server_config
    {
        json config = json::parse(inferCtx);
        config["mindie_server_config"].at("enable_tls") = false;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json npu_fault_reschedule 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC18)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 npu_fault_reschedule
    {
        json config = json::parse(inferCtx);
        config.erase("npu_fault_reschedule");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 npu_fault_reschedule
    {
        json config = json::parse(inferCtx);
        config.at("npu_fault_reschedule") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 npu_fault_reschedule
    {
        json config = json::parse(inferCtx);
        config.at("npu_fault_reschedule") = false;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json termination_grace_period_seconds 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC19)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 termination_grace_period_seconds
    {
        json config = json::parse(inferCtx);
        config.erase("termination_grace_period_seconds");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 termination_grace_period_seconds
    {
        json config = json::parse(inferCtx);
        config.at("termination_grace_period_seconds") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 termination_grace_period_seconds
    {
        json config = json::parse(inferCtx);
        config.at("termination_grace_period_seconds") = false;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json max_unavailable 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC20)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 max_unavailable
    {
        json config = json::parse(inferCtx);
        config.erase("max_unavailable");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 max_unavailable
    {
        json config = json::parse(inferCtx);
        config.at("max_unavailable") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 max_unavailable
    {
        json config = json::parse(inferCtx);
        config.at("max_unavailable") = "%50";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 max_unavailable
    {
        json config = json::parse(inferCtx);
        config.at("max_unavailable") = "%50";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: 单机部署，异常json max_unavailable 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeServerTC21)
{
    SingleNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 max_surge
    {
        json config = json::parse(inferCtx);
        config.erase("max_surge");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 max_surge
    {
        json config = json::parse(inferCtx);
        config.at("max_surge") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 max_surge
    {
        json config = json::parse(inferCtx);
        config.at("max_surge") = "%50";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }

    // 修改 max_surge
    {
        json config = json::parse(inferCtx);
        config.at("max_surge") = "%50";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        nodeServer.Unload();
    }
}

/*
测试描述: SetEnv 配置项
*/
TEST_F(TestMSSingleServer, TestSingleNodeSetEnv)
{
    nlohmann::json containers;
    LoadServiceParams params;
    params.miesInstallPath = "";
    params.mindieUseHttps = true;
    SingleNodeServer nodeServer;
    SetEnv(containers, "scheduler", "npuType", params);
    params.mindieUseHttps = false;
    SetEnv(containers, "scheduler", "npuType", params);
}

/*
测试描述: 测试 `GetDeployStatus` 函数在正常配置下能否成功返回正确的状态。
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2. 调用 `GetDeployStatus` 函数，预期结果1。
预期结果:
    1. 调用 `GetDeployStatus` 函数成功，返回 `(0, valid_json_response)`。
*/
TEST_F(TestMSSingleServer, TestGetDeployStatusTC01)
{
    SingleNodeServer nodeServer;
    stub.set(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq), &SendCreateClientReqStub);

    auto result = nodeServer.GetDeployStatus();

    EXPECT_EQ(result.first, 0);
    stub.reset(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq));
    nodeServer.Unload();
}

/*
测试描述: 测试 `GetDeployStatus` 函数在响应不是 JSON 格式时能否正确处理错误。
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2. 将 `g_currentResponse` 设置为空字符串。
    3. 调用 `GetDeployStatus` 函数，预期结果1。
预期结果:
    1. 调用 `GetDeployStatus` 函数失败。
*/
TEST_F(TestMSSingleServer, TestGetDeployStatusTC02)
{
    SingleNodeServer nodeServer;
    g_currentResponse = "";
    stub.set(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq), &SendCreateClientReqStub);

    auto result = nodeServer.GetDeployStatus();

    EXPECT_EQ(result.first, -1);
    EXPECT_EQ(result.second, "Response in GetDeployStatus is not json.");
    stub.reset(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq));
    nodeServer.Unload();
}

/*
测试描述: 测试 `GetDeployStatus` 函数在响应中的 `items` 字段格式不正确时能否正确处理错误。
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2. 将 `g_currentResponse` 设置为一个无效的 JSON 字符串 `R"({})"`.
    3. 调用 `GetDeployStatus` 函数，预期结果1。
预期结果:
    1. 调用 `GetDeployStatus` 函数失败。
*/
TEST_F(TestMSSingleServer, TestGetDeployStatusTC03)
{
    SingleNodeServer nodeServer;
    g_currentResponse = R"({})";
    stub.set(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq), &SendCreateClientReqStub);

    auto result = nodeServer.GetDeployStatus();

    EXPECT_EQ(result.first, -1);
    EXPECT_EQ(result.second, "Pod items is invalid");
    stub.reset(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq));
    nodeServer.Unload();
}

/*
测试描述: 测试 `GetDeployStatus` 函数在响应中的 `items` 字段缺少 `metadata` 时能否正确处理错误。
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2.  `g_currentResponse` 缺少 `metadata` 。
    3. 调用 `GetDeployStatus` 函数，预期结果1。
预期结果:
    1. 调用 `GetDeployStatus` 函数失败。
*/
TEST_F(TestMSSingleServer, TestGetDeployStatusTC04)
{
    SingleNodeServer nodeServer;
    g_currentResponse = R"({
        "items": [
            {
                "status": {
                    "podIP": "127.0.0.1"
                }
            }
        ]
    })";
    stub.set(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq), &SendCreateClientReqStub);

    auto result = nodeServer.GetDeployStatus();

    EXPECT_EQ(result.first, -1);
    EXPECT_EQ(result.second, "Pod item does not contain metadata");
    stub.reset(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq));
    nodeServer.Unload();
}

/*
测试描述: 测试 `GetDeployStatus` 函数在响应中的 `metadata` 中缺少 `name` 时能否正确处理错误。
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2.  `g_currentResponse` 的 `metadata` 中缺少 `name`。
    3. 调用 `GetDeployStatus` 函数，预期结果1。
预期结果:
    1. 调用 `GetDeployStatus` 函数失败。
*/
TEST_F(TestMSSingleServer, TestGetDeployStatusTC05)
{
    SingleNodeServer nodeServer;
    g_currentResponse = R"({
        "items": [
            {
                "metadata": {},
                "status": {
                    "podIP": "127.0.0.1"
                }
            }
        ]
    })";
    stub.set(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq), &SendCreateClientReqStub);

    auto result = nodeServer.GetDeployStatus();

    EXPECT_EQ(result.first, -1);
    EXPECT_EQ(result.second, "Pod item metadata does not contain name");
    stub.reset(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq));
    nodeServer.Unload();
}

/*
测试描述: 测试 `GetDeployStatus` 函数在服务未启动时能否正确处理错误。
测试步骤:
    1. 创建 `SingleNodeServer` 对象。
    2. 将 `g_currentCode` 设置为 `0` ，非有效状态码。
    3. 调用 `GetDeployStatus` 函数，预期结果1。
预期结果:
    1. 调用 `GetDeployStatus` 函数失败。
*/
TEST_F(TestMSSingleServer, TestGetDeployStatusTC06)
{
    SingleNodeServer nodeServer;
    g_currentCode = 0;
    stub.set(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq), &SendCreateClientReqStub);

    auto result = nodeServer.GetDeployStatus();

    EXPECT_EQ(result.first, -1);
    EXPECT_EQ(result.second, "Service is not exist.");
    stub.reset(ADDR(MINDIE::MS::SingleNodeServer, CreateClientReq));
    nodeServer.Unload();
}