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
#include <sys/stat.h>
#include "gtest/gtest.h"
#include "ServerManager.h"
#define private public
#define protected public
#define main __main_server__
#define g_maxPort g_maxServerPort
#define g_minPort g_minServerPort
#define PrintHelp ServerPrintHelp
#include "deployer/server/main.cpp"
#undef g_maxPort
#undef g_minPort
#define main __main_client__
#define PrintHelp ClientPrintHelp
#include "deployer/msctl/main.cpp"
#include "ConfigParams.h"
#include "Helper.h"
#include "stub.h"
#include "JsonFileManager.h"
#include "HttpClient.h"
#include "CrossNodeServer.cpp"
#include "StatusHandler.h"
#include "HttpServer.cpp"
#include "Util.h"
#include "Util.cpp"


std::string g_stubK8sResponseBody = "";
int32_t g_stubK8sResponseCode = 0;

class TestMSServer : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }
    static boost::system::error_code UseCertificateChainFileStub(std::string &path, boost::system::error_code &ec)
    {
        std::cout << "UseCertificateChainFileStub" << std::endl;
    }
    static boost::system::error_code UsePrivateKeyFileStub(const std::string &path,
        boost::asio::ssl::context::file_format format, boost::system::error_code &ec)
    {
        std::cout << "UsePrivateKeyFileStub" << std::endl;
    }

    static boost::system::error_code AddCertificateAuthorityStub(const boost::asio::const_buffer &ca,
        boost::system::error_code &ec)
    {
        std::cout << "AddCertificateAuthorityStub" << std::endl;
    }
    static bool ServerDecryptStub(boost::asio::ssl::context &mSslCtx, const MINDIE::MS::HttpServerParams &httpParams,
        std::pair<std::string, int32_t> &mPassword)
    {
        std::cout << "ServerDecryptStub" << std::endl;
        return true;
    }
    static int32_t SendStub(MINDIE::MS::HttpClient selfObj, const Request &request, int timeoutSeconds, int retries,
        std::string &responseBody, int32_t &code)
    {
        std::cout << "Run_Stub" << std::endl;
        code = 200; // 200 成功
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

    static void AssociateServiceStub(const char *format)
    {
        std::cout << "AssociateServiceStub" << std::endl;
        return;
    }

    void SetUp()
    {
        CopyDefaultConfig();
        std::string exePath = GetExecutablePath();
        std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
        auto msctlJson = GetAbsolutePath(parentPath, "tests/ms/common/.mindie_ms/msctl.json");
        std::cout << "deployer:" << msctlJson << std::endl;
        auto homePath = GetParentPath(GetParentPath(msctlJson));
        setenv("HOME", homePath.c_str(), 1);
        std::cout << "HOME=" << homePath << std::endl;
        auto hseceasyPath = GetHSECEASYPATH();
        setenv("HSECEASY_PATH", hseceasyPath.c_str(), 1);
        std::cout << "HSECEASY_PATH=" << hseceasyPath << std::endl;
    
        g_stubK8sResponseBody = "";
        g_stubK8sResponseCode = 0;
        stub.set((boost::system::error_code(boost::asio::ssl::context::*)(const std::string &,
            boost::system::error_code &))ADDR(boost::asio::ssl::context, use_certificate_chain_file),
            UseCertificateChainFileStub);

        stub.set((boost::system::error_code(boost::asio::ssl::context::*)(const boost::asio::const_buffer &,
            boost::system::error_code &))ADDR(boost::asio::ssl::context, add_certificate_authority),
            AddCertificateAuthorityStub);

        stub.set((boost::system::error_code(boost::asio::ssl::context::*)(const std::string &,
            boost::asio::ssl::context::file_format, boost::system::error_code &))ADDR(boost::asio::ssl::context,
            use_private_key_file),
            UsePrivateKeyFileStub);

        stub.set((boost::system::error_code(boost::asio::ssl::context::*)(const std::string &,
            boost::asio::ssl::context::file_format, boost::system::error_code &))ADDR(boost::asio::ssl::context,
            use_private_key_file),
            UsePrivateKeyFileStub);

        stub.set((std::size_t(boost::asio::io_context::*)())ADDR(boost::asio::io_context, run), run_stub);
        stub.set(ADDR(MINDIE::MS::HttpClient, SendRequest), SendStub);
        stub.set(ServerDecrypt, ServerDecryptStub);
    }

    void TearDown()
    {
        stub.reset((boost::system::error_code(boost::asio::ssl::context::*)(const std::string &,
            boost::system::error_code &))ADDR(boost::asio::ssl::context, use_certificate_chain_file));
        stub.reset((boost::system::error_code(boost::asio::ssl::context::*)(const std::string &,
            boost::asio::ssl::context::file_format, boost::system::error_code &))ADDR(boost::asio::ssl::context,
            use_private_key_file));
        stub.reset((std::size_t(boost::asio::io_context::*)())ADDR(boost::asio::io_context, run));
        stub.reset(ADDR(MINDIE::MS::HttpClient, SendRequest));
    }

    Stub stub;
};

