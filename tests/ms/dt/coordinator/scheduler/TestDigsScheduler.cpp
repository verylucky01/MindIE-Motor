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
#include <stdlib.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <random>
#include <utility>
#include <unistd.h>
#include <pthread.h>
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#define private public
#define protected public
#include "ThreadPool.h"
#include "coordinator/digs_scheduler/digs_scheduler_impl.h"
#include "Helper.h"
#include "BaseScheduler.h"
#include "SchedulerFactory.h"
#include "Logger.h"

using namespace MINDIE::MS;

class TestDigsScheduler : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }

    void SetUp()
    {
        MINDIE::MS::DefaultLogConfig defaultLogConfig;
        defaultLogConfig.option.toFile = false;
        defaultLogConfig.option.toStdout = true;
        nlohmann::json logConfigFormFile = {};
        Logger::Singleton()->Init(defaultLogConfig, logConfigFormFile, "");
        auto controllerJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
    }
};

namespace {
size_t g_reqCount = 0;
size_t g_instanceNum = 5;
size_t g_initInputLength = 3000;
size_t g_initOutputLength = 200;
size_t g_periodInputLength = 1024;
size_t g_periodOutputLength = 1024;

void RegisterInstance(std::unique_ptr<MINDIE::MS::DIGSScheduler>& scheduler)
{
    const size_t prefillNum = 3;
    const size_t decodeNum = 2;

    const uint64_t groupId1 = 1;
    const uint64_t groupId2 = 2;

    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> list;

    // 共prefillNum + decodeNum个static块
    MINDIE::MS::DIGSInstanceStaticInfo node;
    for (size_t i = 0; i < prefillNum; ++i) {
        node.id = i;
        node.groupId = groupId1;
        node.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
        node.totalSlotsNum = 100000;
        node.totalBlockNum = 100000;
        list.push_back(node);
    }
    for (size_t i = 0; i < decodeNum; ++i) {
        node.groupId = groupId1;
        node.id = i + prefillNum;
        node.label = MINDIE::MS::DIGSInstanceLabel::DECODE_STATIC;
        node.totalSlotsNum = 100000;
        node.totalBlockNum = 100000;
        list.push_back(node);
    }

    MINDIE::MS::DIGSInstanceStaticInfo node1;
    node1.id = prefillNum + decodeNum; // PREFILL_PREFER块
    node1.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER;
    node1.groupId = groupId2;

    MINDIE::MS::DIGSInstanceStaticInfo node2;
    node2.id = prefillNum + decodeNum + 1;
    node2.label = MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER;
    node2.groupId = groupId2;

    list.push_back(node1);

    list.push_back(node2);

    scheduler->RegisterInstance(list);
}

void RegisterLimitedInstance(std::unique_ptr<MINDIE::MS::DIGSScheduler>& scheduler)
{
    const size_t prefillNum = 3;
    const size_t decodeNum = 2;

    const uint64_t groupId1 = 1;
    const uint64_t groupId2 = 2;

    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> list;

    // 共prefillNum + decodeNum个static块
    MINDIE::MS::DIGSInstanceStaticInfo node;
    for (size_t i = 0; i < prefillNum; ++i) {
        node.id = i;
        node.groupId = groupId1;
        node.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
        node.totalSlotsNum = 10;
        node.totalBlockNum = 10;
        list.push_back(node);
    }
    for (size_t i = 0; i < decodeNum; ++i) {
        node.groupId = groupId1;
        node.id = i + prefillNum;
        node.label = MINDIE::MS::DIGSInstanceLabel::DECODE_STATIC;
        node.totalSlotsNum = 10;
        node.totalBlockNum = 10;
        list.push_back(node);
    }

    MINDIE::MS::DIGSInstanceStaticInfo node1;
    node1.id = prefillNum + decodeNum; // PREFILL_PREFER块
    node1.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_PREFER;
    node1.groupId = groupId2;
    node1.totalSlotsNum = 10;
    node1.totalBlockNum = 10;

    MINDIE::MS::DIGSInstanceStaticInfo node2;
    node2.id = prefillNum + decodeNum + 1;
    node2.label = MINDIE::MS::DIGSInstanceLabel::DECODE_PREFER;
    node2.groupId = groupId2;
    node2.totalSlotsNum = 10;
    node2.totalBlockNum = 10;

    list.push_back(node1);

    list.push_back(node2);

    scheduler->RegisterInstance(list);
}

void SetConnect(std::unique_ptr<MINDIE::MS::DIGSScheduler>& scheduler)
{
    const uint64_t from1 = 3;
    const std::vector<uint64_t> to1{0};
    const uint64_t from2 = 6;
    const std::vector<uint64_t> to2{5};
    const uint64_t from3 = 4;
    const std::vector<uint64_t> to3{1, 2};
    // 设置连通
    MINDIE::MS::DIGSInstanceDynamicInfo info;
    info.id = from1;
    info.peers = to1;
    std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> instances;
    instances.push_back(info);

    info.id = from2;
    info.peers = to2;
    instances.push_back(info);

    info.id = from3;
    info.peers = to3;
    instances.push_back(info);

    scheduler->UpdateInstance(instances);
}

void GenerateText(std::vector<std::string> &texts, size_t num)
{
    std::mt19937 generator;
    std::uniform_int_distribution<int> distribution(5, 20);
    for (size_t idx = 0; idx < num; ++idx) {
        int length = distribution(generator);
        std::string word;
        std::string text;
        int wordlen;
        for (int i = 0; i < length; ++i) {
            wordlen = distribution(generator);
            word.assign(wordlen, 'a');
            text += (word + ' ');
        }
        texts.push_back(text);
    }
}

void AddReq(std::unique_ptr<MINDIE::MS::DIGSScheduler>& scheduler, std::vector<std::string>& texts, size_t num)
{
    GenerateText(texts, num);
    for (size_t i = g_reqCount; i < num; i++) {
        std::string reqId = "test" + std::to_string(i);
        scheduler->ProcReq(reqId, texts[i], MINDIE::MS::ReqType::UNKNOWN);
        g_reqCount++;
    }
}

void AddReqAddInstance(
    std::unique_ptr<MINDIE::MS::DIGSScheduler>& scheduler,
    std::vector<std::string>& texts,
    size_t num)
{
    GenerateText(texts, num);
    for (size_t i = g_reqCount; i < num / 2; i++) {
        std::string reqId = "test" + std::to_string(i);
        scheduler->ProcReq(reqId, texts[i], MINDIE::MS::ReqType::UNKNOWN);
        g_reqCount++;
    }

    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> list;
    MINDIE::MS::DIGSInstanceStaticInfo node;
    node.id = 7;
    node.groupId = 1;
    node.label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
    node.totalSlotsNum = 100000;
    node.totalBlockNum = 100000;
    list.push_back(node);
    
    node.groupId = 1;
    node.id = 8;
    node.label = MINDIE::MS::DIGSInstanceLabel::DECODE_STATIC;
    node.totalSlotsNum = 100000;
    node.totalBlockNum = 100000;
    list.push_back(node);

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    scheduler->RegisterInstance(list);
   
    std::vector<uint64_t> from = {7}; 
    MINDIE::MS::DIGSInstanceDynamicInfo info;
    info.id = 8;
    info.peers = from;
    std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> d_instances;
    d_instances.push_back(info);

    scheduler->UpdateInstance(d_instances);

    for (size_t i = num / 2; i < num; i++) {
        std::string reqId = "test" + std::to_string(i);
        scheduler->ProcReq(reqId, texts[i], MINDIE::MS::ReqType::UNKNOWN);
        g_reqCount++;
    }
}

void AddReqCloseInstance(
    std::unique_ptr<MINDIE::MS::DIGSScheduler>& scheduler,
    std::vector<std::string>& texts,
    size_t num)
{
    GenerateText(texts, num);
    for (size_t i = g_reqCount; i < num / 2; i++) {
        std::string reqId = "test" + std::to_string(i);
        scheduler->ProcReq(reqId, texts[i], MINDIE::MS::ReqType::UNKNOWN);
        g_reqCount++;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    scheduler->CloseInstance({0});

    for (size_t i = num / 2; i < num; i++) {
        std::string reqId = "test" + std::to_string(i);
        scheduler->ProcReq(reqId, texts[i], MINDIE::MS::ReqType::UNKNOWN);
        g_reqCount++;
    }
}

void InitConfig(MINDIE::MS::DIGSScheduler::Config& config)
{
    config["max_schedule_count"] = "20000";
    config["reordering_type"] = "1";
    config["select_type"] = "2";
    config["alloc_file"] = "";
    config["pool_type"] = "1";
    config["block_size"] = "128";
    config["load_score_limit"] = "1000, 100";
    // 根据常见负载的SLO，设置prefill limit为1000ms，decode为50ms
    config["affinity_score_limit"] = "1000, 50";

    config["export_type"] = "0";
    config["pull_request_timeout"] = "500";

    config["max_res_num"] = "5000";
    config["res_view_update_timeout"] = "500";
    config["res_limit_rate"] = "1.1";
    config["metaResource_names"] = "";
    config["load_cost_values"] = "1, 1";
    config["load_cost_coefficient"] = "0, 1, 1024, 24, 6, 0, 0, 1, 1";

    config["model_weight_path"] = "/data/atb_testdata/weights/llama1-7b";
    config["tokenizer_number"] = "4";
}
}



/*
测试描述: digs调度器使用pd分离负载均衡算法高并发调度
测试步骤:
    1. 配置DigsSchedulerImpl，使用负载均衡算法
    2. 向DefaultSchedulerImpl发送高并发请求，预期结果1
预期结果:
    1. 程序正常运行，请求被正确调度
*/
TEST_F(TestDigsScheduler, TestLoadBalanceTC01)
{
    const size_t sleepTime = 100;
    const size_t reqHandleTime = 1000;
    const size_t sleepCount = 3;
    std::cout << "DIGS: main start!" << std::endl;

    MINDIE::MS::DIGSScheduler::Config config;
    InitConfig(config);

    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler =
        MINDIE::MS::SchedulerFactory::GetInstance().CreateScheduler("digs_scheduler", config);

    if (scheduler == nullptr) {
        std::cout << "DIGS: digs scheduler not find!" << std::endl;
        exit(0);
    }

    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    scheduler->RegisterPDNotifyAllocation([&reqNode](const std::string& reqId, uint64_t prefill,
        uint64_t decode) -> int32_t {
        std::cout << "DIGS: get allocation req: " << reqId << " prefill: " << prefill << " decode: " << decode <<
            std::endl;
    reqNode[reqId] = std::make_pair(prefill, decode);
        return 0;
    });

    RegisterInstance(scheduler);

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));

    // 关闭服务测试，需要关闭的实例id写入del变量即可
    std::vector<uint64_t> del{};
    scheduler->CloseInstance(del);

    // 手动增加连通情况
    SetConnect(scheduler);

    const size_t reqNum1 = 1000;
    const size_t reqNum2 = 3;
    std::vector<std::string> texts;
    AddReq(scheduler, texts, reqNum1);
    scheduler->ProcReq("test0", " ", MINDIE::MS::ReqType::UNKNOWN);

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * sleepCount));

    scheduler->UpdateReq("test0", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    scheduler->UpdateReq("test1", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    scheduler->UpdateReq("test3", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(reqHandleTime * sleepCount));

    std::vector<int> totalLength(7, 0);
    for (auto iter = reqNode.begin(); iter != reqNode.end(); ++iter) {
        int length = texts[std::stoi(iter->first.substr(4))].size();
        totalLength[(iter->second).first] += length;
        totalLength[(iter->second).second] += length;
    }
    for (int i = 0; i < 7; ++i) {
        std::cout << totalLength[i] << " ";
    }
    std::cout << std::endl;
}

/*
测试描述: digs调度器使用pd分离负载均衡算法高并发调度,过程中增加节点
测试步骤:
    1. 配置DigsSchedulerImpl，使用负载均衡算法
    2. 向DefaultSchedulerImpl发送高并发请求，过程中增加节点，预期结果1
预期结果:
    1. 程序正常运行，请求被正确调度，后续加入的节点会被考虑使用
*/
TEST_F(TestDigsScheduler, TestLoadBalanceTC02)
{
    const size_t sleepTime = 100;
    const size_t reqHandleTime = 1000;
    const size_t sleepCount = 3;
    std::cout << "DIGS: main start!" << std::endl;

    MINDIE::MS::DIGSScheduler::Config config;
    InitConfig(config);

    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler =
        MINDIE::MS::SchedulerFactory::GetInstance().CreateScheduler("digs_scheduler", config);

    if (scheduler == nullptr) {
        std::cout << "DIGS: digs scheduler not find!" << std::endl;
        exit(0);
    }

    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    scheduler->RegisterPDNotifyAllocation([&reqNode](const std::string& reqId, uint64_t prefill,
        uint64_t decode) -> int32_t {
        std::cout << "DIGS: get allocation req: " << reqId << " prefill: " << prefill << " decode: " << decode <<
            std::endl;
    reqNode[reqId] = std::make_pair(prefill, decode);
        return 0;
    });

    RegisterInstance(scheduler);

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));

    // 关闭服务测试，需要关闭的实例id写入del变量即可
    std::vector<uint64_t> del{};
    scheduler->CloseInstance(del);

    // 手动增加连通情况
    SetConnect(scheduler);

    const size_t reqNum1 = 1000;
    const size_t reqNum2 = 3;
    std::vector<std::string> texts;
    AddReqAddInstance(scheduler, texts, reqNum1);
    scheduler->ProcReq("test0", " ", MINDIE::MS::ReqType::UNKNOWN);

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * sleepCount));

    scheduler->UpdateReq("test0", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    scheduler->UpdateReq("test1", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    scheduler->UpdateReq("test3", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(reqHandleTime * sleepCount));

    std::vector<int> totalLengthFront(9, 0);
    std::vector<int> totalLength(9, 0);
    for (auto iter = reqNode.begin(); iter != reqNode.end(); ++iter) {
        auto reqid = std::stoi(iter->first.substr(4));
        int length = texts[reqid].size();
        totalLength[(iter->second).first] += length;
        totalLength[(iter->second).second] += length;
        if (reqid < 50000) {
            totalLengthFront[(iter->second).first] += length;
            totalLengthFront[(iter->second).second] += length;
        }
    }
    std::cout << "front:\n";
    for (int i = 0; i < 9; ++i) {
        std::cout << totalLengthFront[i] << " ";
    }
    std::cout << std::endl;

    std::cout << "total:\n";
    for (int i = 0; i < 9; ++i) {
        std::cout << totalLength[i] << " ";
    }
    std::cout << std::endl;
}

/*
测试描述: digs调度器使用pd分离负载均衡算法高并发调度,过程中关闭P节点
测试步骤:
    1. 配置DigsSchedulerImpl，使用负载均衡算法
    2. 向DefaultSchedulerImpl发送高并发请求，过程中关闭P节点，预期结果1
预期结果:
    1. 程序正常运行，请求被正确调度，后续关闭的节点不会被考虑使用
*/
TEST_F(TestDigsScheduler, TestLoadBalanceTC03)
{
    const size_t sleepTime = 100;
    const size_t reqHandleTime = 1000;
    const size_t sleepCount = 3;
    std::cout << "DIGS: main start!" << std::endl;

    MINDIE::MS::DIGSScheduler::Config config;
    InitConfig(config);

    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler =
        MINDIE::MS::SchedulerFactory::GetInstance().CreateScheduler("digs_scheduler", config);

    if (scheduler == nullptr) {
        std::cout << "DIGS: digs scheduler not find!" << std::endl;
        exit(0);
    }

    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    scheduler->RegisterPDNotifyAllocation([&reqNode](const std::string& reqId, uint64_t prefill,
        uint64_t decode) -> int32_t {
        std::cout << "DIGS: get allocation req: " << reqId << " prefill: " << prefill << " decode: " << decode <<
            std::endl;
        reqNode[reqId] = std::make_pair(prefill, decode);
        return 0;
    });

    RegisterInstance(scheduler);

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));

    // 关闭服务测试，需要关闭的实例id写入del变量即可
    std::vector<uint64_t> del{};
    scheduler->CloseInstance(del);

    // 手动增加连通情况
    SetConnect(scheduler);

    const size_t reqNum1 = 1000;
    const size_t reqNum2 = 3;
    std::vector<std::string> texts;
    AddReqCloseInstance(scheduler, texts, reqNum1);
    scheduler->ProcReq("test0", " ", MINDIE::MS::ReqType::UNKNOWN);

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * sleepCount));

    scheduler->UpdateReq("test0", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    scheduler->UpdateReq("test1", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    scheduler->UpdateReq("test3", MINDIE::MS::DIGSReqStage::DECODE, 0, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(reqHandleTime * sleepCount));

    std::vector<int> totalLengthBack(9, 0);
    std::vector<int> totalLength(9, 0);
    for (auto iter = reqNode.begin(); iter != reqNode.end(); ++iter) {
        auto reqid = std::stoi(iter->first.substr(4));
        int length = texts[reqid].size();
        totalLength[(iter->second).first] += length;
        totalLength[(iter->second).second] += length;
        if (reqid >= 500) {
            totalLengthBack[(iter->second).first] += length;
            totalLengthBack[(iter->second).second] += length;
        }
    }
    std::cout << "back:\n";
    for (int i = 0; i < 7; ++i) {
        std::cout << totalLengthBack[i] << " ";
    }
    std::cout << std::endl;

    std::cout << "total:\n";
    for (int i = 0; i < 7; ++i) {
        std::cout << totalLength[i] << " ";
    }
    std::cout << std::endl;
}

/*
测试描述: digs调度器使用pd分离负载均衡算法调度,资源满的时候暂停调度
测试步骤:
    1. 配置DigsSchedulerImpl，使用负载均衡算法，设置实例资源上限为15
    2. 向DefaultSchedulerImpl发送60条请求，不做UpdateReq，预期结果1
    3. 做UpdateReq，预期结果2
预期结果:
    1. 程序正常运行，45条请求被调度后暂停
    2. 剩下的请求也会被调度
*/
TEST_F(TestDigsScheduler, TestLoadBalanceTC04)
{
    const size_t sleepTime = 1000;
    const size_t reqHandleTime = 5000;
    const size_t sleepCount = 1;
    std::cout << "DIGS: main start!" << std::endl;

    MINDIE::MS::DIGSScheduler::Config config;
    InitConfig(config);

    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler =
        MINDIE::MS::SchedulerFactory::GetInstance().CreateScheduler("digs_scheduler", config);

    if (scheduler == nullptr) {
        std::cout << "DIGS: digs scheduler not find!" << std::endl;
        exit(0);
    }

    std::atomic<int> count = 0;
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    scheduler->RegisterPDNotifyAllocation([&reqNode, &scheduler, &count](const std::string& reqId,
        uint64_t prefill, uint64_t decode) -> int32_t {
        std::cout << "DIGS: get allocation req: " << reqId << " prefill: " << prefill << " decode: " << decode <<
            std::endl;
        reqNode[reqId] = std::make_pair(prefill, decode);
        count += 1;
        std::cout << "count: " << count << std::endl;
        return 0;
    });

    RegisterLimitedInstance(scheduler);

    // 关闭服务测试，需要关闭的实例id写入del变量即可
    std::vector<uint64_t> del{};
    scheduler->CloseInstance(del);

    // 手动增加连通情况
    SetConnect(scheduler);

    const size_t reqNum1 = 60;
    const size_t reqNum2 = 3;
    std::vector<std::string> texts;
    AddReq(scheduler, texts, reqNum1);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * sleepCount));
    std::vector<std::string> finishedReq;
    for (auto iter = reqNode.begin(); iter != reqNode.end(); ++iter) {
        finishedReq.push_back(iter->first);
    }
    for (int i = 0; i < finishedReq.size(); ++i)
    {
        scheduler->UpdateReq(finishedReq[i], MINDIE::MS::DIGSReqStage::DECODE, 0, 0);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * sleepCount));
}

