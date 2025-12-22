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
#include "FuzzFramework.h"
#include <gtest/gtest.h>
#include "secodeFuzz.h"

int g_fuzzCount = 30000000;                  // 运行次数,默认三千万次
int g_fuzzTime = 3600 * 3;                   // 运行时间，默认三个小时
int g_isLeakCheck = 1;                   // 是否检测内存泄漏
int g_isreproduce = 0;                   // 是否是复现模式
char* g_reportPath = "./secodefuzz_report"; // 运行模式，all,random,one
char* g_corpusPath = "/seed/";                  // 运行的命令名字,one模式有效
char* g_crashCorpusName = nullptr;          // crash样本名字，在复现模式有效
char  g_testCaseName[512] = {0};

int GetFuzzCount()
{
    return g_fuzzCount;
}

int GetFuzzTime()
{
    return g_fuzzTime;
}

int GetIsReproduce()
{
    return g_isreproduce;
}

char* GetReportPath()
{
    return g_reportPath;
}

char* GetCorpusPath()
{
    return g_corpusPath;
}

char* GetCrashCorpusName()
{
    return g_crashCorpusName;
}

char* GetTestCaseName()
{
    return g_testCaseName;
}

// 测试用例循环执行函数
void TestLoop(void)
{
    char *corpus = nullptr;
    printf("---------------------------------------------------------------------\r\n");
    printf("Count           is %d\r\n", g_fuzzCount);
    printf("Time            is %d\r\n", g_fuzzTime);
    printf("IsLeakCheck     is %d\r\n", g_isLeakCheck);
    printf("Isreproduce     is %d\r\n", g_isreproduce);
    printf("reportPath    is %s\r\n", g_reportPath);
    printf("corpusPath         is %s\r\n", g_corpusPath);
    printf("testcaseName is %s\r\n", g_crashCorpusName);
    printf("---------------------------------------------------------------------\r\n");

    // 设置报告路径
    DT_Set_Report_Path(g_reportPath);

    // 设置使能fork模式，每个测试用例单独在子进程运行
    DT_SetEnableFork(1);

    // 检测大内存使用，超过2048M使用或者1024M分配则当做bug报错
    DT_SetCheckOutOfMemory(1024, 2048);

    // 是能内存泄漏单次执行检测，默认也开启
    DT_Enable_Leak_Check(g_isLeakCheck, 0);

    // 设置用例单次执行多久超时，4.9版本后已经默认开启，默认600s
    DT_Set_TimeOut_Second(60);

    // 设置运行时间
    DT_Set_Running_Time_Second(g_fuzzTime);
}

int ParseCmd(int argc, char *argv[])
{
    int opt;

   // 得到运行参数
    while ((opt = getopt(argc, argv, "C:T:R:L:r:c:t:h")) != EOF) {
        switch (opt) {
            case 'C': printf(" count is:%s\n", optarg);
                g_fuzzCount = atol(optarg);
                break;
            case 'T': printf(" time is:%s\n", optarg);
                g_fuzzTime = atol(optarg);
                break;
            case 'R': printf(" reproduce is:%s\n", optarg);
                g_isreproduce = atol(optarg);
                break;
            case 'L': printf(" leak check is:%s\n", optarg);
                g_isLeakCheck = atol(optarg);
                break;
            case 'r': printf("\033[40;31m * reportPath is:%s* \033[0m \n", optarg);
                g_reportPath = (char*)malloc(strlen(optarg) + 1);
                memcpy_s(g_reportPath, (strlen(optarg) + 1), optarg, (strlen(optarg) + 1));
                break;
            case 'c': printf("\033[40;31m * corpusPath is:%s* \033[0m \n", optarg);
                g_corpusPath = (char*)malloc(strlen(optarg) + 1);
                memcpy_s(g_corpusPath, (strlen(optarg) + 1), optarg, (strlen(optarg) + 1));
                break;
            case 't': printf("\033[40;31m * testcaseName is:%s* \033[0m \n", optarg);
                g_crashCorpusName = (char*)malloc(strlen(optarg) + 1);
                memcpy_s(g_crashCorpusName, (strlen(optarg) + 1), optarg, (strlen(optarg) + 1));
                break;
            case 'h':
                printf("-------------------------\r\n");
                printf("-C  run count\r\n");
                printf("-T  run time (seconds)");
                printf("-R  Is reproduce ?(1,0)\r\n");
                printf("-r  reportPath is \r\n");
                printf("-c  corpusPath is\r\n");
                printf("-t  run testcaseName\r\n");
                printf("-h  help\r\n");
                printf("-------------------------\r\n");
                return 0;
            default:
                printf("other option is wrong\n");
                break;
        }
    }

    // 运行测试例
    TestLoop();

    printf("test main is exit\n");
    return 0;
}