// 测试PrintHelp
TEST_F(TestMSServer, TestServerTC01)
{
    // Expected output string
    const std::string expectedOutput = "Usage: ./ms_server [config_file]\n\n"
        "Description:\n"
        "  ms_server is a management server used to manage mindie inference server.\n\n"
        "Arguments:\n"
        "  config_file    Path to the configuration file (in JSON format) that specifies the server settings,"
        " for more detail, see the product documentation.\n\n"
        "Examples:\n"
        "  Run the management server with the specified configuration file:\n"
        "    ./ms_server ms_server.json\n"
        "\n";

    // Capture the actual output from PrintHelp()
    std::ostringstream ss;
    std::streambuf *prev = std::cout.rdbuf(ss.rdbuf());
    // Call the function whose output we want to capture
    ServerPrintHelp();
    // Restore the original stream buffer
    std::cout.rdbuf(prev);
    std::string actualOutput = ss.str();
    // Compare the actual output with the expected output
    std::cout << "Actual Output:\n" << actualOutput << std::endl;
    EXPECT_EQ(actualOutput, expectedOutput);
}

int PrintfStub(const char *fileName, HttpClientParams &clientParams, HttpServerParams &httpServerParams,
    std::string &statusPath)
{
    std::cout << "I am PrintfStub:\n" << std::endl;
    statusPath = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    return 5; // 5 stub返回值
}

TEST_F(TestMSServer, TestServerTC04)
{
    Stub stub;
    stub.set(ParseCommandArgs, PrintfStub);
    HttpClientParams clientParams;
    HttpServerParams httpServerParams;
    std::string statusPath;

    auto ret = ParseCommandArgs("test.json", clientParams, httpServerParams, statusPath);
    std::cout << "ret:\n" << ret << std::endl;
    std::cout << "statusPath:\n" << statusPath << std::endl;
    stub.reset(ParseCommandArgs);
}

/*
测试描述: 服务端配置文件测试，正常配置文件
测试步骤:
    1. 配置serverTestJsonPath为正常文件，内容有效，预期结果1
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
*/
TEST_F(TestMSServer, ServerStartTest)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
    EXPECT_EQ(ret, 0);
    JsonFileManager manager(serverTestJsonPath);
    manager.Load();

    EXPECT_EQ(clientParams.k8sIP, manager.Get<std::string>("k8s_apiserver_ip"));
    EXPECT_EQ(clientParams.k8sPort, manager.Get<int64_t>("k8s_apiserver_port"));

    EXPECT_EQ(statusPath, manager.Get<std::string>("ms_status_file"));

    MINDIE::MS::ServerManager serverManager(clientParams);
    HttpServer server(1);

    ret = server.Run(serverParams);
}

