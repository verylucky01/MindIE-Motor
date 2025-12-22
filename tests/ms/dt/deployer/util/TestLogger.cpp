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
#include <cstdarg>
#include <sstream>
#include <string>
#include <cstdio>
#include <dirent.h>
#include <iomanip>
#include <chrono>
#include <thread>
#include "gtest/gtest.h"
#include "Logger.h"
#include "hse_cryptor.h"
#include "Helper.h"
#include "stub.h"

using namespace testing;
using namespace MINDIE::MS;

constexpr const char* ENV_LEVEL = "MINDIE_LOG_LEVEL";
constexpr const char* ENV_FILE = "MINDIE_LOG_TO_FILE";
constexpr const char* ENV_STDOUT = "MINDIE_LOG_TO_STDOUT";
constexpr const char* ENV_PATH = "MINDIE_LOG_PATH";
constexpr const char* ENV_VERBOSE = "MINDIE_LOG_VERBOSE";
constexpr const char* ENV_ROTATE = "MINDIE_LOG_ROTATE";
constexpr const char* ENV_LEVEL_OLD = "MINDIEMS_LOG_LEVEL";
constexpr const char* ENV_HOME = "HOME";
class LoggerTest : public ::testing::Test {
protected:
    Logger logger;
    std::stringstream ss;
    MINDIE::MS::DefaultLogConfig defaultLogConfig;

    static std::time_t GetTestTimeStampNow()
    {
        return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }

    static std::string GetLocalTimeNow()
    {
        std::time_t nowC = GetTestTimeStampNow();
        std::tm *nowTm = std::localtime(&nowC);

        std::stringstream timeStream;
        timeStream << std::put_time(nowTm, "%Y%m%d%H%M%S");
        return timeStream.str();
    }

    void SetUp() override
    {
        unsetenv(ENV_LEVEL);
        unsetenv(ENV_FILE);
        unsetenv(ENV_STDOUT);
        unsetenv(ENV_PATH);
        unsetenv(ENV_VERBOSE);
        unsetenv(ENV_ROTATE);
        unsetenv(ENV_LEVEL_OLD);
        logger.mMaxLogFileSize = DEFAULT_MAX_LOG_FILE_SIZE * 1024 * 1024;  // 1024表示转换为MB
        logger.mMaxLogFileNum = DEFAULT_MAX_LOG_FILE_NUM;
    }

    void TearDown() override
    {}
};

/*
测试描述: 日志字符串过滤测试
测试步骤:
    1. 定义一系列测试数据集。
    2. 遍历测试数据集并调用 FilterLogStr 方法。
    3. 检查过滤后的结果。
预期结果:
    1. 当输入包含控制字符时，返回的字符串应为空。
    2. 当输入包含空格和换行符时，返回的字符串应仅包含空格。
    3. 当输入为普通文本时，返回的字符串应与输入相同。
    4. 当输入为空字符串时，返回的字符串也应为空。
*/
TEST_F(LoggerTest, FilterLogStr_TestCases)
{
    const std::vector<std::pair<std::string, std::string>> testCases = {
        {"\t\n\v\f\r\b\u007F", ""},
        {"  \t \n  ", " "},
        {"Hello, World!", "Hello, World!"},
        {"", ""}
    };

    for (const auto &testCase : testCases) {
        const std::string &input = testCase.first;
        const std::string &expectedOutput = testCase.second;
        std::string output = FilterLogStr(input);

        EXPECT_EQ(output, expectedOutput) << "Failed for input: " << input;
    }
}

/*
测试描述: 记录错误日志
测试步骤:
    1. 设置 OutputOption。
    2. 调用 Init 方法初始化日志记录器。
    3. 调用 Log 方法，并传入错误级别的日志消息。
    4. 检查返回值。
预期结果:
    1. 返回值为 0，表示日志记录成功。
*/
TEST_F(LoggerTest, LogError)
{
    defaultLogConfig.option.toFile = false;
    defaultLogConfig.option.toStdout = true;
    logger.Init(defaultLogConfig, {}, "");
    int32_t result = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "This is an error message");
    EXPECT_EQ(result, 0);
}

/*
测试描述: LogError，ret = 0
测试步骤:
    1. 设置 OutputOption。
    2. 调用 Init 方法初始化日志记录器。
    3. 调用 Log 方法，并传入错误级别的日志消息， 消息语句过长。
    4. 检查返回值。
预期结果:
    1. 返回值为 -1，表示日志记录失败
*/
TEST_F(LoggerTest, LogMessageExceedingMaxSize)
{
    defaultLogConfig.option.toFile = false;
    defaultLogConfig.option.toStdout = true;
    defaultLogConfig.logLevel = "DEBUG";
    defaultLogConfig.maxLogStrSize = 19; // 19: max string size of a log message
    std::string longMessage(LOG_MSG_STR_SIZE_MAX, 'A');
    logger.Init(defaultLogConfig, {}, "");
    int32_t result = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_DEBUG, longMessage.c_str());
    EXPECT_EQ(result, -1);
}

