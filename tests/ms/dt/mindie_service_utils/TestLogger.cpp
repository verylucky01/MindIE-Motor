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
#include <sstream>
#include <string>
#define private public
#include "Logger.h"

using namespace MINDIE::MS;
using json = nlohmann::json;

class LoggerTest : public ::testing::Test {
protected:

    std::stringstream buffer;

    std::streambuf* originOut;

    void SetUp() override
    {

        buffer.str("");
        buffer.clear();

        originOut = std::cout.rdbuf(buffer.rdbuf());

        Logger *logger = Logger::Singleton();

        logger->SetLogLevel(LogLevel::MINDIE_LOG_INFO);
        
        // 测试时关闭文件输出, 仅测试标准输出
        logger->mToFile = false;
        logger->mLogPath.clear();
        logger->mOutStream.clear();
        logger->mCurrentSize.clear();
        logger->mIsLogFileReady = false;

        logger->mToStdout = true;
        logger->mIsLogVerbose = true;
        logger->mMaxLogStrSize = 8192;

        logger->mIsLogEnable = true;
    }

    void TearDown() override
    {
        std::cout.rdbuf(originOut);
    }

    void VerifySplit(const std::string& targetStr, const std::vector<std::string>& lines, size_t maxLineLen)
    {
        auto TruncateLogPrefix = [](std::string& line) -> void {
            size_t pos = line.find("[INFO]");
            if (pos != std::string::npos) {
                line = line.substr(pos);
            }
        };

        // 验证每段内容
        size_t totalLen = targetStr.size();
        size_t processed = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            size_t remaining = totalLen - processed;
            size_t expectedLen = remaining > maxLineLen ? maxLineLen : remaining;
            std::string expectedLine = targetStr.substr(processed, expectedLen);
            std::string actualLine = lines[i];
            if (i == 0) {
                TruncateLogPrefix(expectedLine);
                TruncateLogPrefix(actualLine);
            }
            if (actualLine != "" && expectedLine == "") {
                size_t bufferSize = maxLineLen * MAX_SPLIT_NUM;
                bufferSize = bufferSize > LOG_MSG_STR_SIZE_MAX ? LOG_MSG_STR_SIZE_MAX : bufferSize;
                std::ostringstream lastLineOss;
                lastLineOss << "Logger Warning: Exception accurs when put input string to buffer. "
                               "Maybe the length of the input string exceeds the maximum allowed log print length "
                            << bufferSize;
                expectedLine = lastLineOss.str();
            }
            EXPECT_EQ(actualLine, expectedLine);
            processed += expectedLen;
        }
    }
};

TEST_F(LoggerTest, LogOverflowInputSplitTest)
{
    size_t maxLineLen = 8192;

    Logger *logger = Logger::Singleton();
    logger->mMaxLogStrSize = maxLineLen;
    logger->mIsLogVerbose = false;
    logger->SetLogLevel(LogLevel::MINDIE_LOG_INFO);

    std::string logPrefix = logger->GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_INFO);

    // input string is overflow
    std::string longMsg(LOG_MSG_STR_SIZE_MAX, 'A');
    std::string targetStr = FilterLogStr(logPrefix + ": " + longMsg);
    targetStr = targetStr.substr(0, LOG_MSG_STR_SIZE_MAX - 1);
    LOG_I("%s", longMsg.c_str());

    std::string outputStr = buffer.str();

    std::istringstream iss(outputStr);
    std::string line;
    std::vector<std::string> lines;

    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    VerifySplit(targetStr, lines, maxLineLen);
}

TEST_F(LoggerTest, LogLongInputSplitTest)
{
    size_t maxLineLen = 8192;

    Logger *logger = Logger::Singleton();
    logger->mMaxLogStrSize = maxLineLen;
    logger->mIsLogVerbose = false;
    logger->SetLogLevel(LogLevel::MINDIE_LOG_INFO);

    std::string logPrefix = logger->GetLogPrefix(LogType::RUN, LogLevel::MINDIE_LOG_INFO);

    // long input string
    std::string longMsg(30000, 'A');
    std::string targetStr = FilterLogStr(logPrefix + ": " + longMsg);
    LOG_I("%s", longMsg.c_str());
    std::string outputStr = buffer.str();

    std::istringstream iss(outputStr);
    std::string line;
    std::vector<std::string> lines;

    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    VerifySplit(targetStr, lines, maxLineLen);
}