/*
测试描述: 部署配置测试用例
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestMSServer, InferServerLoadServerTest)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    std::string deployStr = SetInferJsonDefault(GetMSDeployTestJsonPath());
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
    EXPECT_EQ(ret, 0);
    MINDIE::MS::ServerManager serverManager(clientParams);

    boost::beast::http::request<boost::beast::http::string_body> boostReq;
    boostReq.body() = deployStr;
    boostReq.version(11); // 11 http1.1
    boostReq.method(boost::beast::http::verb::post);
    boostReq.target("/v1/servers");
    boostReq.prepare_payload();
    REGISTER_INFER_SERVER(CrossNodeServer, mindie_cross_node);
    serverManager.PostServersHandler(boostReq);
    boostReq.method(boost::beast::http::verb::get);
    boostReq.target("/v1/servers/mindie-ie");
    serverManager.GetDeployStatus(boostReq);
    serverManager.UnloadServer(boostReq);
}

/*
测试描述: 加载已部署服务测试用例
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestMSServer, InferServeFromStatusFileTest)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    std::string deployStr = SetInferJsonDefault(GetMSDeployTestJsonPath());
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
    EXPECT_EQ(ret, 0);

    JsonFileManager manager(serverTestJsonPath);
    manager.Load();
    std::string ms_status_file = manager.Get<std::string>("ms_status_file");

    JsonFileManager status(ms_status_file);
    status.Load();
    json server;
    json server_list;

    server["namespace"] = "mindie";
    server["replicas"] = 1;
    server["server_name"] = "test";
    server["server_type"] = "mindie_cross_node";
    server["use_service"] = true;
    server_list.push_back(server);
    status.Set("server_list", server_list);
    status.Save();
    std::cout << "status: " << status.Dump() << std::endl;
    MINDIE::MS::ServerManager serverManager(clientParams);
    serverManager.FromStatusFile(ms_status_file);
}

/*
测试描述: 服务端配置文件测试，配置异常的ip地址字符串值
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestMSServer, ParseCommandArgsInvalidIPString)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();

    std::vector<std::string> cfgTypes = { "ip", "k8s_apiserver_ip" };
    std::vector<std::string> testIPs = { "256.0.0.0", "0.0.0.0", "", "127.0.0.1.", "127.0.1", "127.a.0.1" };
    for (auto cfg : cfgTypes) {
        for (auto ip : testIPs) {
            std::cout << "cfg: " << cfg << std::endl;
            std::cout << "ip: " << ip << std::endl;
            {
                CopyDefaultConfig();
                // 2. 修改 ip 字段为异常值，预期结果2
                JsonFileManager manager(serverTestJsonPath);
                manager.Load();
                manager.Set(cfg, ip);
                manager.Save();
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, -1);
            }
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.Set(cfg, 127); // 127 非法配置
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.Erase(cfg);
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
    }
}

/*
测试描述: 服务端配置文件测试，配置异常的端口字符串值
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestMSServer, ParseCommandArgsInvalidPort)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;

    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();

    std::vector<std::string> cfgTypes = { "port", "k8s_apiserver_port" };
    std::vector<int64_t> testPorts = { -20, 0, 1023, 65536, 9999999 };
    for (auto cfg : cfgTypes) {
        for (auto port : testPorts) {
            std::cout << "cfg: " << cfg << std::endl;
            std::cout << "port: " << port << std::endl;
            {
                CopyDefaultConfig();
                // 2. 修改 ip 字段为异常值，预期结果2
                JsonFileManager manager(serverTestJsonPath);
                manager.Load();
                manager.Set(cfg, port);
                manager.Save();
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, -1);
            }
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.Set(cfg, "sss");
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.Erase(cfg);
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
    }
}

/*
测试描述: 服务端配置文件测试，配置异常的日志目录不存在
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestMSServer, ParseCommandArgsInvalidlog)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;
    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    std::vector<std::string> cfgKeys = { "run_log_path", "operation_log_path" };
    for (auto item : cfgKeys) {
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            std::string valueStr = item;
            for (int i = 0; i <= MAX_DEPTH; i++) {
                valueStr += "/A";
            }
            manager.SetList({"log_info", item}, "./" + std::to_string(GetTimeStampNow()) + valueStr);  // 路径深度过深
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, -1);
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.SetList({ "log_info", item }, 127); // 127 不是有效路径
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, 0);
        }
        {
            CopyDefaultConfig();
            // 2. 修改 ip 字段为异常值，预期结果2
            JsonFileManager manager(serverTestJsonPath);
            manager.Load();
            manager.EraseList({ "log_info", item });
            manager.Save();
            int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
            EXPECT_EQ(ret, 0);
        }
    }
}

template <typename T> void SetMSStatusFile(JsonFileManager &manager, const T &value)
{
    manager.Load();
    manager.Set("ms_status_file", value);
    manager.Save();
}
/*
测试描述: 服务端配置文件测试，配置ms_status_file异常
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestMSServer, ParseCommandArgsInvalidStatus)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;
    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();

    {
        CopyDefaultConfig();
        // 文件不存在
        JsonFileManager manager(serverTestJsonPath);
        SetMSStatusFile(manager, "AAAAA");
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, 0);
    }
    {
        CopyDefaultConfig();
        // 日志目录不存在
        JsonFileManager manager(serverTestJsonPath);
        SetMSStatusFile(manager, "AAAAA/BBBBB");
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, 0);
    }
    {
        CopyDefaultConfig();
        // 文件内容异常
        JsonFileManager manager(serverTestJsonPath);
        SetMSStatusFile(manager, "testcase");
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, 0);
    }
    {
        CopyDefaultConfig();
        // 文件内容异常
        JsonFileManager manager(serverTestJsonPath);
        SetMSStatusFile(manager, 123); // 123 不合规文件名
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, -1);
    }
    {
        CopyDefaultConfig();
        JsonFileManager manager(serverTestJsonPath);
        manager.Load();
        manager.Erase("ms_status_file");
        manager.Save();
        int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
        EXPECT_EQ(ret, -1);
    }
}

template <typename T>
void SetManagerList(JsonFileManager &manager, const std::vector<std::string> &keys, const T &value)
{
    manager.Load();
    manager.SetList(keys, value);
    manager.Save();
}

/*
测试描述: 服务端配置文件测试，配置证书异常
测试步骤:
    1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    2. 修改 ip 字段为异常值，预期结果2
    3. 调用解析接口，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 修改成功
    3. 解析返回-1
*/
TEST_F(TestMSServer, ParseCommandArgsInvalidCert)
{
    HttpClientParams clientParams;
    HttpServerParams serverParams;
    std::string statusPath;
    // 1. 配置serverTestJsonPath为默认配置内容，内容有效，预期结果1
    std::string serverTestJsonPath = GetMSServerTestJsonPath();
    
    std::vector<std::string> certPath = { "server_tls_items", "client_k8s_tls_items", "client_mindie_tls_items" };
    std::vector<std::string> certType = { "ca_cert", "tls_cert", "tls_key", "tls_crl" };

    // 文件不存在
    for (auto item : certPath) {
        for (auto fileName : certType) {
            std::cout << "item: " << item << std::endl;
            std::cout << "fileName: " << fileName << std::endl;
            // 文件不存在
            {
                CopyDefaultConfig();
                JsonFileManager manager(serverTestJsonPath);
                SetManagerList(manager, {"client_k8s_tls_enable"}, true);
                SetManagerList(manager, {"client_mindie_server_tls_enable"}, true);
                SetManagerList(manager, { item, fileName }, "AAAAA");
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, 0);
                MINDIE::MS::ServerManager serverManager(clientParams);
                serverManager.LoadServer(serverTestJsonPath);
            }
            // 配置项不存在
            {
                CopyDefaultConfig();
                JsonFileManager manager(serverTestJsonPath);
                manager.Load();
                SetManagerList(manager, {"client_k8s_tls_enable"}, true);
                SetManagerList(manager, {"client_mindie_server_tls_enable"}, true);
                manager.EraseList({ item, fileName });
                manager.Save();
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, -1);
            }
            // 配置项非字符串
            {
                CopyDefaultConfig();
                JsonFileManager manager(serverTestJsonPath);
                SetManagerList(manager, {"client_k8s_tls_enable"}, true);
                SetManagerList(manager, {"client_mindie_server_tls_enable"}, true);
                SetManagerList(manager, { item, fileName }, 128); // 128 不合规的文件名
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, -1);
            }
            // 配置项内容异常
            {
                CopyDefaultConfig();
                JsonFileManager manager(serverTestJsonPath);
                SetManagerList(manager, {"client_k8s_tls_enable"}, true);
                SetManagerList(manager, {"client_mindie_server_tls_enable"}, true);
                SetManagerList(manager, { item, fileName }, "testcase");
                int32_t ret = ParseCommandArgs(serverTestJsonPath.c_str(), clientParams, serverParams, statusPath);
                EXPECT_EQ(ret, 0);
            }
        }
    }
}

