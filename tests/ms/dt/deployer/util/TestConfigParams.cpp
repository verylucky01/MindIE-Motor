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
#include <gmock/gmock.h>
#include <fstream>
#include <string>
#include <utility>
#include <algorithm>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <securec.h>
#include "stub.h"
#include "Helper.h"
#include "ConfigParams.h"
#include "Logger.h"

using namespace std;
using namespace testing;
using ::testing::_;
using ::testing::Return;
using namespace MINDIE::MS;
using json = nlohmann::json;

class ConfigParamsTest : public ::testing::Test {
protected:
    TlsItems tlsConfig;

    void SetUp() override
    {}

    void TearDown() override
    {}
};

/*
测试描述: 将字符串内容写入文件的测试
测试步骤:
    1. 定义有效的文件路径和无效的文件路径。
    2. 调用 DumpStringToFile 方法，并传入有效路径和字符串内容，预期结果1。
    3. 调用 DumpStringToFile 方法，并传入无效路径和字符串内容，预期结果2。
预期结果:
    1. 返回值为 0，字符串内容写入文件成功。
    2. 返回值为 -1，字符串内容写入文件失败。
*/
TEST_F(ConfigParamsTest, DumpStringToFile_TestCases)
{
    string filePathSuccess = "test.txt";
    string filePathFailure = "/invalid/path/test.txt";
    string data = "Hello, World!";

    int32_t resultSuccess = DumpStringToFile(filePathSuccess, data);
    int32_t resultFailure = DumpStringToFile(filePathFailure, data);

    EXPECT_EQ(resultSuccess, 0);
    EXPECT_EQ(resultFailure, -1);
}

/*
测试描述: 验证 IP 地址格式的测试（支持IPv4和IPv6）
测试步骤:
    1. 定义一系列包含有效和无效 IP 地址的测试数据集。
    2. 遍历测试数据集，并调用 IsValidIp 方法。
    3. 当地址格式正确时，预期结果1。
    4. 当地址格式错误时，预期结果2。
预期结果:
    1. IsValidIp 方法调用成功。
    2. IsValidIp 方法调用失败。
*/
TEST_F(ConfigParamsTest, IsValidIp_TestCases)
{
    auto IpTestCases = {
        // IPv4 测试用例
        make_tuple("0.0.0.0", false),
        make_tuple("", false),
        make_tuple("192.168.1.", false),
        make_tuple("192.168", false),
        make_tuple("192.168.1.a", false),
        make_tuple("192.168.1.256", false),
        make_tuple("192.168.1.300", false),
        make_tuple("192.168.1.1", true),
        // IPv6 测试用例
        make_tuple("::", false), // 默认不开启全零监听，不合法
        make_tuple("::1", true),
        make_tuple("2001:db8::1", true),
        make_tuple("2001:db8:0:0:0:0:0:1", true),
        make_tuple("2001:db8::1:2:3:4", true),
        make_tuple("2001:db8::1::2", false), // 多个::
        make_tuple("2001:db8:g::1", false),  // 无效字符
        make_tuple("2001:db8::1:2:3:4:5:6:7", false), // 长度非法
        make_tuple("2001:db8::1:2:3:4:5:6:7:8", false), // 长度非法
    };

    for (const auto &testCase : IpTestCases) {
        const string &ip = get<0>(testCase);
        bool expectedResult = get<1>(testCase);
        bool result = IsValidIp(ip);
        EXPECT_EQ(result, expectedResult) << "Failed for IP: " << ip;
    }
}

/*
测试描述: 验证端口号有效性的测试
测试步骤:
    1. 定义一系列包含有效和无效端口号的测试数据集。
    2. 遍历测试数据集，并调用 IsValidPort 方法。
    3. 当端口号在有效范围内（1024-65535）时，预期结果1。
    4. 当端口号超出有效范围时，预期结果2。
预期结果:
    1. IsValidPort 方法调用成功，返回 true。
    2. IsValidPort 方法调用失败，返回 false。
*/
TEST_F(ConfigParamsTest, IsValidPort_TestCases)
{
    auto portTestCases = {make_tuple(1024, true), make_tuple(1023, false), make_tuple(65536, false)};

    for (const auto &testCase : portTestCases) {
        int64_t port = get<0>(testCase);
        bool expectedResult = get<1>(testCase);
        bool result = IsValidPort(port);
        EXPECT_EQ(result, expectedResult) << "Failed for port: " << port;
    }
}

