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
#include <gtest/gtest.h>
#include "LogLevelDynamicHandler.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <nlohmann/json.hpp>

using namespace MINDIE::MS;
using json = nlohmann::json;

 // 模拟环境变量
void SetEnv(const std::string& name, const std::string& value)
{
    setenv(name.c_str(), value.c_str(), 1);
}

 // 测试用例类
class LogLevelDynamicHandlerTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 设置环境变量
        SetEnv("MIES_INSTALL_PATH", ".");
        SetEnv("MINDIE_LOG_LEVEL", "DEBUG");

        // 创建测试配置文件
        std::string dirName = "./conf";
        mkdir(dirName.c_str(), 0750);
        json config;
        config["LogConfig"] = {
            {"dynamicLogLevel", ""},
            {"dynamicLogLevelValidHours", 2},
            {"dynamicLogLevelValidTime", ""}
        };
        std::string configPath = "./conf/config.json";
        std::ofstream out(configPath);
        int jsonIndentation = 4;
        out << config.dump(jsonIndentation);
        out.close();
        chmod(configPath.c_str(), 0640);

        handler = &LogLevelDynamicHandler::GetInstance();
        handler->Stop(); // 确保线程停止
    }

    void TearDown() override
    {
        // 清理测试文件
        std::string configPath = "./conf/config.json";
        std::remove(configPath.c_str());
        unsetenv("MIES_INSTALL_PATH");
        unsetenv("MINDIE_LOG_LEVEL");
    }

    LogLevelDynamicHandler* handler;
};

 // 测试初始化方法
TEST_F(LogLevelDynamicHandlerTest, InitTest)
{
    handler->Init(1000, "default", true);
    EXPECT_TRUE(handler->isRunning_);
    handler->Stop();
    EXPECT_FALSE(handler->isRunning_);
}

 // 测试配置文件读取
TEST_F(LogLevelDynamicHandlerTest, ReadLogConfigTest)
{
    handler->Init(1000, "default", true);
    json logConfig;
    bool result = handler->CheckLogConfig("./conf/config.json", logConfig);
    EXPECT_TRUE(result);
    EXPECT_TRUE(logConfig.contains("dynamicLogLevel"));
    handler->Stop();
}

 // 测试无效参数处理
TEST_F(LogLevelDynamicHandlerTest, CheckAndFlushInvalidParamTest)
{
    handler->Init(1000, "default", true);
    json logConfig;
    logConfig["dynamicLogLevel"] = 123; // 非字符串
    logConfig["dynamicLogLevelValidHours"] = "abc"; // 非整数
    logConfig["dynamicLogLevelValidTime"] = "invalid-time"; // 无效时间格式
    bool result = handler->CheckAndFlushInvalidParam(logConfig);
    EXPECT_FALSE(result);
    handler->Stop();
}

 // 测试日志级别更新
TEST_F(LogLevelDynamicHandlerTest, UpdateDynamicLogParamTest)
{
    handler->Init(1000, "default", true);
    handler->GetAndSetLogConfig();
    EXPECT_EQ(handler->currentLevel_, "");
    EXPECT_EQ(handler->currentValidHours_, 2);
    handler->Stop();
}

 // 测试配置文件写入
TEST_F(LogLevelDynamicHandlerTest, SetConfigFileTest)
{
    handler->Init(1000, "default", true);
    handler->SetConfigFile("dynamicLogLevel", "INFO", false);
    handler->SetConfigFile("dynamicLogLevelValidHours", "3", true);
    handler->SetConfigFile("dynamicLogLevelValidTime", "2025-04-05 12:00:00", false);

    json logConfig;
    bool result = handler->CheckLogConfig("./conf/config.json", logConfig);
    EXPECT_TRUE(result);
    EXPECT_EQ(logConfig["dynamicLogLevel"], "INFO");
    EXPECT_EQ(logConfig["dynamicLogLevelValidHours"], 3);
    EXPECT_EQ(logConfig["dynamicLogLevelValidTime"], "2025-04-05 12:00:00");
    handler->Stop();
}

 // 测试插入默认配置
TEST_F(LogLevelDynamicHandlerTest, InsertLogConfigToFileTest)
{
    std::string configPath = "./conf/config.json";
    std::remove(configPath.c_str());
    std::ofstream file(configPath);
    if (file.is_open()) {
        file << R"({})";
        file.close();
    }
    chmod(configPath.c_str(), 0640);
    handler->Init(1000, "default", true);
    handler->InsertLogConfigToFile();
    json logConfig;
    bool result = handler->CheckLogConfig(configPath, logConfig);
    EXPECT_TRUE(result);
    EXPECT_TRUE(logConfig.contains("dynamicLogLevel"));
    EXPECT_TRUE(logConfig.contains("dynamicLogLevelValidHours"));
    EXPECT_TRUE(logConfig.contains("dynamicLogLevelValidTime"));
}

 // 测试时间有效性判断
TEST_F(LogLevelDynamicHandlerTest, IsCurrentTimeWithinValidRangeTest)
{
    std::time_t now = std::time(nullptr);
    std::tm tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    std::string validTime = oss.str();

    bool result = handler->IsCurrentTimeWithinValidRange(validTime, 1);
    EXPECT_TRUE(result);
}

 // 测试清除配置
TEST_F(LogLevelDynamicHandlerTest, ClearLogConfigParamTest)
{
    handler->Init(1000, "default", true);
    handler->ClearLogConfigParam();

    json logConfig;
    bool result = handler->CheckLogConfig("./conf/config.json", logConfig);
    EXPECT_TRUE(result);
    EXPECT_TRUE(logConfig["dynamicLogLevel"].is_string());
    EXPECT_EQ(logConfig["dynamicLogLevel"].get<std::string>(), "");
    EXPECT_TRUE(logConfig["dynamicLogLevelValidTime"].is_string());
    EXPECT_EQ(logConfig["dynamicLogLevelValidTime"].get<std::string>(), "");
    handler->Stop();
}