int32_t SendKubeHttpRequestStub(MINDIE::MS::CrossNodeServer selfObj, const std::string& target,
    boost::beast::http::verb method, const std::string &header, const std::string &reqBody, std::string& responseBody)
{
    std::cout << "SendKubeHttpRequestStub" << std::endl;
    responseBody = g_stubK8sResponseBody;
    std::cout << "g_stubK8sResponseBody: " << g_stubK8sResponseBody << std::endl;
    return g_stubK8sResponseCode;
}

/*
测试描述: 多机初始化正常流程
*/
TEST_F(TestMSServer, TestCrossNodeServerInitTC01)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    HttpClientParams httpClientCfg;
    httpClientCfg.k8sIP = "127.0.0.1";
    httpClientCfg.k8sPort = 6334;

    const std::string statusPath = "ms_status.json";
    auto msg = nodeServer.Init(httpClientCfg, statusPath);

    stub.set(ADDR(MINDIE::MS::HttpClient, SetTlsCtx), ReturnOneStub);
    httpClientCfg.k8sClientTlsItems.tlsEnable = false;
    msg = nodeServer.Init(httpClientCfg, statusPath);

    stub.reset(ADDR(MINDIE::MS::HttpClient, SetTlsCtx));
}

/*
测试描述: 多机部署正常流程
*/
TEST_F(TestMSServer, TestCrossNodeServerTC01)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    auto msg = nodeServer.Deploy(config);

    nodeServer.UpdateInstanceStatus();
    nodeServer.GetDeployStatus();
    nodeServer.Unload();
    nodeServer.MinitoringInstance();
}

