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
#ifndef TIME_H
#define TIME_H

#if defined(_MSC_VER)
#include <chrono>
#else
#include <time.h>
#endif
#include <iostream>

const uint32_t SFE_SEC_TO_MS = 1000;   /* 每秒等于1000毫秒 */
const uint32_t  SFE_MS_TO_NS = 1000000; /* 每毫秒等于1000000纳秒 */

inline double SFE_GetShareTickCoarse(void)
{
#if defined(_MSC_VER)
    return (double)std::chrono::high_resolution_clock::now().time_since_epoch().count();
#else
    timespec stTime { };
    double d64Ms;

    if (clock_gettime(CLOCK_MONOTONIC, &stTime) == 0) {
        d64Ms = (double)stTime.tv_sec * SFE_SEC_TO_MS + (double)stTime.tv_nsec / SFE_MS_TO_NS;
        return d64Ms;
    }

    return 0;
#endif
}

class Timer {
public:
    void start()
    {
        t_st = SFE_GetShareTickCoarse();
    };

    void stop()
    {
        t_end = SFE_GetShareTickCoarse();
    };

    void print()
    {
        std::cout << "run time : " << (t_end - t_st) << "ms"<<std::endl;
    };

    double value()
    {
        return t_end - t_st;
    }
    
private:
    timespec stTime;
    double t_st, t_end;
};

static Timer g_timer;

static void TimerStart()
{
    g_timer.start();
}

static void TimerStop()
{
    g_timer.stop();
}

static void TimerPrint()
{
    g_timer.print();
}

static double TimerRecord()
{
    return g_timer.value();
}

#endif