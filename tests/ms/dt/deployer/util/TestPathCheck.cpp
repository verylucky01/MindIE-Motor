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
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <cstdint>
#include <pthread.h>
#include "gtest/gtest.h"
#include "ConfigParams.h"
#include "ConfigParams.cpp"
#include "Logger.cpp"
#include "Helper.h"
#include "stub.h"
#include "Util.cpp"
#include "HttpServer.h"
#include "ServerManager.h"

using namespace MINDIE::MS;
class TestPathCheck : public ::testing::Test {
protected:
    bool isFileExist;

    void SetUp() override
    {
        isFileExist = false;
    }
};

TEST_F(TestPathCheck, EmptyPath)
{
    std::string path;
    EXPECT_FALSE(PathCheck(path, isFileExist));
}

TEST_F(TestPathCheck, PathTooLong)
{
    std::string path(PATH_MAX + 1, 'a');
    EXPECT_FALSE(PathCheck(path, isFileExist));
}

TEST_F(TestPathCheck, DirectoryDoesNotExist)
{
    std::string path("/nonexistent/path/to/file");
    std::string dir = path.substr(0, path.find_last_of('/'));
    Stub stub;
    stub.set(DirectoryExists, ReturnFalseStub);
    EXPECT_FALSE(PathCheck(path, isFileExist));
}

TEST_F(TestPathCheck, RealPathFailure)
{
    std::string path("/path/to/file");
    Stub stub;
    stub.set(stat, ReturnOneStub);
    EXPECT_FALSE(PathCheck(path, isFileExist));
}

TEST_F(TestPathCheck, FailedAbsolutePath)
{
    std::string path;
    EXPECT_FALSE(IsAbsolutePath(path));
    std::string longString(4096, '0'); // 生成长度为4096的字符串，字符全部为0
    path = "/" + longString;
    EXPECT_FALSE(IsAbsolutePath(path));
    path = "a";
    EXPECT_FALSE(IsAbsolutePath(path));
    path = "/$a";
    EXPECT_FALSE(IsAbsolutePath(path));
    path = "/^";
    EXPECT_FALSE(IsAbsolutePath(path));
    path = "/(";
    EXPECT_FALSE(IsAbsolutePath(path));
    path = "/<";
    std::cout << path << std::endl;
    EXPECT_FALSE(IsAbsolutePath(path));
    path = "/_";
    EXPECT_TRUE(IsAbsolutePath(path));
    path = "/-";
    EXPECT_TRUE(IsAbsolutePath(path));
    path = "/.";
    EXPECT_TRUE(IsAbsolutePath(path));
}

TEST_F(TestPathCheck, FileExistsAndOwnerCheckSuccess)
{
    std::string path(".");
    EXPECT_TRUE(PathCheck(path, isFileExist));
    EXPECT_TRUE(isFileExist);
}

TEST_F(TestPathCheck, FileExistsAndOwnerCheckFailure)
{
    std::string path("not_exist");
    EXPECT_TRUE(PathCheck(path, isFileExist));
    EXPECT_FALSE(isFileExist);
}

// DumpStringToFile 不存在的文件
TEST_F(TestPathCheck, DumpStringToFileFail)
{
    std::string path("/path/to/file");
    std::string result;
    auto ret = DumpStringToFile(path, result);
    EXPECT_EQ(ret, -1);
}


// IsValidIp 空ip
TEST_F(TestPathCheck, IsValidIpFail)
{
    std::string ip("");
    auto ret = IsValidIp(ip);
    EXPECT_FALSE(ret);
}

// LoadCertRevokeListFile 返回空
TEST_F(TestPathCheck, LoadCertRevokeListFile_BIO_new)
{
    std::string crlFile;
    Stub stub;
    stub.set(BIO_new, ReturnNullptrStub);
    auto ret = LoadCertRevokeListFile(crlFile);
    EXPECT_EQ(ret, nullptr);
}