/*
测试描述: 多机部署正常流程
*/
TEST_F(TestMSServer, TestCrossNodeServerGetMasterPodIPByLabelTC01)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    auto msg = nodeServer.Deploy(config);

    std::string podIP;
    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);
    {
        g_stubK8sResponseCode = -1;
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        g_stubK8sResponseBody = "aasss";
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        json ranktable;
        ranktable["ssss"] = 1;
        g_stubK8sResponseBody = ranktable.dump();
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        json ranktable;
        ranktable["items"] = 1;
        g_stubK8sResponseBody = ranktable.dump();
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        json ranktable;
        ranktable["items"] = json::array();
        g_stubK8sResponseBody = ranktable.dump();
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        json ranktable;
        ranktable["items"] = json::array();
        json item;
        item["statusxxx"] = 1;
        ranktable["items"].push_back(item);
        g_stubK8sResponseBody = ranktable.dump();
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        json ranktable;
        ranktable["items"] = json::array();
        json item;
        item["status"] = 1;
        ranktable["items"].push_back(item);
        g_stubK8sResponseBody = ranktable.dump();
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        json ranktable;
        ranktable["items"] = json::array();
        json item;
        item["status"] = 1;
        ranktable["items"].push_back(item);
        g_stubK8sResponseBody = ranktable.dump();
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        json ranktable;
        ranktable["items"] = json::array();
        json item;
        item["status"] = json();
        item["status"]["podIP"] = 1;
        ranktable["items"].push_back(item);
        g_stubK8sResponseBody = ranktable.dump();
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, -1);
    }

    {
        g_stubK8sResponseCode = 0;
        json ranktable;
        ranktable["items"] = json::array();
        json item;
        item["status"] = json();
        item["status"]["podIP"] ="127.0.0.1";
        ranktable["items"].push_back(item);
        g_stubK8sResponseBody = ranktable.dump();
        auto ret = nodeServer.GetMasterPodIPByLabel("mindie-server", "mindie-server", podIP);
        EXPECT_EQ(ret, 0);
    }

    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));

    nodeServer.Unload();
}

/*
测试描述: 多机部署与K8S交互失败
*/
TEST_F(TestMSServer, TestCrossNodeServerK8sFailTC01)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);
    g_stubK8sResponseCode = -1;
    auto msg = nodeServer.Deploy(config);
    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
}


/*
测试描述: 多机部署，异常json文件
*/
TEST_F(TestMSServer, TestCrossNodeServerTC02)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string config;
    auto ret = FileToBuffer(inferJson, config, 0777);
    EXPECT_EQ(ret, 0);

    auto msg = nodeServer.Deploy("config");

    nodeServer.Unload();
}