/*
测试描述: 验证 JSON 字符串有效性的测试
测试步骤:
    1. 定义一系列包含有效和无效 JSON 字符串的测试数据集。
    2. 遍历测试数据集，并调用 IsJsonStringValid 方法。
    3. 当字符串长度满足最小长度要求时，预期结果1。
    4. 当字符串长度不满足最小长度要求时，预期结果2。
预期结果:
    1. IsJsonStringValid 方法调用成功，返回 true。
    2. IsJsonStringValid 方法调用失败，返回 false。
*/
TEST_F(ConfigParamsTest, IsJsonStringValid_TestCases)
{
    auto jsonStringTestCases = {make_tuple(json{{"key1", "value1"}, {"key2", "value2"}}, "key3", 5, false),
        make_tuple(json{{"key1", 123}, {"key2", "value2"}}, "key1", 5, false),
        make_tuple(json{{"key1", "value"}}, "key1", 6, false),
        make_tuple(json{{"key1", string(4097, 'a')}}, "key1", 1, false),
        make_tuple(json{{"key1", "value"}}, "key1", 1, true)};

    for (const auto &testCase : jsonStringTestCases) {
        const json &jsonObj = get<0>(testCase);
        const string &key = get<1>(testCase);
        uint32_t minLen = get<2>(testCase);
        bool expectedResult = get<3>(testCase);
        bool result = IsJsonStringValid(jsonObj, key, minLen);
        EXPECT_EQ(result, expectedResult) << "Failed for key: " << key << " with minLen: " << minLen;
    }
}

/*
测试描述: 验证 JSON 数组有效性的测试
测试步骤:
    1. 定义一系列包含有效和无效 JSON 数组的测试数据集。
    2. 遍历测试数据集，并调用 `IsJsonArrayValid` 方法。
    3. 当数组长度在指定范围内时，预期结果1。
    4. 当数组长度超出指定范围时，预期结果2。
预期结果:
    1. `IsJsonArrayValid` 函数调用成功，返回 `true`。
    2. `IsJsonArrayValid` 函数调用成功，返回 `false`。
*/

TEST_F(ConfigParamsTest, IsJsonArrayValid_TestCases)
{
    auto jsonArrayTestCases = {
        make_tuple(json{{"key1", {1, 2, 3}}, {"key2", {4, 5, 6}}}, "key3", 1, 3, false),
        make_tuple(json{{"key1", {1, 2, 3}}, {"key2", 42}}, "key2", 1, 3, false),
        make_tuple(json{{"key1", {1, 2, 3}}, {"key2", {4, 5, 6, 7}}}, "key2", 1, 3, false),
        make_tuple(json{{"key1", {1, 2, 3}}, {"key2", {4, 5, 6}}}, "key1", 1, 3, true),
    };

    for (const auto &testCase : jsonArrayTestCases) {
        const json &jsonObj = get<0>(testCase);
        const string &key = get<1>(testCase);
        uint32_t minLen = get<2>(testCase);
        uint32_t maxLen = get<3>(testCase);
        bool expectedResult = get<4>(testCase);
        bool result = IsJsonArrayValid(jsonObj, key, minLen, maxLen);
        EXPECT_EQ(result, expectedResult) << "Failed for key: " << key << ", minLen: " << minLen;
    }
}

