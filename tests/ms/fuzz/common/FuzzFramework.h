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
#ifndef MINDIE_MS_FUZZ_FRAMEWORK_H
#define MINDIE_MS_FUZZ_FRAMEWORK_H

#include <iostream>
#include <securec.h>
#include "secodeFuzz.h"
#include "gtest/gtest.h"

constexpr uint32_t THIRTY_MILLION_FUZZ_COUNT = 3000 * 10000;           // 3000w 次 fuzzcount
static uint32_t GetDefaultFuzzCount()
{
    return THIRTY_MILLION_FUZZ_COUNT;
}

static const char* GetDefaultFuzzTestCaseName() {
    static char testcaseName[256] = {0};
    auto testinfo = testing::UnitTest::GetInstance()->current_test_info();
    return testinfo->name();
}

class TestTimer {
public:
    static TestTimer &GetInstance()
    {
        static TestTimer instance;
        return instance;
    }

    void Start()
    {
        time(&start);
    }

    bool IfEnd()
    {
        if (m_seconds == 0) {
            return false;
        }
        time_t now;
        time(&now);
        if (now - start >= m_seconds) {
            return true;
        }
        return false;
    }

    // 单位秒，为0则不生效
    void SetTimer(uint32_t seconds)
    {
        m_seconds = seconds;
    }

    // 单位秒，为0则不生效
    uint32_t GetTimer()
    {
        return m_seconds;
    }

    ~TestTimer() = default;

private:
    TestTimer() = default;
    uint32_t m_seconds = 0;
    time_t start = 0;
};

class ExternalFuzzCount {
public:
    static ExternalFuzzCount &GetInstance()
    {
        static ExternalFuzzCount instance;
        return instance;
    }
    void SetCount(uint32_t count)
    {
        o_count = count;
    }
    uint32_t GetCount()
    {
        return o_count;
    }

private:
    ExternalFuzzCount() = default;
    ~ExternalFuzzCount() = default;
    ExternalFuzzCount(const ExternalFuzzCount &) = delete;
    ExternalFuzzCount &operator = (const ExternalFuzzCount &) = delete;
    uint32_t o_count = 0;
};

int ParseCmd(int argc, char *argv[]);
int GetFuzzCount();
int GetFuzzTime();
int GetIsReproduce();
char* GetTestCaseName();
char* GetCrashCorpusName();
char* GetCorpusPath();

#define DT_FUZZ_START1(testCaseName)  \
    char* testcaseName = testCaseName; \
    if (GetCrashCorpusName()) \
        testcaseName = GetCrashCorpusName(); \
    GetTestCaseName()[0] = 0; \
    sprintf_s(GetTestCaseName(), 512, "%s%s", GetCorpusPath(), testcaseName); \
    DT_FUZZ_START(0, GetFuzzCount(), GetTestCaseName(), GetIsReproduce())
#endif