/*
测试描述: 多机部署，异常json server_name 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC04)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除server_name
    {
        json config = json::parse(inferCtx);
        config.erase("server_name");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 server_name
    {
        json config = json::parse(inferCtx);
        config.at("server_name") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 多机部署，异常json replicas 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerReplicasTC01)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 replicas
    {
        json config = json::parse(inferCtx);
        config.erase("replicas");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 replicas
    {
        json config = json::parse(inferCtx);
        config.at("replicas") = "1";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 replicas
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
测试描述: 多机部署，异常json cross_node_num 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerCrossNodeNumTC01)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 cross_node_num
    {
        json config = json::parse(inferCtx);
        config.erase("cross_node_num");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 cross_node_num
    {
        json config = json::parse(inferCtx);
        config.at("cross_node_num") = "1";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 cross_node_num
    {
        json config = json::parse(inferCtx);
        config.at("cross_node_num") = 50;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }
}

/*
测试描述: 多机部署，异常json scheduler 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC05)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json init_delay 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerInitDealyTC01)
{
    CrossNodeServer nodeServer;
    auto inferJson = GetMSDeployTestJsonPath();
    std::string inferCtx;
    auto ret = FileToBuffer(inferJson, inferCtx, 0777);
    EXPECT_EQ(ret, 0);

    // 删除 init_delay
    {
        json config = json::parse(inferCtx);
        config.erase("init_delay");

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);

        nodeServer.Unload();
    }

    // 修改 init_delay
    {
        json config = json::parse(inferCtx);
        config.at("init_delay") = 9;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        
        nodeServer.Unload();
    }

    // 修改 init_delay
    {
        json config = json::parse(inferCtx);
        config.at("init_delay") = 189;

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        
        nodeServer.Unload();
    }

    // 修改 init_delay
    {
        json config = json::parse(inferCtx);
        config.at("init_delay") = "default111";

        auto inferNewCtx = config.dump();
        auto msg = nodeServer.Deploy(inferNewCtx);
        msg = nodeServer.Update(inferNewCtx);
        
        nodeServer.Unload();
    }
}

/*
测试描述: 多机部署，异常json service_type 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC06)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json service_port 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC07)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json resource_requests 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC08)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json resource_requests memory 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC09)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json resource_requests cpu_core 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC10)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json resource_requests npu_type 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC11)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json resource_requests npu_chip_num 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC12)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json mindie_server_config 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC13)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json mindie_server_config mies_install_path 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC14)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json mindie_server_config infer_port 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC15)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json mindie_server_config management_port 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC16)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json mindie_server_config enable_tls 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC17)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json npu_fault_reschedule 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC18)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json termination_grace_period_seconds 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC19)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json max_unavailable 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC20)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json max_unavailable 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerTC21)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json liveness_timeout 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerLivenessTimeoutTC01)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json liveness_failure_threshold 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerLivenessFailureThresholdTC01)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json readiness_timeout 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerReadinessTimeoutTC01)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，异常json readiness_failure_threshold 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerReadinessFailureThresholdTC01)
{
    CrossNodeServer nodeServer;
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
测试描述: 多机部署，FromJson 配置项
*/
TEST_F(TestMSServer, TestCrossNodeServerFromJsonTC01)
{
    CrossNodeServer nodeServer;
    nlohmann::json serverStatus;
    serverStatus["server_name"] = "mindie";
    serverStatus["namespace"] = "mindie";
    serverStatus["replicas"] = 3;
    auto ret = nodeServer.FromJson(serverStatus);
    EXPECT_EQ(ret, -1);
}


TEST_F(TestMSServer, FromDeploymentTC02)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie-server";
    json deploymentStatus;
    json deploymentMetadata;
    deploymentMetadata["annotations"] = {
        {"mindie_port", "1024"},
        {"init_delay", "180"},
        {"liveness_timeout", "60"},
        {"liveness_failure_threshold", "60"},
        {"readiness_timeout", "60"},
        {"readiness_failure_threshold", "60"}
    };
    deploymentMetadata["uid"] = "uid";
    deploymentMetadata["resourceVersion"] = "resourceVersion";
    deploymentMetadata["selfLink"] = "selfLink";
    deploymentMetadata["creationTimestamp"] = "creationTimestamp";
    deploymentMetadata["generation"] = "generation";
    deploymentStatus["metadata"] = deploymentMetadata;

    g_stubK8sResponseBody = deploymentStatus.dump();

    json serverStatus = {
        {"server_name", "mindie-server"},
        {"namespace", "mindie"},
        {"replicas", 1},
        {"use_service", true}
    };

    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);

    int ret = nodeServer.FromJson(serverStatus);
    EXPECT_EQ(ret, 0);
    // 返回异常值
    g_stubK8sResponseCode = -1;
    ret = nodeServer.FromJson(serverStatus);
    EXPECT_EQ(ret, -1);

    nodeServer.Unload();

    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
}

// 获取部署信息失败的情况
TEST_F(TestMSServer, FailedToGetDeployment)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie";
    g_stubK8sResponseBody.clear();

    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);
    int ret = nodeServer.FromDeployment(deploymentName);
    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
    EXPECT_EQ(ret, -1);
}

// 获取部署信息失败的情况
TEST_F(TestMSServer, FailedToGetDeploymentTC01)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie";
    g_stubK8sResponseBody.clear();
    g_stubK8sResponseCode = 1;

    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);
    int ret = nodeServer.FromDeployment(deploymentName);
    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
    EXPECT_EQ(ret, -1);
}

// 接收的响应不是有效的 JSON 格式
TEST_F(TestMSServer, InvalidJsonResponse)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie";
    g_stubK8sResponseBody = "invalid json";

    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);
    int ret = nodeServer.FromDeployment(deploymentName);
    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
    EXPECT_EQ(ret, -1);
}