// strcpy_s 返回非0
TEST_F(TestPathCheck, LoadCertRevokeListFile_strcpy_s)
{
    std::string crlFile;
    Stub stub;
    stub.set(strcpy_s, ReturnOneStub);
    auto ret = LoadCertRevokeListFile(crlFile);
    EXPECT_EQ(ret, nullptr);
}


// BIO_ctrl 返回非0
TEST_F(TestPathCheck, LoadCertRevokeListFile_BIO_ctrl)
{
    std::string crlFile;
    Stub stub;
    stub.set(BIO_ctrl, ReturnNeOneStub);
    auto ret = LoadCertRevokeListFile(crlFile);
    EXPECT_EQ(ret, nullptr);
}


// PEM_read_bio_X509_CRL 返回非0
TEST_F(TestPathCheck, LoadCertRevokeListFile_PEM_read_bio_X509_CRL)
{
    std::string exeDir = GetParentPath(GetExecutablePath());
    std::string logDir = JoinPathComponents({ exeDir, "log" });
    CreateDirectory(logDir);
    std::string certDir = JoinPathComponents({ exeDir, "cert_dir" });
    CreateDirectory(certDir);
    std::string crlFile = JoinPathComponents({ exeDir, "cert_dir", "tls_crl.crl" });
    CreateFile(crlFile, "tls_crl");

    Stub stub;
    stub.set(PEM_read_bio_X509_CRL, ReturnNullptrStub);
    auto ret = LoadCertRevokeListFile(crlFile);
    EXPECT_EQ(ret, nullptr);
}

TEST_F(TestPathCheck, CheckSubjectName_X509_X509_get_subject_name)
{
    auto ret = VerifyCallback(nullptr, nullptr);
    EXPECT_EQ(ret, 0);
}

TEST_F(TestPathCheck, CheckOwner_UserNotMatch)
{
    struct stat st {};
    std::string filePath = "/fake/path/test.txt";

    // 模拟文件所有者ID与当前用户不同
    st.st_uid = getuid() + 1;  // 故意制造不匹配
    auto ret = CheckOwner(st, filePath);

    EXPECT_FALSE(ret); // 应返回 false，因为 UID 不匹配
}

TEST_F(TestPathCheck, CheckOwner_UserMatch)
{
    struct stat st {};
    std::string filePath = "/fake/path/test.txt";

    // 模拟文件所有者ID与当前用户一致
    st.st_uid = getuid();
    auto ret = CheckOwner(st, filePath);

    EXPECT_TRUE(ret); // 应返回 true，因为 UID 匹配
}

TEST_F(TestPathCheck, DirectoryExists_stat)
{
    Stub stub;
    std::string filePath;
    stub.set(stat, ReturnOneStub);
    auto ret = DirectoryExists(filePath);
    EXPECT_FALSE(ret);
}

TEST_F(TestPathCheck, PermissionCheck_InvalidPermission)
{
    std::string filePath = "/tmp/testfile";
    struct stat buf {};
    uint32_t mode = 0640;
    buf.st_mode = S_IFREG | 0666;
    auto ret = PermissionCheck(filePath, buf, mode);
    EXPECT_FALSE(ret);
}


TEST_F(TestPathCheck, StrToInt_Fail)
{
    int64_t number;
    auto ret = StrToInt("253647586946334221002101219955219971002", number);
    EXPECT_EQ(ret, -1);
}

std::pair<ErrorCode, Response> TestPostServersHandler(
    const boost::beast::http::request<boost::beast::http::string_body> &req)
{
    std::cout << "TestPostServersHandler" << std::endl;
}

TEST_F(TestPathCheck, HttpServer)
{
    MINDIE::MS::HttpClientParams clientParams;
    MINDIE::MS::ServerManager serverManager(clientParams);
    HttpServer server(1);
    std::string serversUrl = "/v1/servers";

    auto ret = server.RegisterPostUrlHandler(serversUrl, TestPostServersHandler);
    EXPECT_EQ(ret, 0);
    ret = server.RegisterPostUrlHandler(serversUrl, TestPostServersHandler);
    EXPECT_EQ(ret, -1);

    ret = server.RegisterGetUrlHandler(serversUrl, TestPostServersHandler);
    EXPECT_EQ(ret, 0);
    ret = server.RegisterGetUrlHandler(serversUrl, TestPostServersHandler);
    EXPECT_EQ(ret, -1);
    ret = server.RegisterDeleteUrlHandler(serversUrl, TestPostServersHandler);
    EXPECT_EQ(ret, 0);
    ret = server.RegisterDeleteUrlHandler(serversUrl, TestPostServersHandler);
    EXPECT_EQ(ret, -1);
}