/*
测试描述: 验证 JSON 整数有效性的测试
测试步骤:
    1. 定义一系列包含有效和无效 JSON 整数的测试数据集。
    2. 遍历测试数据集，并调用 `IsJsonIntValid` 方法。
    3. 当整数值在指定范围内时，预期结果1。
    4. 当整数值超出指定范围时，预期结果2。
预期结果:
    1. `IsJsonIntValid` 函数调用成功，返回 `true`。
    2. `IsJsonIntValid` 函数调用成功，返回 `false`。
*/
TEST_F(ConfigParamsTest, IsJsonIntValid_TestCases)
{
    auto jsonIntTestCases = {make_tuple(json{{"key1", 10}}, "key2", 0, 20, false),
        make_tuple(json{{"key1", "value1"}}, "key1", 0, 20, false),
        make_tuple(json{{"key1", 30}}, "key1", 0, 20, false),
        make_tuple(json{{"key1", 15}}, "key1", 0, 20, true)};

    for (const auto &testCase : jsonIntTestCases) {
        const json &jsonObj = get<0>(testCase);
        const string &key = get<1>(testCase);
        int64_t min = get<2>(testCase);
        int64_t max = get<3>(testCase);
        bool expectedResult = get<4>(testCase);
        bool result = IsJsonIntValid(jsonObj, key, min, max);
        EXPECT_EQ(result, expectedResult);
    }
}

/*
测试描述: 验证 JSON 对象有效性的测试
测试步骤:
    1. 定义一系列包含有效和无效 JSON 对象的测试数据集。
    2. 遍历测试数据集，并调用 `IsJsonObjValid` 方法。
    3. 当对象格式正确时，预期结果1。
    4. 当对象格式错误时，预期结果2。
预期结果:
    1. `IsJsonObjValid` 函数调用成功，返回 `true`。
    2. `IsJsonObjValid` 函数调用成功，返回 `false`。
*/
TEST_F(ConfigParamsTest, IsJsonObjValid_TestCases)
{
    auto JsonObjTestCases = {make_tuple(json{{"name", "John"}, {"age", 30}, {"city", "New York"}}, "address", false),
        make_tuple(json{{"name", "John"}, {"age", 30}, {"city", "New York"}}, "age", false),
        make_tuple(
            json{{"name", {{"first", "John"}, {"last", "Doe"}}}, {"age", 30}, {"city", "New York"}}, "name", true)};

    for (const auto &testCase : JsonObjTestCases) {
        const json &jsonObj = get<0>(testCase);
        const string &key = get<1>(testCase);
        bool expectedResult = get<2>(testCase);
        bool result = IsJsonObjValid(jsonObj, key);
        EXPECT_EQ(result, expectedResult) << "Failed for key: " << key;
    }
}

/*
测试描述: 验证 JSON 布尔值有效性的测试
测试步骤:
    1. 定义一系列包含有效和无效 JSON 布尔值的测试数据集。
    2. 遍历测试数据集，并调用 `IsJsonBoolValid` 方法。
    3. 当布尔值格式正确时，预期结果1。
    4. 当布尔值格式错误时，预期结果2。
预期结果:
    1. `IsJsonBoolValid` 函数调用成功，返回 `true`。
    2. `IsJsonBoolValid` 函数调用成功，返回 `false`。
*/
TEST_F(ConfigParamsTest, IsJsonBoolValid_TestCases)
{
    auto JsonBoolTestCases = {make_tuple(json{{"key1", true}}, "key2", false),
        make_tuple(json{{"key1", "value1"}}, "key1", false),
        make_tuple(json{{"key1", true}}, "key1", true)};

    for (const auto &testCase : JsonBoolTestCases) {
        const json &jsonObj = get<0>(testCase);
        const string &key = get<1>(testCase);
        bool expectedResult = get<2>(testCase);
        bool result = IsJsonBoolValid(jsonObj, key);
        EXPECT_EQ(result, expectedResult) << "Failed for key: " << key;
    }
}

/*
测试描述: 验证回调函数的测试
测试步骤:
    1. 调用 VerifyCallback 方法，并传入 nullptr，预期结果1。
预期结果:
    1. 返回值为 0。
*/
TEST_F(ConfigParamsTest, VerifyCallback_NullInput)
{
    EXPECT_EQ(VerifyCallback(nullptr, nullptr), 0);
}