// JSON 缺少必要的元数据或注释
TEST_F(TestMSServer, MissingAnnotationsTC01)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie";
    json deploymentStatus;
    json deploymentMetadata;
    deploymentMetadata["annotations"] = {};
    deploymentStatus["metadataXXX"] = deploymentMetadata;

    g_stubK8sResponseBody = deploymentStatus.dump();

    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);
    int ret = nodeServer.FromDeployment(deploymentName);
    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
    EXPECT_EQ(ret, -1);
}


// JSON 缺少必要的元数据或注释
TEST_F(TestMSServer, MissingAnnotations)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie";
    json deploymentStatus;
    json deploymentMetadata;
    deploymentMetadata["annotationsXX"] = {};
    deploymentStatus["metadata"] = deploymentMetadata;

    g_stubK8sResponseBody = deploymentStatus.dump();

    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);
    int ret = nodeServer.FromDeployment(deploymentName);
    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
    EXPECT_EQ(ret, -1);
}

// 注释中的值无法正确转换为整数
TEST_F(TestMSServer, InvalidAnnotationValuestc01)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie";
    json deploymentStatus;
    json deploymentMetadata;
    deploymentMetadata["annotations"] = {
        {"mindie_port", "invalid"},
        {"init_delay", "invalid"},
        {"liveness_timeout", "invalid"},
        {"liveness_failure_threshold", "invalid"},
        {"readiness_timeout", "invalid"},
        {"readiness_failure_threshold", "invalid"}
    };
    deploymentStatus["metadata"] = deploymentMetadata;

    g_stubK8sResponseBody = deploymentStatus.dump();

    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);
    int ret = nodeServer.FromDeployment(deploymentName);
    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
    EXPECT_EQ(ret, -1);
}

// 注释中的值无法正确转换为整数
TEST_F(TestMSServer, InvalidAnnotationValues)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie";
    json deploymentStatus;
    json deploymentMetadata;
    deploymentMetadata["annotations"] = {
        {"mindie_port", "1025"},
        {"init_delay", "108"},
        {"liveness_timeout", "10"},
        {"liveness_failure_threshold", "20"},
        {"readiness_timeout", "30"},
        {"readiness_failure_threshold", "xds"}
    };
    deploymentStatus["metadata"] = deploymentMetadata;

    g_stubK8sResponseBody = deploymentStatus.dump();
    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);

    int ret = nodeServer.FromDeployment(deploymentName);
    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
    EXPECT_EQ(ret, -1);
}

// 注释中的值无法正确转换为整数
TEST_F(TestMSServer, InvalidAnnotationValuesTC01)
{
    CrossNodeServer nodeServer;
    std::string deploymentName = "mindie";
    json deploymentStatus;
    json deploymentMetadata;
    
    deploymentMetadata["annotations"] = {
        {"mindie_port", "1025"},
        {"init_delay", "108"},
        {"liveness_timeout", "10"},
        {"liveness_failure_threshold", "20"},
        {"readiness_timeout", "30"},
        {"readiness_failure_threshold", "10"}
    };

    std::vector<std::string> toModify = {
        "mindie_port",
        "init_delay",
        "liveness_timeout",
        "liveness_failure_threshold",
        "readiness_timeout",
        "readiness_failure_threshold",
        "readiness_failure_threshold"
    };
    stub.set(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest), SendKubeHttpRequestStub);

    for (auto key:toModify) {
        nlohmann::json test(deploymentMetadata);
        test["annotations"].erase(key);
        deploymentStatus["metadata"] = test;
        g_stubK8sResponseBody = deploymentStatus.dump();
        int ret = nodeServer.FromDeployment(deploymentName);
        EXPECT_EQ(ret, -1);
    }
    
    for (auto key:toModify) {
        nlohmann::json test(deploymentMetadata);
        test["annotations"][key] = 1;
        deploymentStatus["metadata"] = test;
        g_stubK8sResponseBody = deploymentStatus.dump();
        int ret = nodeServer.FromDeployment(deploymentName);
        EXPECT_EQ(ret, -1);
    }
    
    for (auto key:toModify) {
        nlohmann::json test(deploymentMetadata);
        test["annotations"][key] = "99999999999999999999999999999999999999999999999999999999999999999999991";
        deploymentStatus["metadata"] = test;
        g_stubK8sResponseBody = deploymentStatus.dump();
        int ret = nodeServer.FromDeployment(deploymentName);
        EXPECT_EQ(ret, -1);
    }

    stub.reset(ADDR(MINDIE::MS::CrossNodeServer, SendKubeHttpRequest));
}