TEST_F(TestPathCheck, HttpClient)
{
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv13_client);
    TlsItems tlsConfigBak;
    tlsConfigBak.tlsEnable = true;
    std::string exeDir = GetParentPath(GetExecutablePath());
    std::string logDir = JoinPathComponents({ exeDir, "log" });
    CreateDirectory(logDir);
    std::string certDir = JoinPathComponents({ exeDir, "cert_dir" });
    CreateDirectory(certDir);
    tlsConfigBak.caCert = JoinPathComponents({ exeDir, "cert_dir", "ca_cert.crt" });
    CreateFile(tlsConfigBak.caCert, "ca_cert");
    tlsConfigBak.tlsCert = JoinPathComponents({ exeDir, "cert_dir", "tls_cert.crt" });
    CreateFile(tlsConfigBak.tlsCert, "tls_cert");
    tlsConfigBak.tlsKey = JoinPathComponents({ exeDir, "cert_dir", "tls_key.key" });
    CreateFile(tlsConfigBak.tlsKey, "tls_key");
    tlsConfigBak.tlsPasswd = JoinPathComponents({ exeDir, "cert_dir", "tls_crl.crl" });
    CreateFile(tlsConfigBak.tlsPasswd, "tls_crl");

    MINDIE::MS::Request req = {};
    int32_t code;
    std::string responseBody;

    Stub stub;

    {
        HttpClient httpClient;
        stub.set(SSL_CTX_set_ciphersuites, ReturnZeroStub);
        httpClient.Init("127.0.0.1", "32001", tlsConfigBak, true);
        stub.reset(SSL_CTX_set_ciphersuites);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
    }
    {
        HttpClient httpClient;
        stub.set(SSL_CTX_get_ciphers, ReturnFalseStub);
        httpClient.Init("127.0.0.1", "32001", tlsConfigBak, true);
        stub.reset(SSL_CTX_get_ciphers);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
    }
    {
        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfigBak, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
    }
    {
        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfigBak, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
    }
    {
        stub.set(DecryptPassword, ReturnTrueStub);

        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfigBak, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
        stub.reset(DecryptPassword);
    }
    {
        // cert 文件不存在
        TlsItems tlsConfig = tlsConfigBak;
        stub.set(DecryptPassword, ReturnTrueStub);
        tlsConfig.tlsCert =
            GetAbsolutePath(certDir, "msctl/security/certs/no_exist_cert.pem");
        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfig, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
        stub.reset(DecryptPassword);
    }
    {
        // 可能存在熵不足导致随机数生成卡死，故桩掉底层依赖
        // key文件不存在
        TlsItems tlsConfig = tlsConfigBak;
        stub.set(DecryptPassword, ReturnTrueStub);

        tlsConfig.tlsKey =
            GetAbsolutePath(certDir, "msctl/security/keys/cert_no_exist.key.pem");
        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfig, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
        stub.reset(DecryptPassword);
    }
    {
        // 可能存在熵不足导致随机数生成卡死，故桩掉底层依赖
        // key文件目录不存在
        TlsItems tlsConfig = tlsConfigBak;
        stub.set(DecryptPassword, ReturnTrueStub);

        tlsConfig.tlsKey =
            GetAbsolutePath(certDir, "msctl/security/keys_no_exist/cert.key.pem");
        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfig, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
        stub.reset(DecryptPassword);
    }

    {
        // 可能存在熵不足导致随机数生成卡死，故桩掉底层依赖
        // kmcKsfStandby文件目录不存在
        TlsItems tlsConfig = tlsConfigBak;
        tlsConfig.tlsPasswd =
            GetAbsolutePath(certDir, "msctl/security/pass/key_pwd_not_exist.txt");
        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfig, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
    }
    {
        // 可能存在熵不足导致随机数生成卡死，故桩掉底层依赖
        // kmcKsfMaster文件不存在

        TlsItems tlsConfig = tlsConfigBak;
        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfig, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
    }
    {
        // 可能存在熵不足导致随机数生成卡死，故桩掉底层依赖
        // kmcKsfStandby文件目录不存在

        TlsItems tlsConfig = tlsConfigBak;
        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfig, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
    }
    {
        // 可能存在熵不足导致随机数生成卡死，故桩掉底层依赖
        // ftok 返回失败
        TlsItems tlsConfig = tlsConfigBak;
        stub.set(ftok, ReturnNeOneStub);

        HttpClient httpClient;
        httpClient.Init("127.0.0.1", "32001", tlsConfig, true);
        auto ret = httpClient.SendRequest(req, 10, 10, responseBody, code);
        EXPECT_EQ(ret, -1);
        stub.reset(ftok);
    }
}