/*
测试描述: 测试LogLevel新环境变量的获取
测试步骤:
    1. LogLevel环境变量中有多个ms的value，最后一个value不为null。
    2. LogLevel环境变量中有多个ms的value，最后一个value为null。
    3. LogLevel环境变量中不含key为ms的value，含有globalValue
预期结果:
    1. 使用最后一个ms的value值
    2. 日志功能不开启
    3. 使用global value值
*/
TEST_F(LoggerTest, LogEnvStrLevelNew)
{
    defaultLogConfig.option.toFile = false;
    defaultLogConfig.option.toStdout = true;
    defaultLogConfig.logLevel = "DEBUG";
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    std::string oldLevelStr = "ERROR";
    {
        std::string levelStr = "debug;  ams: ; llm:  CRitical;  benchmark :  eRRor;   ms  :   warNiNg";
        setenv(ENV_LEVEL, levelStr.c_str(), 1);
        setenv(ENV_LEVEL_OLD, oldLevelStr.c_str(), 1);
        std::cout << ENV_LEVEL << ":" << levelStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_LEVEL);
        unsetenv(ENV_LEVEL_OLD);
        EXPECT_EQ(logger.mIsLogEnable, true);
        EXPECT_EQ(logger.mLogLevel, LogLevel::MINDIE_LOG_WARN);
    }
    {
        std::string levelStr = "debug;  ams: ; llm:  CRitical;  benchmark :  eRRor;   ms  :   warNiNg";
        setenv(ENV_LEVEL, levelStr.c_str(), 1);
        setenv(ENV_LEVEL_OLD, oldLevelStr.c_str(), 1);
        std::cout << ENV_LEVEL << ":" << levelStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_LEVEL);
        unsetenv(ENV_LEVEL_OLD);
        EXPECT_EQ(logger.mIsLogEnable, true);
        EXPECT_EQ(logger.mLogLevel, LogLevel::MINDIE_LOG_WARN);
    }
    {
        std::string levelStr = "debug;  ams: ; llm:  CRitical;  benchmark :  eRRor;   ms  :   warNiNg";
        setenv(ENV_LEVEL, levelStr.c_str(), 1);
        setenv(ENV_LEVEL_OLD, oldLevelStr.c_str(), 1);
        std::cout << ENV_LEVEL << ":" << levelStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_LEVEL);
        unsetenv(ENV_LEVEL_OLD);
        EXPECT_EQ(logger.mIsLogEnable, true);
        EXPECT_EQ(logger.mLogLevel, LogLevel::MINDIE_LOG_WARN);
    }
}