TEST_F(TestMSServer, RecoverInstanceTC01)
{
    CrossNodeServer nodeServer;
    InferInstance instacne;
    instacne.restoreState = RestoreState::NONE;
    nodeServer.RecoverInstance(0, instacne);
    nodeServer.CheckInstanceStatus(0, instacne);
    instacne.restoreState = RestoreState::RECREATING;
    nodeServer.RecoverInstance(0, instacne);
    nodeServer.CheckInstanceStatus(0, instacne);
    instacne.restoreState = RestoreState::PENDING;
    nodeServer.RecoverInstance(0, instacne);
    nodeServer.CheckInstanceStatus(0, instacne);
}


Stub g_TlsStub;

int32_t ObtainJsonValueStub(nlohmann::json mainObj, HttpClientParams &clientParams,
    HttpServerParams &httpServerParams)
{
    g_TlsStub.reset(ObtainJsonValue);
    ObtainJsonValue(mainObj, clientParams, httpServerParams);
    httpServerParams.serverTlsItems.tlsEnable = false;
    return 0;
}

int32_t ParseInitConfigStub(MSCtlParams &params)
{
    g_TlsStub.reset(ParseInitConfig);
    ParseInitConfig(params);
    params.clientTlsItems.tlsEnable = false;

    return 0;
}

TEST(TestHttpServer, TestHttpServerTC01)
{
    CopyDefaultConfig();
    auto msctlJson = GetMSClientTestJsonPath();
    auto homePath = GetParentPath(GetParentPath(msctlJson));

    std::cout << "deployer:" << msctlJson << std::endl;
    setenv("HOME", homePath.c_str(), 1);
    std::cout << "HOME=" << homePath << std::endl;
    auto msServerJson = GetMSServerTestJsonPath();

    chmod(const_cast<char*>(msctlJson.c_str()), 0640);
    
    chmod(const_cast<char*>(msServerJson.c_str()), 0640);

    std::cout << "msServerJson=" << msServerJson << std::endl;

    auto lamda = [&]() {
        char arg1[100] = "ms_server";
        char* file = const_cast<char*>(msServerJson.c_str());
        char *args[2] = {arg1, file};
        int32_t argc = 2;
        __main_server__(argc, args);
    };

    g_TlsStub.set(ObtainJsonValue, ObtainJsonValueStub);
    g_TlsStub.set(ParseInitConfig, ParseInitConfigStub);

    std::thread threadObj(lamda); // 创建线程
    EXPECT_TRUE(threadObj.joinable());

    sleep(5);

    char arg1[100] = "msctl";
    char *args1[5] = {arg1, "get", "infer_server", "-n", "mindie-server"};
    int32_t argc = 5;
    __main_client__(argc, args1);

    EXPECT_TRUE(threadObj.joinable());
    threadObj.detach(); // 将线程放入后台运行
}

TEST(TestHttpServer, TestHttpServerWithTlsTC01)
{
    CopyDefaultConfig();
    auto msctlJson = GetMSClientTestJsonPath();
    auto homePath = GetParentPath(GetParentPath(msctlJson));

    std::cout << "deployer:" << msctlJson << std::endl;
    setenv("HOME", homePath.c_str(), 1);
    std::cout << "HOME=" << homePath << std::endl;
    auto msServerJson = GetMSServerTestJsonPath();

    chmod(const_cast<char*>(msctlJson.c_str()), 0640);
    
    chmod(const_cast<char*>(msServerJson.c_str()), 0640);

    std::cout << "msServerJson=" << msServerJson << std::endl;

    auto lamda = [&]() {
        char arg1[100] = "ms_server";
        char* file = const_cast<char*>(msServerJson.c_str());
        char *args[2] = {arg1, file};
        int32_t argc = 2;
        __main_server__(argc, args);
    };

    std::thread threadObj(lamda); // 创建线程
    EXPECT_TRUE(threadObj.joinable());

    sleep(5);

    char arg1[100] = "msctl";
    char *args1[5] = {arg1, "get", "infer_server", "-n", "mindie-server"};
    int32_t argc = 5;
    __main_client__(argc, args1);

    EXPECT_TRUE(threadObj.joinable());
    threadObj.detach(); // 将线程放入后台运行
}