// 文件夹创建成功
TEST_F(TestPathCheck, CreateDirSuccess)
{
    std::time_t nowC = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); // 防重名
    std::tm *nowTm = std::localtime(&nowC);

    std::stringstream timeStream;
    timeStream << std::put_time(nowTm, "%Y%m%d%H%M%S");

    std::string dirPath = GetParentPath(GetExecutablePath()) +"/createDirSuccess_"+ timeStream.str();
    for (int i = 0; i + 1 < MAX_DEPTH; i++) {   // 1是已经加了一层目录
        dirPath += "/0";
    }
    dirPath += "/log.txt";  // 最终深度为max
    EXPECT_FALSE(PathCheck(dirPath, isFileExist, 0640, true, false)); // 权限0640
    EXPECT_TRUE(PathCheck(dirPath, isFileExist, 0640, true, true)); // 权限0640
    EXPECT_TRUE(PathCheck(dirPath, isFileExist, 0640, true, false)); // 权限0640
}

// 文件夹深度过深，未完全创建，创建失败
TEST_F(TestPathCheck, CreateDirFailed)
{
    std::time_t nowC = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); // 防重名
    std::tm *nowTm = std::localtime(&nowC);

    std::stringstream timeStream;
    timeStream << std::put_time(nowTm, "%Y%m%d%H%M%S");

    std::string dirPath = GetParentPath(GetExecutablePath()) +"/createDirFailed_"+ timeStream.str();
    for (int i = 0; i < MAX_DEPTH; i++) {
        dirPath += "/0";
    }
    dirPath += "/log.txt";    // 最终深度为max+1
    EXPECT_FALSE(PathCheck(dirPath, isFileExist, 0640, true, false)); // 权限0640
    EXPECT_FALSE(PathCheck(dirPath, isFileExist, 0640, true, true)); // 权限0640
    EXPECT_FALSE(PathCheck(dirPath, isFileExist, 0640, true, false)); // 权限0640
}

// realpath创建测试
TEST_F(TestPathCheck, CreateDirReal)
{
    std::time_t nowC = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); // 防重名
    std::tm *nowTm = std::localtime(&nowC);

    std::stringstream timeStream;
    timeStream << std::put_time(nowTm, "%Y%m%d%H%M%S");

    std::string dirPath = GetParentPath(GetExecutablePath()) +"/createRealPath_"+ timeStream.str();
    for (int i = 0; i < MAX_DEPTH; i++) {
        dirPath += "/.";
    }
    dirPath += "/log.txt";      // 传入深度为max+1，最终深度为1
    EXPECT_FALSE(PathCheck(dirPath, isFileExist, 0640, true, false)); // 权限0640
    EXPECT_TRUE(PathCheck(dirPath, isFileExist, 0640, true, true)); // 权限0640
    EXPECT_TRUE(PathCheck(dirPath, isFileExist, 0640, true, false)); // 权限0640
}