/*
测试描述: 测试LogLevel环境变量和配置文件配置的兼容性
测试步骤:
    1. LogLevel新环境变量为空，旧环境变量不为空。
    2. LogLevel新环境变量为空，LogLevel旧环境变量为空, 配置文件不为空
    3. LogLevel新环境变量为空，LogLevel旧环境变量为空, 配置文件为空
    4. LogLevel新环境变量为空，LogLevel旧环境变量为空, 配置文件为无效值
预期结果:
    1. 使用旧环境变量的值
    2. 使用配置文件的值
    3. 使用默认级别INFO
    4. 使用默认级别INFO
*/
TEST_F(LoggerTest, LogEnvStrLevelConfigCompatibility)
{
    defaultLogConfig.option = {false, true};
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    std::string oldLevelStr = "ERROR";
    {
        unsetenv(ENV_LEVEL);
        setenv(ENV_LEVEL_OLD, oldLevelStr.c_str(), 1);
        std::cout << ENV_LEVEL_OLD << ":" << oldLevelStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_LEVEL);
        unsetenv(ENV_LEVEL_OLD);
        EXPECT_EQ(logger.mIsLogEnable, true);
        EXPECT_EQ(logger.mLogLevel, LogLevel::MINDIE_LOG_ERROR);
    }
    {
        unsetenv(ENV_LEVEL);
        unsetenv(ENV_LEVEL_OLD);
        nlohmann::json configObj = {
            {"log_info", {
                {"log_level", "DEBUG"},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        EXPECT_EQ(logger.mIsLogEnable, true);
        EXPECT_EQ(logger.mLogLevel, LogLevel::MINDIE_LOG_DEBUG);
    }
    {
        unsetenv(ENV_LEVEL);
        unsetenv(ENV_LEVEL_OLD);
        logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(logger.mIsLogEnable, true);
        EXPECT_EQ(logger.mLogLevel, LogLevel::MINDIE_LOG_INFO);
    }
    {
        unsetenv(ENV_LEVEL);
        unsetenv(ENV_LEVEL_OLD);
        nlohmann::json configObj = {
            {"log_info", {
                {"log_level", "INVALID"},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        EXPECT_EQ(logger.mIsLogEnable, true);
        EXPECT_EQ(logger.mLogLevel, LogLevel::MINDIE_LOG_INFO);
    }
}

/*
测试描述: 测试toStdout环境变量的获取
测试步骤:
    1. toStdout环境变量中有多个ms的value，最后一个value为true。
    2. toStdout环境变量中有多个ms的value，最后一个value为0。
    3. toStdout环境变量中不含key为ms的value，含有globalValue
预期结果:
    1. 使用最后一个ms的value值
    2. 使用最后一个ms的value值
    3. 使用global value值
*/
TEST_F(LoggerTest, LogEnvStrTostdout)
{
    defaultLogConfig.option.toFile = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    {
        std::string tostdoutStr = "ture;  ms :   false; llm:  true;  benchmark :  false;   ms  :   true";
        setenv(ENV_STDOUT, tostdoutStr.c_str(), 1);
        std::cout << ENV_STDOUT << ":" << tostdoutStr << std::endl;
        logger.Init(defaultLogConfig, "", "");
        unsetenv(ENV_STDOUT);
        EXPECT_EQ(logger.mToStdout, true);
    }
    {
        std::string tostdoutStr = "ture;  ms :   true; llm:  true;  benchmark :  false;   ms  :   0  ";
        setenv(ENV_STDOUT, tostdoutStr.c_str(), 1);
        std::cout << ENV_STDOUT << ":" << tostdoutStr << std::endl;
        logger.Init(defaultLogConfig, "", "");
        unsetenv(ENV_STDOUT);
        EXPECT_EQ(logger.mToStdout, false);
    }
    {
        std::string tostdoutStr = "ture;  ; llm:  true;  benchmark :  false ; 0 ; 1   ";
        setenv(ENV_STDOUT, tostdoutStr.c_str(), 1);
        std::cout << ENV_STDOUT << ":" << tostdoutStr << std::endl;
        logger.Init(defaultLogConfig, "", "");
        unsetenv(ENV_STDOUT);
        EXPECT_EQ(logger.mToStdout, true);
    }
}

/*
测试描述: 测试toStdout环境变量和配置文件的兼容性
测试步骤:
    1. toStdout环境变量设置1打屏， 配置文件设置false不打屏
    2. toStdout环境变量不设置， 配置文件设置true打屏
    3. toStdout环境变量不设置， 配置文件设置无效值
    4. toStdout环境变量不设置， 配置文件不设置
预期结果:
    1. 使用环境变量， 打屏
    2. 使用配置文件， 打屏
    3. 使用默认配置， 不打屏
    4. 使用默认配置， 不打屏
*/
TEST_F(LoggerTest, TestToStdoutEnvConfigCompatibility)
{
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.option.toFile = false;
    defaultLogConfig.maxLogStrSize = 512;  // 512: max string size of a log message
    {
        std::string tostdoutStr = "ms : 1";
        setenv(ENV_STDOUT, tostdoutStr.c_str(), 1);
        std::cout << ENV_STDOUT << ":" << tostdoutStr << std::endl;
        nlohmann::json configObj = {
            {"log_info", {
                {"to_stdout", "false"},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        unsetenv(ENV_STDOUT);
        EXPECT_EQ(logger.mToStdout, true);
    }
    {
        nlohmann::json configObj = {
            {"log_info", {
                {"to_stdout", true},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        EXPECT_EQ(logger.mToStdout, true);
    }
    {
        nlohmann::json configObj = {
            {"log_info", {
                {"to_stdout", "trueee"},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        EXPECT_EQ(logger.mToStdout, false);
    }
    {
        logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(logger.mToStdout, false);
    }
}


/*
测试描述: 测试toFile环境变量的获取
测试步骤:
    1. toFile环境变量中有多个ms的value，最后一个value为true。
    2. toFile环境变量中有多个ms的value，最后一个value为0。
    3. toFile环境变量中不含key为ms的value，含有globalValue
预期结果:
    1. 使用最后一个ms的value值
    2. 使用最后一个ms的value值
    3. 使用global value值
*/
TEST_F(LoggerTest, LogEnvStrTofile)
{
    defaultLogConfig.maxLogStrSize = 512;  // 512: max string size of a log message
    {
        std::string tofileStr = "ture;  ms :   false; llm:  true;  benchmark :  false;   ms  :   true";
        setenv(ENV_FILE, tofileStr.c_str(), 1);
        std::cout << ENV_FILE << ":" << tofileStr << std::endl;
        logger.Init(defaultLogConfig, "", "");
        unsetenv(ENV_FILE);
        EXPECT_EQ(logger.mToFile, true);
    }
    {
        std::string tofileStr = "ture;  ms :   true; llm:  true;  benchmark :  false;   ms  :   0  ";
        setenv(ENV_FILE, tofileStr.c_str(), 1);
        std::cout << ENV_FILE << ":" << tofileStr << std::endl;
        logger.Init(defaultLogConfig, "", "");
        unsetenv(ENV_FILE);
        EXPECT_EQ(logger.mToFile, false);
    }
    {
        std::string tofileStr = "ture;  ; llm:  true;  benchmark :  false ; 0 ; 1   ";
        setenv(ENV_FILE, tofileStr.c_str(), 1);
        std::cout << ENV_FILE << ":" << tofileStr << std::endl;
        logger.Init(defaultLogConfig, "", "");
        unsetenv(ENV_FILE);
        EXPECT_EQ(logger.mToFile, true);
    }
}

/*
测试描述: 测试toFile环境变量和配置文件的兼容性
测试步骤:
    1. toFile环境变量设置false， 配置文件设置true
    2. toFile环境变量不设置， 配置文件设置false
    3. toFile环境变量不设置， 配置文件设置无效值
    4. toFile环境变量不设置， 配置文件不设置
预期结果:
    1. 使用环境变量， 不落盘
    2. 使用配置文件， 不落盘
    3. 使用默认配置， 落盘
    4. 使用默认配置， 落盘
*/
TEST_F(LoggerTest, TestTofileEnvConfigCompatibility)
{
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.maxLogStrSize = 512;  // 512: max string size of a log message
    {
        std::string tostdoutStr = "ms : 0";
        setenv(ENV_FILE, tostdoutStr.c_str(), 1);
        std::cout << ENV_FILE << ":" << tostdoutStr << std::endl;
        nlohmann::json configObj = {
            {"log_info", {
                {"to_file", "true"},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        unsetenv(ENV_FILE);
        EXPECT_EQ(logger.mToFile, false);
    }
    {
        nlohmann::json configObj = {
            {"log_info", {
                {"to_file", false},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        EXPECT_EQ(logger.mToFile, false);
    }
    {
        nlohmann::json configObj = {
            {"log_info", {
                {"to_file", "trueee"},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        EXPECT_EQ(logger.mToFile, true);
    }
    {
        logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(logger.mToFile, true);
    }
}

/*
测试描述: 测试verbose环境变量的获取
测试步骤:
    1. verbose环境变量中有多个ms的value，最后一个value为true。
    2. verbose环境变量中有多个ms的value，最后一个value为0。
    3. verbose环境变量中不含key为ms的value，含有globalValue
    4. verbose环境变量取值不合法
预期结果:
    1. 使用最后一个ms的value值
    2. 使用最后一个ms的value值
    3. 使用global value值
    4. 使用默认值
*/
TEST_F(LoggerTest, LogEnvStrVerbose)
{
    defaultLogConfig.option.toFile = false;
    defaultLogConfig.option.toStdout = true;
    defaultLogConfig.maxLogStrSize = 512;  // 512: max string size of a log message
    {
        std::string verboseStr = "ture;  ms :   false; llm:  true;  benchmark :  false;   ms  :   true";
        setenv(ENV_VERBOSE, verboseStr.c_str(), 1);
        std::cout << ENV_VERBOSE << ":" << verboseStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_VERBOSE);
        EXPECT_EQ(logger.GetMIsLogVerbose(), true);
    }
    {
        std::string verboseStr = "ture;  ms :   true; llm:  true;  benchmark :  false;   ms  :   0  ";
        setenv(ENV_VERBOSE, verboseStr.c_str(), 1);
        std::cout << ENV_VERBOSE << ":" << verboseStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");        unsetenv(ENV_VERBOSE);
        EXPECT_EQ(logger.GetMIsLogVerbose(), false);
    }
    {
        std::string verboseStr = "ture;  ; llm:  true;  benchmark :  false ; 0 ; 1   ";
        setenv(ENV_VERBOSE, verboseStr.c_str(), 1);
        std::cout << ENV_VERBOSE << ":" << verboseStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_VERBOSE);
        EXPECT_EQ(logger.GetMIsLogVerbose(), true);
    }
    {
        bool verboseInit = logger.GetMIsLogVerbose();
        std::string verboseStr = "ture;  ; llm:  true;  benchmark :  false ; 0 ; 1; ms:trueabc";
        setenv(ENV_VERBOSE, verboseStr.c_str(), 1);
        std::cout << ENV_VERBOSE << ":" << verboseStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_VERBOSE);
        EXPECT_EQ(logger.GetMIsLogVerbose(), verboseInit);
    }
}


/*
测试描述: 测试 MINDIE_LOG_PATH 环境变量的获取
测试步骤:
    1. MINDIE_LOG_PATH 中无ms路径， 统一路径为HOME之下的绝对路径且有效， 以~开头
    2. MINDIE_LOG_PATH 为绝对路径且有效
    3. MINDIE_LOG_PATH 为空
预期结果:
    1. 日志模块初始化成功， 落盘目录正确
    2. 日志模块初始化成功， 落盘目录正确
    3. 路径有效， 日志模块初始化成功， 落盘目录正确
*/
TEST_F(LoggerTest, LogEnvStrAbsolutePathInitSuccess)
{
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    const char* originalHomeEnv = std::getenv("HOME");
    std::string homeEnv = "/root";
    setenv(ENV_HOME, homeEnv.c_str(), 1);
    Stub stub;
    stub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
    {
        std::string logPath = "~/testMsLogPath   ; rt: ./";
        setenv(ENV_PATH, logPath.c_str(), 1);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, 0);
        unsetenv(ENV_PATH);
        EXPECT_EQ(logger.mLogPath[LogType::RUN].find("/root/testMsLogPath/log/debug/mindie-ms"), 0);
        EXPECT_EQ(logger.mLogPath[LogType::OPERATION].find("/root/testMsLogPath/log/security/mindie-ms"), 0);
    }
    {
        std::string logPath = "/home/testMsLogPath";
        unsetenv(ENV_PATH);
        setenv(ENV_PATH, logPath.c_str(), 1);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, 0);
        unsetenv(ENV_PATH);
        EXPECT_EQ(logger.mLogPath[LogType::RUN].find("/home/testMsLogPath/log/debug/mindie-ms"), 0);
        EXPECT_EQ(logger.mLogPath[LogType::OPERATION].find("/home/testMsLogPath/log/security/mindie-ms"), 0);
    }
    {
        std::string logPath = "";
        unsetenv(ENV_PATH);
        setenv(ENV_PATH, logPath.c_str(), 1);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, 0);
        unsetenv(ENV_PATH);
        EXPECT_EQ(logger.mLogPath[LogType::RUN].find("/root/mindie/log/debug/mindie-ms"), 0);
        EXPECT_EQ(logger.mLogPath[LogType::OPERATION].find("/root/mindie/log/security/mindie-ms"), 0);
    }
    stub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
    if (originalHomeEnv == nullptr) {
        unsetenv(ENV_HOME);
    } else {
        setenv("HOME", originalHomeEnv, 1);
    }
}

/*
测试描述: 测试 MINDIE_LOG_PATH 环境变量的获取
测试步骤:
    1. MINDIE_LOG_PATH 为相对路径且有效
    2. MINDIE_LOG_PATH 中有多个ms路径， 最后一个ms路径为相对路径且有效
    3. MINDIE_LOG_PATH 包含非法字符
预期结果:
    1. 日志模块初始化成功， 落盘目录正确
    2. 日志模块初始化成功， 落盘目录正确， 取最后一个路径
    3. 日志模块初始化成功， 落盘到默认目录
*/
TEST_F(LoggerTest, LogEnvStrRelativePathInitSuccess)
{
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    const char* originalHomeEnv = std::getenv("HOME");
    std::string homeEnv = "/root";
    unsetenv(ENV_HOME);
    setenv(ENV_HOME, homeEnv.c_str(), 1);
    Stub stub;
    stub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
    {
        std::string logPath = "testMsLogPath";
        unsetenv(ENV_PATH);
        setenv(ENV_PATH, logPath.c_str(), 1);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, 0);
        unsetenv(ENV_PATH);
        EXPECT_EQ(logger.mLogPath[LogType::RUN].find("/root/mindie/log/testMsLogPath/log/debug/mindie-ms"), 0);
        EXPECT_EQ(logger.mLogPath[LogType::OPERATION].find("/root/mindie/log/testMsLogPath/log/security/mindie-ms"), 0);
    }
    {
        std::string logPath = "  ms : ./; rt: ./rtLogPath  ; ms :  ./testMsLogPath";
        unsetenv(ENV_PATH);
        setenv(ENV_PATH, logPath.c_str(), 1);;
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, 0);
        unsetenv(ENV_PATH);
        EXPECT_EQ(logger.mLogPath[LogType::RUN].find("/root/mindie/log/testMsLogPath/log/debug/mindie-ms"), 0);
        EXPECT_EQ(logger.mLogPath[LogType::OPERATION].find("/root/mindie/log/testMsLogPath/log/security/mindie-ms"), 0);
    }
    {
        std::string logPath = ",,,;,,;::";
        unsetenv(ENV_PATH);
        setenv(ENV_PATH, logPath.c_str(), 1);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, 0);
        unsetenv(ENV_PATH);
        EXPECT_EQ(logger.mLogPath[LogType::RUN].find("/root/mindie/log/debug/mindie-ms"), 0);
        EXPECT_EQ(logger.mLogPath[LogType::OPERATION].find("/root/mindie/log/security/mindie-ms"), 0);
    }
    stub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
    if (originalHomeEnv == nullptr) {
        unsetenv(ENV_HOME);
    } else {
        setenv("HOME", originalHomeEnv, 1);
    }
}


/*
测试描述: 测试 MINDIE_LOG_PATH 环境变量的获取
测试步骤:
    1. MINDIE_LOG_PATH 为HOME之下的相对路径， 但HOME环境变量不存在
    2. MINDIE_LOG_PATH 路径长度超过最大限度
预期结果:
    1. 路径为无效路径， 日志模块初始化失败
    2. 路径为无效路径， 日志模块初始化失败
*/
TEST_F(LoggerTest, LogEnvStrPathInitFailed)
{
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    const char* originalHomeEnv = std::getenv("HOME");
    {
        std::string logPath = "./testMsLogPath";
        unsetenv(ENV_PATH);
        unsetenv(ENV_HOME);
        setenv(ENV_PATH, logPath.c_str(), 1);
        Stub stub;
        stub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, -1);
        stub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
        unsetenv(ENV_PATH);
    }
    {
        std::string logPath(256, 'a'); // 256: Length of log path string
        std::string homeEnv = "/root";
        unsetenv(ENV_PATH);
        unsetenv(ENV_HOME);
        setenv(ENV_PATH, logPath.c_str(), 1);
        setenv(ENV_HOME, homeEnv.c_str(), 1);
        Stub stub;
        stub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, -1);
        stub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
        unsetenv(ENV_PATH);
    }
    if (originalHomeEnv == nullptr) {
        unsetenv(ENV_HOME);
    } else {
        setenv("HOME", originalHomeEnv, 1);
    }
}


/*
测试描述: 测试 MINDIE_LOG_PATH 环境变量的获取
测试步骤:
    1. 无HOME, MINDIE_LOG_PATH 环境变量， 且初始化输入路径为空
预期结果:
    1. 路径为无效路径， 日志模块初始化失败
*/
TEST_F(LoggerTest, LogNoEnvStrPathInitFailed)
{
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    const char* originalHomeEnv = std::getenv("HOME");
    {
        unsetenv(ENV_PATH);
        unsetenv(ENV_HOME);
        Stub stub;
        stub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
        auto result = logger.Init(defaultLogConfig, {}, "");
        stub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
        EXPECT_EQ(result, -1);
    }
    if (originalHomeEnv == nullptr) {
        unsetenv(ENV_HOME);
    } else {
        setenv("HOME", originalHomeEnv, 1);
    }
}


/*
测试描述: 测试 MINDIE_LOG_PATH 环境变量和配置文件路径的兼容性
测试步骤:
    1. 环境变量设置有效路径， 配置文件设置有效路径
    2. 环境变量为空， 配置文件设置有效路径
预期结果:
    1. 使用环境变量路径
    2. 使用配置文件路径
*/
TEST_F(LoggerTest, LogEnvConfigFileCompatibility)
{
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    const char* originalHomeEnv = std::getenv("HOME");
    std::string homeEnv = "/root";
    unsetenv(ENV_HOME);
    setenv(ENV_HOME, homeEnv.c_str(), 1);
    Stub stub;
    stub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
    {
        std::string logPath = "/root/testMsLogPathEnv";
        setenv(ENV_PATH, logPath.c_str(), 1);
        nlohmann::json configObj = {
            {"log_info", {
                {"run_log_path", "/root/testMsLogPathFile"},
                {"max_log_file_num", "/root/testMsLogPathFile"},
            }}};
        auto result = logger.Init(defaultLogConfig, configObj, "");
        EXPECT_EQ(result, 0);
        unsetenv(ENV_PATH);
        EXPECT_EQ(logger.mLogPath[LogType::RUN].find("/root/testMsLogPathEnv/log/debug/mindie-ms"), 0);
        EXPECT_EQ(logger.mLogPath[LogType::OPERATION].find("/root/testMsLogPathEnv/log/security/mindie-ms"), 0);
    }
    {
        unsetenv(ENV_PATH);
        nlohmann::json configObj = {
            {"log_info", {
                {"run_log_path", "/root/testMsLogPathFile"},
                {"operation_log_path", "/root/testMsLogPathFile"},
            }}};
        auto result = logger.Init(defaultLogConfig, configObj, "");
        EXPECT_EQ(result, 0);
        unsetenv(ENV_PATH);
        EXPECT_EQ(logger.mLogPath[LogType::RUN].find("/root/testMsLogPathFile"), 0);
        EXPECT_EQ(logger.mLogPath[LogType::OPERATION].find("/root/testMsLogPathFile"), 0);
    }
    stub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
    if (originalHomeEnv == nullptr) {
        unsetenv(ENV_HOME);
    } else {
        setenv("HOME", originalHomeEnv, 1);
    }
}

/*
测试描述: 测试rotate环境变量的获取
测试步骤:
    1. rotate环境变量中不含key为ms的value，也不含globalValue
    2. rotate环境变量中有多个ms的value，最后一个value不为空。
    3. rotate环境变量中不含key为ms的value，含有globalValue
预期结果:
    1. 使用默认的配置值
    2. 使用最后一个ms的value值
    3. 使用global value值
*/
TEST_F(LoggerTest, LogEnvStrRotate)
{
    uint32_t mByte = 1024 * 1024; // 1024表示转换为MB
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    {
        std::string rotateStr =
            "benchmark : -fs 58 -s weekly -r 9 ;   server : -s daily -s monthly -fs 55 -r 6 ; llm: -s 5 -fs 6 -r 7";
        setenv(ENV_ROTATE, rotateStr.c_str(), 1);
        std::cout << ENV_ROTATE << ":" << rotateStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_ROTATE);
        EXPECT_EQ(logger.mMaxLogFileSize, DEFAULT_MAX_LOG_FILE_SIZE * mByte);
        EXPECT_EQ(logger.mMaxLogFileNum, DEFAULT_MAX_LOG_FILE_NUM);
    }
    {
        std::string rotateStr = "-fs 58 -s weekly -r 9 ;   ms : -s daily -s monthly -fs 55 -r 6 ; llm: -s 5 -fs 6 -r 7";
        setenv(ENV_ROTATE, rotateStr.c_str(), 1);
        std::cout << ENV_ROTATE << ":" << rotateStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_ROTATE);
        EXPECT_EQ(logger.mMaxLogFileSize, 55 * mByte);  // 环境变量值为55
        EXPECT_EQ(logger.mMaxLogFileNum, 6);    // 环境变量值为6
    }
    {
        std::string rotateStr =
            "-fs 58 -s weekly -s yeARly -s 57 -r 9 ;   server : -s daily -s monthly -fs 55 -r 6 ; llm: -s 5 -fs 6 -r 7";
        setenv(ENV_ROTATE, rotateStr.c_str(), 1);
        std::cout << ENV_ROTATE << ":" << rotateStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_ROTATE);
        EXPECT_EQ(logger.mMaxLogFileSize, 58 * mByte);  // 环境变量值为58
        EXPECT_EQ(logger.mMaxLogFileNum, 9);    // 环境变量值为9
    }
}

/*
测试描述: 测试rotate环境变量的获取
测试步骤:
    1.. rotate环境变量中-fs, -s, -r 数值超过最大界限
    2. rotate环境变量中-fs, -s, -r 数值超过最小界限
预期结果:
    1. 使用默认配置
    2. 使用默认配置
*/
TEST_F(LoggerTest, LogEnvStrRotateDefaultVal)
{
    uint32_t mByte = 1024 * 1024; // 1024表示转换为MB
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    {
        std::string rotateStr =
            "-fs 501 -s weekly -s yeARly -s 181 -r 65 ;   server : -s daily -s monthly -fs 55 -r 6 ;"
            " llm: -s 5 -fs 6 -r 7";
        setenv(ENV_ROTATE, rotateStr.c_str(), 1);
        std::cout << ENV_ROTATE << ":" << rotateStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_ROTATE);
        EXPECT_EQ(logger.mMaxLogFileSize, 20 * mByte); // 20: default log file size (MB)
        EXPECT_EQ(logger.mMaxLogFileNum, 10); // 10: default log file num per process
    }
    {
        std::string rotateStr =
            "-fs -1 -s weekly -s yeARly -s -1 -r -1 ;   server : -s daily -s monthly -fs 55 -r 6 ;"
            " llm: -s 5 -fs 6 -r 7";
        setenv(ENV_ROTATE, rotateStr.c_str(), 1);
        std::cout << ENV_ROTATE << ":" << rotateStr << std::endl;
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_ROTATE);
        EXPECT_EQ(logger.mMaxLogFileSize, 20 * mByte);  // 20: default log file size (MB)
        EXPECT_EQ(logger.mMaxLogFileNum, 10); // 10: default log file num per process
    }
}

/*
测试描述: 测试rotate环境变量和配置文件兼容性
测试步骤:
    1. rotate环境变量配置-fs, -r，配置文件设置 max_log_file_size, max_log_file_num
    2. rotate环境变量无轮转配置， 配置文件设置 max_log_file_size, max_log_file_num
    3. rotate环境变量无轮转配置， 无配置文件字段
预期结果:
    1. 使用环境变量配置
    2. 使用配置文件配置
    3. 使用默认配置值
*/
TEST_F(LoggerTest, LogEnvStrRotateEnvVarConfigCompatibility)
{
    uint32_t mByte = 1024 * 1024; // 1024表示转换为MB
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = false;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    {
        std::string rotateStr = "-fs 30 -r 20 ";
        setenv(ENV_ROTATE, rotateStr.c_str(), 1);
        std::cout << ENV_ROTATE << ":" << rotateStr << std::endl;
        nlohmann::json configObj = {
            {"log_info", {
                {"max_log_file_size", 40},
                {"max_log_file_num", 30},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        unsetenv(ENV_ROTATE);
        EXPECT_EQ(logger.mMaxLogFileSize, 30 * mByte); // 30: log file size (MB)
        EXPECT_EQ(logger.mMaxLogFileNum, 20); // 20: log file num per process
    }
    {
        unsetenv(ENV_ROTATE);
        nlohmann::json configObj = {
            {"log_info", {
                {"max_log_file_size", 40},
                {"max_log_file_num", 30},
            }}};
        logger.Init(defaultLogConfig, configObj, "");
        unsetenv(ENV_ROTATE);
        EXPECT_EQ(logger.mMaxLogFileSize, 40 * mByte); // 40: log file size (MB)
        EXPECT_EQ(logger.mMaxLogFileNum, 30); // 30: log file num per process
    }
    {
        unsetenv(ENV_ROTATE);
        logger.Init(defaultLogConfig, {}, "");
        unsetenv(ENV_ROTATE);
        EXPECT_EQ(logger.mMaxLogFileSize, 20 * mByte); // 20: default log file size (MB)
        EXPECT_EQ(logger.mMaxLogFileNum, 10); // 10: default log file num per process
    }
}

/*
测试描述: 测试Log模块错误码获取
测试步骤:
    1. 调取错误码接口
预期结果:
    1. 错误码包含预期值
*/
TEST_F(LoggerTest, LogErrorCode)
{
    std::string codePrefix = "MIE03"; // 03为ms的组件编号
    {
        std::string warnCode = "";
        warnCode = GetWarnCode(ErrorType::EXCEPTION, ControllerFeature::CONTROLLER);
        EXPECT_NE(warnCode.find(codePrefix+"W10"), std::string::npos);
        warnCode = GetWarnCode(ErrorType::EXCEPTION, CoordinatorFeature::COORDINATOR);
        EXPECT_NE(warnCode.find(codePrefix + "W20"), std::string::npos);
        warnCode = GetWarnCode(ErrorType::EXCEPTION, DeployerFeature(-1));
        EXPECT_TRUE(warnCode.empty());
        warnCode = GetWarnCode(ErrorType::EXCEPTION, CommonFeature::INVALID_ENUM);
        EXPECT_TRUE(warnCode.empty());
    }
    {
        std::string errorCode = "";
        errorCode = GetErrorCode(ErrorType::EXCEPTION, ControllerFeature::CONTROLLER);
        EXPECT_NE(errorCode.find(codePrefix + "E10"), std::string::npos);
        errorCode = GetErrorCode(ErrorType::EXCEPTION, CoordinatorFeature(-1));
        EXPECT_TRUE(errorCode.empty());
        errorCode = GetErrorCode(ErrorType::EXCEPTION, DeployerFeature::MSCTL);
        EXPECT_NE(errorCode.find(codePrefix + "E30"), std::string::npos);
        errorCode = GetErrorCode(ErrorType::EXCEPTION, CommonFeature::INVALID_ENUM);
        EXPECT_TRUE(errorCode.empty());
    }
    {
        std::string criticalCode = "";
        criticalCode = GetCriticalCode(ErrorType::EXCEPTION, ControllerFeature::INVALID_ENUM);
        EXPECT_TRUE(criticalCode.empty());
        criticalCode = GetCriticalCode(ErrorType::EXCEPTION, CoordinatorFeature::COORDINATOR);
        EXPECT_NE(criticalCode.find(codePrefix + "C20"), std::string::npos);
        criticalCode = GetCriticalCode(ErrorType::EXCEPTION, DeployerFeature(-1));
        EXPECT_TRUE(criticalCode.empty());
        criticalCode = GetCriticalCode(ErrorType::EXCEPTION, CommonFeature::HTTPSERVER);
        EXPECT_NE(criticalCode.find(codePrefix + "C40"), std::string::npos);
    }
}

/*
测试描述: 测试Log模块日志前缀
测试步骤:
    1. 开启可选日志选项
    2. 获取各级别日志前缀
    3. 对比结果
预期结果:
    1. 前缀包含预期值
*/
TEST_F(LoggerTest, LogPrefix)
{
    {
        logger.mIsLogVerbose = true;
        std::string logPrefix = "";
        logPrefix = logger.GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_CRITICAL);
        EXPECT_FALSE(logPrefix.empty());
        EXPECT_NE(logPrefix.find("CRITICAL"), std::string::npos);

        logPrefix = logger.GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_ERROR);
        EXPECT_FALSE(logPrefix.empty());
        EXPECT_NE(logPrefix.find("ERROR"), std::string::npos);

        logPrefix = logger.GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_WARN);
        EXPECT_FALSE(logPrefix.empty());
        EXPECT_NE(logPrefix.find("WARN"), std::string::npos);

        logPrefix = logger.GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_INFO);
        EXPECT_FALSE(logPrefix.empty());
        EXPECT_NE(logPrefix.find("INFO"), std::string::npos);

        logPrefix = logger.GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_PERF);
        EXPECT_FALSE(logPrefix.empty());
        EXPECT_NE(logPrefix.find("PERF"), std::string::npos);

        logPrefix = logger.GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_UNKNOWN);
        EXPECT_FALSE(logPrefix.empty());
        EXPECT_NE(logPrefix.find("[]"), std::string::npos);
        logger.mIsLogVerbose = false;
    }
}

static void RotateTest(     // 日志轮转的比较测试函数
    Logger &logger, uint32_t loopCount, std::string dirPath, std::string runLogName, std::string operationLogName)
{
    auto retRun = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "a message");
    auto retOperation = logger.Log(LogType::OPERATION, LogLevel::MINDIE_LOG_ERROR, "a message");
    EXPECT_NE(retRun, -1);
    EXPECT_NE(retOperation, -1);
    uint32_t runFileCount = 0;
    uint32_t operationFileCount = 0;

    DIR *dir;
    struct dirent *entry;
    if ((dir = opendir(dirPath.c_str())) == nullptr) {
        std::cerr << "Failed to open directory: " << std::strerror(errno) << std::endl;
        return;
    }
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;  // 忽略当前目录和父目录
        }

        std::string entryPath = dirPath + "/" + std::string(entry->d_name);
        struct stat statbuf;
        if (stat(entryPath.c_str(), &statbuf) == -1) {
            std::cerr << "Failed to stat entry: " << std::strerror(errno) << std::endl;
            closedir(dir);
            break;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            continue;  // 如果是目录
        } else if (entryPath.find(runLogName) != std::string::npos) {
            runFileCount++;
        } else if (entryPath.find(operationLogName) != std::string::npos) {
            operationFileCount++;
        }
    }
    closedir(dir);

    EXPECT_EQ(runFileCount, loopCount + 1);
    EXPECT_EQ(operationFileCount, loopCount + 1);
    return ;
}

/*
测试描述: 测试Log模块日志轮转写
测试步骤:
    1. 触发轮转写，调取获取日志轮转写的接口
预期结果:
    1. 轮转的日志文件名称、数量与预期相同：
    若当前进程的文件数量超过上限， 将删除旧文件。
*/
TEST_F(LoggerTest, LogRotateWrite)
{
    uint32_t mMaxLogFileNum = 5;

    std::string dirPath = GetParentPath(GetExecutablePath());
    std::string runLogName = "test_rotate_run_" + GetLocalTimeNow();  // 时间戳防重名
    std::string operationLogName = "test_rotate_operation_" + GetLocalTimeNow();

    Stub stub;
    stub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = true;
    defaultLogConfig.maxLogStrSize = 512; // 512: max string size of a log message
    defaultLogConfig.logLevel = "DEBUG";
    defaultLogConfig.runLogPath = dirPath + "/" + runLogName + ".log";
    defaultLogConfig.operationLogPath = dirPath + "/" + operationLogName + ".log";
    auto result = logger.Init(defaultLogConfig, {}, "");
    stub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
    EXPECT_EQ(result, 0);

    logger.mMaxLogFileNum = mMaxLogFileNum;  // 最大文件数
    for (uint32_t i = 1; i < mMaxLogFileNum; i++) {
        logger.mCurrentSize[LogType::RUN] = logger.mMaxLogFileSize;
        logger.mCurrentSize[LogType::OPERATION] = logger.mMaxLogFileSize;
        RotateTest(logger, i, dirPath, runLogName, operationLogName);
    }
    // test number of files per process
    for (uint32_t extra = 1; extra <= 3; extra++) { // 3: number of files exeeds max num of files allowed
        logger.mCurrentSize[LogType::RUN] = logger.mMaxLogFileSize;
        logger.mCurrentSize[LogType::OPERATION] = logger.mMaxLogFileSize;
        RotateTest(logger, mMaxLogFileNum - 1, dirPath, runLogName, operationLogName);
    }
}

/*
测试描述: 打桩测试Log模块文件初始化
测试步骤:
    1. 文件不存在，fopen/fclose失败
    2. 文件不存在，创建文件成功
    3. 文件存在，fopen失败
    4. 文件存在，fopen成功，(feek、fclose)失败/fseek成功、fclose失败
    5. 文件存在，fopen成功，chmod失败
预期结果:
    1. 初始化日志文件失败，初始化失败
    2. 初始化成功
    3. 初始化日志文件失败，初始化失败
    4. 初始化日志文件失败，初始化失败
    5. 初始化日志文件失败，初始化失败
*/
TEST_F(LoggerTest, LogInitLogFileStub)
{
    Stub hseStub;
    hseStub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
    std::string dirPath = GetParentPath(GetExecutablePath());
    defaultLogConfig.option = {true, true};
    defaultLogConfig.logLevel = "DEBUG";
    defaultLogConfig.runLogPath = dirPath + "/" + "test_init_run_" + GetLocalTimeNow() + ".log";  // 时间戳防重名
    defaultLogConfig.operationLogPath = dirPath + "/" + "test_init_operation_" + GetLocalTimeNow() + ".log";
    {
        Stub stub;
        stub.set(fopen, ReturnNullptrStub);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, -1);
        stub.reset(fopen);

        stub.set(fclose, ReturnNeOneStub);
        result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, -1);
        stub.reset(fclose);
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
    defaultLogConfig.runLogPath = dirPath + "/" + "test_init_run_" + GetLocalTimeNow() + ".log";  // 时间戳防重名
    defaultLogConfig.operationLogPath = dirPath + "/" + "test_init_operation_" + GetLocalTimeNow() + ".log";
    auto result = logger.Init(defaultLogConfig, {}, "");
    EXPECT_EQ(result, 0);
    {
        Stub stub;
        stub.set(fopen, ReturnNullptrStub);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, -1);
        stub.reset(fopen);
    }
    {
        Stub stub;
        stub.set(fclose, ReturnNeOneStub);
        result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, -1);
        stub.reset(fclose);
    }
    {
        Stub stub;
        stub.set(chmod, ReturnNeOneStub);
        auto result = logger.Init(defaultLogConfig, {}, "");
        EXPECT_EQ(result, -1);
        stub.reset(chmod);
    }
    hseStub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
}