/*
测试描述: 验证回调函数的测试 - 路径检查失败
测试步骤:
    1. 设置 X509_STORE_CTX 和 VerifyItems 结构体。
    2. 设置 VerifyItems.crlFile 为无效路径。
    3. 使用 Stub 设置 PathCheck 函数返回 false。
    4. 调用 VerifyCallback 方法，预期结果1。
预期结果:
    1. 返回值为 -1。
*/
TEST_F(ConfigParamsTest, VerifyCallback_PathCheckFalse)
{
    X509_STORE_CTX *x509ctx = X509_STORE_CTX_new();
    VerifyItems verifyItems;
    verifyItems.crlFile = "invalid_crl_file.crl";

    Stub stub;
    stub.set(PathCheck, ReturnFalseStub);  // 模拟PathCheck函数返回false
    EXPECT_EQ(VerifyCallback(x509ctx, &verifyItems), -1);
    stub.reset(PathCheck);
    X509_STORE_CTX_cleanup(x509ctx);
    X509_STORE_CTX_free(x509ctx);
}

/*
测试描述: 验证回调函数的测试 - 文件不存在
测试步骤:
    1. 设置 X509_STORE_CTX 和 VerifyItems 结构体。
    2. 设置 VerifyItems.crlFile 为不存在的路径。
    3. 调用 VerifyCallback 方法，预期结果1。
预期结果:
    1. 返回值为 -1。
*/
TEST_F(ConfigParamsTest, VerifyCallback_FileDoesNotExist)
{
    X509_STORE_CTX *x509ctx = X509_STORE_CTX_new();
    VerifyItems verifyItems;
    verifyItems.crlFile = "nonexistent_crl_file.crl";
    EXPECT_EQ(VerifyCallback(x509ctx, &verifyItems), -1);
    X509_STORE_CTX_cleanup(x509ctx);
    X509_STORE_CTX_free(x509ctx);
}

/*
测试描述: 清除解密数据的测试 - 输入数据为 nullptr
测试步骤:
    1. 创建一个包含解密数据的 std::pair。
    2. 当输入数据为 nullptr 时，调用 EraseDecryptedData 方法，预期结果1。
预期结果:
    1. 返回 nullptr 并且大小为 0。
*/
TEST_F(ConfigParamsTest, NullData)
{
    std::pair<char *, int32_t> result = {nullptr, 10};
    EraseDecryptedData(result);
    EXPECT_EQ(result.first, nullptr);
    EXPECT_EQ(result.second, 0);
}

/*
测试描述: 清除解密数据的测试 - 输入数据非 nullptr
测试步骤:
    1. 创建一个包含解密数据的 std::pair。
    2. 当输入数据非 nullptr 时，调用 EraseDecryptedData 方法，预期结果1。
预期结果:
    1. 返回大小为 0 。
*/
TEST_F(ConfigParamsTest, NonNullData)
{
    std::pair<char *, int32_t> result = {new char[5], 5};
    strcpy_s(result.first, result.second, "test");
    EraseDecryptedData(result);
    EXPECT_EQ(result.second, 0);
    EXPECT_STREQ(result.first, nullptr);
    delete[] result.first;
}

/*
测试描述: 解密密码的测试 - 路径检查失败
测试步骤:
    1. 设置一个无效的路径作为 tlsPasswd。
    2. 调用 DecryptPassword 方法，预期结果1。
预期结果:
    1. 返回值为 false，表示解密失败。
*/
TEST_F(ConfigParamsTest, DecryptPassword_PathCheckFailed)
{
    int domainId = 1;
    pair<char *, int32_t> password;
    tlsConfig.tlsPasswd = "/invalid/path";
    EXPECT_FALSE(DecryptPassword(domainId, password, tlsConfig));
}