/*
测试描述: digs调度器使用pd分离负载均衡算法调度,重复释放请求
测试步骤:
    1. 配置DigsSchedulerImpl，使用负载均衡算法
    2. 向DefaultSchedulerImpl发送请求，预期结果1
    3. 重复做UpdateReq，预期结果2
预期结果:
    1. 请求被正常调度
    2. 程序正常运行
*/
TEST_F(TestDigsScheduler, TestLoadBalanceTC05)
{
    const size_t sleepTime = 2000;
    const size_t reqHandleTime = 5000;
    const size_t sleepCount = 1;
    std::cout << "DIGS: main start!" << std::endl;

    MINDIE::MS::DIGSScheduler::Config config;
    InitConfig(config);

    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler =
        MINDIE::MS::SchedulerFactory::GetInstance().CreateScheduler("digs_scheduler", config);

    if (scheduler == nullptr) {
        std::cout << "DIGS: digs scheduler not find!" << std::endl;
        exit(0);
    }

    std::atomic<int> count = 0;
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    scheduler->RegisterPDNotifyAllocation([&reqNode, &scheduler, &count](const std::string& reqId,
        uint64_t prefill, uint64_t decode) -> int32_t {
        std::cout << "DIGS: get allocation req: " << reqId << " prefill: " << prefill << " decode: " << decode <<
            std::endl;
        reqNode[reqId] = std::make_pair(prefill, decode);
        scheduler->UpdateReq(reqId, MINDIE::MS::DIGSReqStage::DECODE, 0, 0);
        return 0;
    });

    RegisterLimitedInstance(scheduler);

    // 关闭服务测试，需要关闭的实例id写入del变量即可
    std::vector<uint64_t> del{};
    scheduler->CloseInstance(del);

    // 手动增加连通情况
    SetConnect(scheduler);

    const size_t reqNum1 = 2000;
    std::vector<std::string> texts;
    AddReq(scheduler, texts, reqNum1);
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * sleepCount));
    std::vector<std::string> finishedReq;

    for (auto iter = reqNode.begin(); iter != reqNode.end(); ++iter) {
        scheduler->UpdateReq(iter->first, MINDIE::MS::DIGSReqStage::DECODE, 0, 0);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * sleepCount));
}


