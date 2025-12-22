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
#include <iostream>
#include <cstdlib>
#include <thread>
#include "gtest/gtest.h"
#include "stub.h"
#include "ProbeServer.h"
#include "ControllerConfig.h"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "Helper.h"

using namespace MINDIE::MS;
const int CODE_OK = 200;

class TestProbeServer : public testing::Test {
public:
    void SetUp() override
    {
        port = GetUnBindPort();
        std::string controllerJson = GetMSControllerConfigJsonPath();
        auto testJson = GetProbeServerTestJsonPath();
        CopyFile(controllerJson, testJson);
        JsonFileManager manager(testJson);
        manager.Load();
        manager.SetList({"http_server", "port"}, port);
        manager.SetList({"tls_config", "request_coordinator_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "http_server_tls_enable"}, false);
        manager.Save();
        ModifyJsonItem(testJson, "http_server", "port", port);
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << testJson << std::endl;

        ControllerConfig::GetInstance()->Init();

        clientTlsItems.tlsEnable = false;
        waitSeconds = 5;
        retryTimes = 2;
    }

    std::unique_ptr<std::thread> mMainThread;
    TlsItems clientTlsItems;
    int waitSeconds;
    int retryTimes;
    int port;
};

struct Params {
    TlsItems clientTlsItems;
    int waitSeconds;
    int retryTimes;
};

int SendHttpRequestMock(const std::string& ip,
                        const std::string& portStr,
                        const std::string& url,
                        const Params& params)
{
    HttpClient httpClient;
    httpClient.Init(ip, portStr, params.clientTlsItems, true);

    std::string response;
    int32_t code;
    std::map<boost::beast::http::field, std::string> headers;
    headers[boost::beast::http::field::accept] = "*/*";
    headers[boost::beast::http::field::content_type] = "application/json";

    Request req = {url, boost::beast::http::verb::get, headers, ""};
    auto ret = httpClient.SendRequest(req, params.waitSeconds, params.retryTimes, response, code);

    nlohmann::json responseJsonObj = nlohmann::json::parse(response);

    // response 校验，校验是否为管理面健康检查接口返回
    if (ret == 0 && code == CODE_OK) {
        try {
            nlohmann::json responseJsonObj = nlohmann::json::parse(response);
            if (responseJsonObj.contains("message") && responseJsonObj.contains("status")) {
                return 0;
            } else {
                std::cerr << "JSON response validation failed: " << responseJsonObj.dump() << std::endl;
            }
        } catch (const nlohmann::json::parse_error& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception: " << e.what() << std::endl;
        }
    }

    LOG_E("[SendHttpRequest] Get ip %s, port %s, url %s, ret %d", ip.c_str(), portStr.c_str(), url.c_str(), code);
    return -1;
}

/*
测试描述: ProbeServerRunSuccess测试用例，默认不开启全零监听
测试步骤:
    1. 启动ProbeServer:
        - 期待ProbeServer.Init()返回0表示初始化成功
        - 启动服务器的运行线程，并detach()
        - 等待一段时间以确保ProbeServer完全启动
    2. 通过执行SendHttpRequestMock进行检查:
        - SendHttpRequestMock参数127.0.0.1 1026 /v1/startup并期待返回值为0
        - SendHttpRequestMock参数127.0.0.1 1026 /v1/health并期待返回值为0

预期结果:
    1. ProbeServer成功启动:
        - ProbeServer.Init()返回0
        - ProbeServer.Run()在新线程中运行
    2. SendHttpRequestMock的运行结果为0表示成功:
        - 运行SendHttpRequestMock 127.0.0.1 port /v1/startup成功
        - 运行SendHttpRequestMock 127.0.0.1 port /v1/health成功
*/
TEST_F(TestProbeServer, ProbeServerRunSuccess)
{
    ControllerConfig::GetInstance()->Init();
    ProbeServer probeServer;

    EXPECT_EQ(probeServer.Init(), 0);

    std::this_thread::sleep_for(std::chrono::seconds(3));

    Params params;
    params.clientTlsItems = clientTlsItems;
    params.waitSeconds = waitSeconds;
    params.retryTimes = retryTimes;

    bool success1 = (SendHttpRequestMock("127.0.0.1", std::to_string(port), "/v1/startup", params) == 0);
    bool success2 = (SendHttpRequestMock("127.0.0.1", std::to_string(port), "/v1/health", params) == 0);

    EXPECT_TRUE(success1);
    EXPECT_TRUE(success2);
}

/*
测试描述: Controller健康探针服务端开启全零监听测试用例
测试步骤:
    1. 修改默认的controller配置文件，放行全零监听并将http_server的ip设为全零
    2. 启动ProbeServer:
        - 期待ProbeServer.Init()返回0表示初始化成功
        - 启动服务器的运行线程，并detach()
        - 等待一段时间以确保ProbeServer完全启动
    3. 通过执行SendHttpRequestMock进行检查:
        - SendHttpRequestMock参数127.0.0.1 1026 /v1/startup并期待返回值为0
        - SendHttpRequestMock参数127.0.0.1 1026 /v1/health并期待返回值为0

预期结果:
    1. ProbeServer成功启动:
        - ProbeServer.Init()返回0
        - ProbeServer.Run()在新线程中运行
    2. SendHttpRequestMock的运行结果为0表示成功:
        - 运行SendHttpRequestMock 127.0.0.1 port /v1/startup成功
        - 运行SendHttpRequestMock 127.0.0.1 port /v1/health成功
*/
TEST_F(TestProbeServer, AllZeroIpListeningSuccess)
{
    auto testJson = GetProbeServerTestJsonPath();
    ModifyJsonItem(testJson, "allow_all_zero_ip_listening", "", true);
    ModifyJsonItem(testJson, "http_server", "ip", "0.0.0.0");
    ControllerConfig::GetInstance()->Init();
    ProbeServer probeServer;

    EXPECT_EQ(probeServer.Init(), 0);

    std::this_thread::sleep_for(std::chrono::seconds(3));

    Params params;
    params.clientTlsItems = clientTlsItems;
    params.waitSeconds = waitSeconds;
    params.retryTimes = retryTimes;

    bool success1 = (SendHttpRequestMock("127.0.0.1", std::to_string(port), "/v1/startup", params) == 0);
    bool success2 = (SendHttpRequestMock("127.0.0.1", std::to_string(port), "/v1/health", params) == 0);

    EXPECT_TRUE(success1);
    EXPECT_TRUE(success2);
}