/*
测试描述: 解密密码的测试 - 文件不存在
测试步骤:
    1. 设置 PathCheck 为总是返回 true 的模拟函数。
    2. 设置 tlsPasswd 为一个不存在的文件路径。
    3. 调用 DecryptPassword 方法，预期结果1。
预期结果:
    1. 返回值为 false，表示解密失败。
*/
TEST_F(ConfigParamsTest, DecryptPassword_FileNotExist)
{
    int domainId = 1;
    pair<char *, int32_t> password;
    Stub stub;
    stub.set(PathCheck, ReturnTrueStub);
    EXPECT_FALSE(DecryptPassword(domainId, password, tlsConfig));
    stub.reset(PathCheck);
}

/*
测试描述: 解密密码的测试 - 解密失败
测试步骤:
    1. 设置 HseCryptor::Decrypt 为总是返回非零值的模拟函数。
    2. 设置 tlsPasswd 为一个实际存在的文件路径。
    3. 调用 DecryptPassword 方法，预期结果1。
预期结果:
    1. 返回值为 false，解密失败。
*/
TEST_F(ConfigParamsTest, DecryptPassword_DecryptFailed)
{
    int domainId = 1;
    pair<char *, int32_t> password;
    std::string exePath = GetExecutablePath();
    std::string parentPath = GetParentPath(GetParentPath(GetParentPath(GetParentPath(exePath))));
    tlsConfig.tlsPasswd = GetAbsolutePath(parentPath, "tests/ms/common/certificate/msctl/security/pass/key_pwd.txt");

    Stub stub;
    EXPECT_FALSE(DecryptPassword(domainId, password, tlsConfig));
}

/*
测试描述: 字符串转整数的测试
测试步骤:
    1. 定义一系列包含有效和无效字符串的测试数据集。
    2. 遍历测试数据集，并调用 `StrToInt` 方法。
    3. 检查返回值。
    4. 当字符串能正确转换为整数时，预期结果1。
    5. 当字符串不能转换为整数时，预期结果2。
预期结果:
    1. `StrToInt` 函数调用成功，返回 `0` 并且转换结果正确。
    2. `StrToInt` 函数调用成功，返回 `-1` 并且转换结果为 `0`。
*/
TEST_F(ConfigParamsTest, StrToInt_TestCases)
{
    for (const auto &testCase : {
             make_tuple("12345", 12345, 0),
             make_tuple("-12345", -12345, 0),
             make_tuple("abcde", 0, -1),
             make_tuple("", 0, -1),
             make_tuple("999999999999999", 0, -1)  // 超出范围
         }) {
        const string &str = get<0>(testCase);
        int64_t expectedNumber = get<1>(testCase);
        int32_t expectedResult = get<2>(testCase);

        int64_t number = 0;
        int32_t result = StrToInt(str, number);

        EXPECT_EQ(result, expectedResult);
        if (result == 0) {
            EXPECT_EQ(number, expectedNumber);
        }
    }
}

/*
测试描述: 获取当前日期时间字符串的测试
测试步骤:
    1. 获取当前系统时间。
    2. 格式化时间为字符串形式。
    3. 调用 GetCurrentDateTimeString 方法，预期结果1。
预期结果:
    1. 当前日期时间字符串与格式化后的日期时间字符串相等。
*/
TEST_F(ConfigParamsTest, GetCurrentDateTimeString_TestCase)
{
    auto now = chrono::system_clock::now();
    time_t nowC = chrono::system_clock::to_time_t(now);
    tm *nowTm = localtime(&nowC);

    stringstream ss;
    ss << put_time(nowTm, "%Y-%m-%d %H:%M");

    string expected = ss.str();
    string actual = GetCurrentDateTimeString();

    std::cout << "expected: " << expected << std::endl;
    std::cout << "actual: " << actual << std::endl;
    EXPECT_NE(actual.find(expected), string::npos);
}