/*
测试描述: digs调度器使用pd分离负载均衡算法调度,并发发送请求再并发结束请求
测试步骤:
    1. 配置DigsSchedulerImpl，使用负载均衡算法
    2. 向DefaultSchedulerImpl并发发送请求，预期结果1
    3. 并发UpdateReq，预期结果2
预期结果:
    1. 请求被正常调度
    2. 程序正常运行
*/
TEST_F(TestDigsScheduler, TestLoadBalanceTC06)
{
    const size_t sleepTime = 2000;
    const size_t reqHandleTime = 5000;
    const size_t sleepCount = 1;
    std::cout << "DIGS: main start!" << std::endl;

    MINDIE::MS::DIGSScheduler::Config config;
    InitConfig(config);

    std::unique_ptr<MINDIE::MS::DIGSScheduler> scheduler =
        MINDIE::MS::SchedulerFactory::GetInstance().CreateScheduler("digs_scheduler", config);

    if (scheduler == nullptr) {
        std::cout << "DIGS: digs scheduler not find!" << std::endl;
        exit(0);
    }

    std::atomic<int> count = 0;
    std::unordered_map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    scheduler->RegisterPDNotifyAllocation([&reqNode, &scheduler, &count](const std::string& reqId,
        uint64_t prefill, uint64_t decode) -> int32_t {
        std::cout << "DIGS: get allocation req: " << reqId << " prefill: " << prefill << " decode: " << decode <<
            std::endl;
        reqNode[reqId] = std::make_pair(prefill, decode);
        return 0;
    });

    RegisterInstance(scheduler);

    // 关闭服务测试，需要关闭的实例id写入del变量即可
    std::vector<uint64_t> del{};
    scheduler->CloseInstance(del);

    // 手动增加连通情况
    SetConnect(scheduler);

    const size_t reqNum1 = 1000;

    ThreadPool requestPool;
    std::vector<uint32_t> cores;
    requestPool.Init(8, cores);
    std::pair<std::function<void(uint32_t)>, uint32_t> requestTasks;
    requestTasks.second = reqNum1;
    requestTasks.first = [&scheduler](uint32_t tid) {
        std::string prompt = "Anyway stream truncated means that the other end closed the connection abruptly.";
        scheduler->ProcReq(std::to_string(tid), prompt, MINDIE::MS::ReqType::UNKNOWN);
        return 0;
    };
    requestPool.Active();
    requestPool.Enqueue(std::move(requestTasks));
    requestPool.Deactive();

    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime * sleepCount));

    ThreadPool updatePool;
    updatePool.Init(8, cores);
    std::pair<std::function<void(uint32_t)>, uint32_t> updateTasks;
    updateTasks.second = reqNum1;
    updateTasks.first = [&scheduler](uint32_t tid) {
        scheduler->UpdateReq(std::to_string(tid), MINDIE::MS::DIGSReqStage::DECODE, 0, 0);
    };
    updatePool.Active();
    updatePool.Enqueue(std::move(updateTasks));
    updatePool.Deactive();
}