/*
测试描述: 打桩测试Log模块日志写失败场景
测试步骤:
    1. 日志长度超过最大长度
    2. 日志文件未准备完成
    3. remove文件失败
    4. rename文件失败
    5. chmod文件失败
预期结果:
    1. 日志写入失败，未开启日志功能时返回成功
    2. 日志不执行失败，进行打屏
    3. 日志轮转失败
    4. 日志轮转失败
    5. 日志轮转失败
*/
TEST_F(LoggerTest, LogLogStub)
{
    Stub hseStub;
    Stub stub;
    hseStub.set(ADDR(ock::hse::HseCryptor, SetExternalLogger), ReturnZeroStub);
    uint32_t maxLogStrSize = 512;   // 单条日志最大长度512
    std::string dirPath = GetParentPath(GetExecutablePath());
    defaultLogConfig.option.toFile = true;
    defaultLogConfig.option.toStdout = true;
    defaultLogConfig.maxLogStrSize = maxLogStrSize;
    defaultLogConfig.logLevel = "DEBUG";
    defaultLogConfig.runLogPath = dirPath + "/" + "test_log_run_" + GetLocalTimeNow() + ".log";  // 时间戳防重名;
    defaultLogConfig.operationLogPath = dirPath + "/" + "test_log_operation_" + GetLocalTimeNow() + ".log";
    auto result = logger.Init(defaultLogConfig, {}, "");
    EXPECT_NE(result, -1);

    //  日志长度超过最大长度
    std::string logPrefix = logger.GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_ERROR);
    logger.mMaxLogStrSize = logPrefix.size() + 1;
    auto ret1 = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "a message");
    EXPECT_EQ(ret1, 0);
    std::string overflowMessage(LOG_MSG_STR_SIZE_MAX, 'A');
    ret1 = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, overflowMessage.c_str());
    EXPECT_EQ(ret1, -1);
    logger.mIsLogEnable = false;
    ret1 = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "a message");
    EXPECT_NE(ret1, -1);
    logger.mMaxLogStrSize = maxLogStrSize;
    logger.mIsLogEnable = true;

    // 日志文件未准备完成
    logger.mIsLogFileReady = false;
    auto ret2 = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "a message");
    EXPECT_NE(ret2, -1);
    logger.mIsLogFileReady = true;

    // remove文件失败
    stub.set(remove, ReturnNeOneStub);
    for (uint32_t i = 0; i < logger.mMaxLogFileNum - 1; i++) {
        logger.mCurrentSize[LogType::RUN] = logger.mMaxLogFileSize;
        auto ret3 = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "a message");
        EXPECT_NE(ret3, -1);
    }
    logger.mCurrentSize[LogType::RUN] = logger.mMaxLogFileSize;
    auto ret3 = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "a message");
    EXPECT_EQ(ret3, -1);
    stub.reset(remove);

    // rename文件失败
    logger.mCurrentSize[LogType::RUN] = logger.mMaxLogFileSize;
    stub.set(rename, ReturnNeOneStub);
    auto ret4 = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "a message");
    EXPECT_EQ(ret4, -1);
    stub.reset(rename);

    // chmod文件失败
    logger.mCurrentSize[LogType::RUN] = logger.mMaxLogFileSize;
    stub.set(chmod, ReturnNeOneStub);
    auto ret5 = logger.Log(LogType::RUN, LogLevel::MINDIE_LOG_ERROR, "a message");
    EXPECT_EQ(ret5, -1);
    stub.reset(chmod);

    hseStub.reset(ADDR(ock::hse::HseCryptor, SetExternalLogger));
}

static std::time_t GetTimeStampAfterDay()
{
    std::cout << "Set GetTimeStampDayStub" << std::endl;
    uint64_t oneDaySeconds = 86400;  // 一天86400秒
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + oneDaySeconds;
}

static std::time_t GetTimeStampAfterWeek()
{
    std::cout << "Set GetTimeStampWeekStub" << std::endl;
    uint64_t oneDaySeconds = 86400;  // 一天86400秒
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + oneDaySeconds * 7;   // 一周7天
}

static std::time_t GetTimeStampAfterMonth()
{
    std::cout << "Set GetTimeStampMonthStub" << std::endl;
    uint64_t oneDaySeconds = 86400;  // 一天86400秒
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + oneDaySeconds * 31;   // 一月最多31天
}

static std::time_t GetTimeStampAfterYear()
{
    std::cout << "Set GetTimeStampYearStub" << std::endl;
    uint64_t oneDaySeconds = 86400;  // 一天86400秒
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) + oneDaySeconds * 366;   // 一年最多366天
}