/*
测试描述: 验证正确端口号的测试
测试步骤:
    1. 定义一些列字符串（涵盖有效端口，无效端口）
    2. 调用 IsValidPortString 方法，返回预期结果。
预期结果:
    1. 有效端口返回true[1024-65535]）,无效端口返回false。
*/
TEST_F(ConfigParamsTest, IsValidPortString_TestCases)
{
    EXPECT_TRUE(IsValidPortString("1024"));  // Minimum valid port
    EXPECT_TRUE(IsValidPortString("65535")); // Maximum valid port
    EXPECT_TRUE(IsValidPortString("8080"));  // Common valid port

    EXPECT_FALSE(IsValidPortString("abcd"));  // Non-numeric string
    EXPECT_FALSE(IsValidPortString("123a"));  // Mixed numeric and non-numeric
    EXPECT_FALSE(IsValidPortString(""));      // Empty string

    EXPECT_FALSE(IsValidPortString("1023"));  // Below minimum port
    EXPECT_FALSE(IsValidPortString("65536")); // Above maximum port
    EXPECT_FALSE(IsValidPortString("-1"));    // Negative port

    EXPECT_FALSE(IsValidPortString("00080")); // leading zeros
}

/*
测试描述: 验证给定的JSON对象中某个键对应的值是否是一个有效的双精度浮点数的测试
测试步骤:
    1. 定义一些验证元组，包含 JSON 对象、键、最小值、最大值和预期结果。
    2. 遍历验证元组，调用 IsJsonDoubleValid 方法。
    3. 当值在指定范围内时，预期结果true。
    4. 当值不在指定范围内时，预期结果fase。
预期结果:
    1. 存在有效返回true,否则返回false。
*/
TEST_F(ConfigParamsTest, IsJsonDoubleValid_TestCases)
{
    auto testCases = {
        std::make_tuple(nlohmann::json{{"otherKey", 1.23}}, "missingKey", 0.0, 10.0, false), // Key missing
        std::make_tuple(nlohmann::json{{"key", "notANumber"}}, "key", 0.0, 10.0, false),    // Invalid type
        std::make_tuple(nlohmann::json{{"key", 15.0}}, "key", 0.0, 10.0, false),           // Value out of range
        std::make_tuple(nlohmann::json{{"key", 5.5}}, "key", 0.0, 10.0, true),             // Value in range
        std::make_tuple(nlohmann::json{{"key", 0.0}}, "key", 0.0, 10.0, true),             // Value at min boundary
        std::make_tuple(nlohmann::json{{"key", 10.0}}, "key", 0.0, 10.0, true)             // Value at max boundary
    };

    for (const auto& testCase : testCases) {
        const auto& jsonObj = std::get<0>(testCase);
        const auto& key = std::get<1>(testCase);
        double min = std::get<2>(testCase);
        double max = std::get<3>(testCase);
        bool expectedResult = std::get<4>(testCase);

        EXPECT_EQ(IsJsonDoubleValid(jsonObj, key, min, max), expectedResult)
            << "Failed for key: " << key << ", min: " << min << ", max: " << max;
    }
}

/*
测试描述：验证给定的字符串是否是一个有效的URL路径的测试
测试步骤：
    1. 定义一些验证元组，包含URL字符串和预期结果。
    2. 遍历验证元组，调用 IsValidUrlString 方法。
    3. 当URL字符串符合要求时，预期结果true。
    4. 当URL字符串不符合要求时，预期结果false。
预期结果：
    1. 有效返回true，否则返回false
*/
TEST_F(ConfigParamsTest, IsValidUrlString_TestCases)
{
    auto testCases = {
        std::make_tuple("", false),                          // Empty string
        std::make_tuple("example/path", false),             // Does not start with slash
        std::make_tuple("/example/path?query=1", false),    // Contains invalid characters
        std::make_tuple("/example/path", false),            // Not in whitelist
        std::make_tuple("/example_path-with-dash", false),  // Not in whitelist
        std::make_tuple("/v1/startup", true),               // Valid URL in whitelist
        std::make_tuple("/v1/health", true),                // Valid URL in whitelist
        std::make_tuple("/v1/readiness", true),             // Valid URL in whitelist
        std::make_tuple("/v2/health/ready", true)            // Valid URL in whitelist
    };

    for (const auto& testCase : testCases) {
        const auto& url = std::get<0>(testCase);
        bool expectedResult = std::get<1>(testCase);

        EXPECT_EQ(IsValidUrlString(url), expectedResult)
            << "Failed for URL: " << url;
    }
}