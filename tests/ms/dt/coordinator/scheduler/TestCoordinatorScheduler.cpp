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
#include <map>
#include <random>
#include <unistd.h>
#include <pthread.h>
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"
#define private public
#define protected public
#include "ThreadPool.h"
#include "DefaultScheduler.h"
#include "Helper.h"
using namespace MINDIE::MS;

class TestCoordinatorScheduler : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }

    void SetUp()
    {
        auto controllerJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
    }
};

void RegisterInstance(DefaultScheduler& scheduler, uint32_t nodeNum)
{
    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> instances(nodeNum);
    for (uint32_t i = 0; i < nodeNum; i++) {
        instances[i].id = i;
        instances[i].groupId;
        instances[i].role = MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE;
        instances[i].maxSeqLen = 2048;
        instances[i].maxOutputLen = 512;
        instances[i].totalSlotsNum = 200;
        instances[i].totalBlockNum = 1024;
        instances[i].blockSize = 128;
    }
    scheduler.RegisterInstance(instances);
}

void RegisterPDInstance(DefaultScheduler& scheduler, uint32_t pNodeNum, uint32_t dNodeNum)
{
    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> instances(pNodeNum + dNodeNum);
    for (uint32_t i = 0; i < pNodeNum; i++) {
        instances[i].id = i;
        instances[i].groupId;
        instances[i].role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        instances[i].label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
        instances[i].maxSeqLen = 2048;
        instances[i].maxOutputLen = 512;
        instances[i].totalSlotsNum = 200;
        instances[i].totalBlockNum = 1024;
        instances[i].blockSize = 128;
    }

    for (uint32_t i = pNodeNum; i < pNodeNum + dNodeNum; i++) {
        instances[i].id = i;
        instances[i].groupId;
        instances[i].role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        instances[i].label = MINDIE::MS::DIGSInstanceLabel::DECODE_STATIC;
        instances[i].maxSeqLen = 2048;
        instances[i].maxOutputLen = 512;
        instances[i].totalSlotsNum = 200;
        instances[i].totalBlockNum = 1024;
        instances[i].blockSize = 128;
    }

    for (uint32_t i = 0; i < pNodeNum + dNodeNum; i++) {
        std::cout << "instance " << i << " with role " << static_cast<uint32_t>(instances[i].role) << std::endl;
    }

    scheduler.RegisterInstance(instances);
}

void UpdateInstance(DefaultScheduler& scheduler, uint32_t nodeId, size_t availSlotsNum, size_t availBlockNum)
{
    std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> instances(1);
    instances[0].id = nodeId;
    instances[0].availSlotsNum = availSlotsNum;
    instances[0].availBlockNum = availBlockNum;
    scheduler.UpdateInstance(instances);
}

void UpdateDInstances(DefaultScheduler& scheduler, std::vector<uint32_t> nodeIdVec, size_t availSlotsNum,
    size_t availBlockNum, std::vector<std::vector<uint64_t>> &peers)
{
    std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> instances(nodeIdVec.size());
    for (uint32_t i = 0; i < nodeIdVec.size(); ++i) {
        instances[i].id = nodeIdVec[i];
        instances[i].availSlotsNum = availSlotsNum;
        instances[i].availBlockNum = availBlockNum;
        instances[i].peers = peers[i];
    }
    scheduler.UpdateInstance(instances);
}

void RegisterInstanceOneByOne(DefaultScheduler& scheduler, uint32_t nodeNum)
{
    for (uint32_t i = 0; i < nodeNum; i++) {
        MINDIE::MS::DIGSInstanceStaticInfo instance;
        instance.id = i;
        instance.groupId;
        instance.role = MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE;
        instance.maxSeqLen = 2048;
        instance.maxOutputLen = 512;
        instance.totalSlotsNum = 200;
        instance.totalBlockNum = 1024;
        instance.blockSize = 128;
        scheduler.RegisterInstance({instance});
    }
}

void RegisterPDInstanceOnebyOne(DefaultScheduler& scheduler, uint32_t pNodeNum, uint32_t dNodeNum)
{
    std::vector<MINDIE::MS::DIGSInstanceStaticInfo> instances(pNodeNum + dNodeNum);
    for (uint32_t i = 0; i < pNodeNum; i++) {
        instances[i].id = i;
        instances[i].groupId;
        instances[i].role = MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE;
        instances[i].label = MINDIE::MS::DIGSInstanceLabel::PREFILL_STATIC;
        instances[i].maxSeqLen = 2048;
        instances[i].maxOutputLen = 512;
        instances[i].totalSlotsNum = 200;
        instances[i].totalBlockNum = 1024;
        instances[i].blockSize = 128;
    }

    for (uint32_t i = pNodeNum; i < pNodeNum + dNodeNum; i++) {
        instances[i].id = i;
        instances[i].groupId;
        instances[i].role = MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE;
        instances[i].label = MINDIE::MS::DIGSInstanceLabel::DECODE_STATIC;
        instances[i].maxSeqLen = 2048;
        instances[i].maxOutputLen = 512;
        instances[i].totalSlotsNum = 200;
        instances[i].totalBlockNum = 1024;
        instances[i].blockSize = 128;
    }

    for (uint32_t i = 0; i < pNodeNum + dNodeNum; i++) {
        std::cout << "instance " << i << " with role " << static_cast<uint32_t>(instances[i].role) << std::endl;
        scheduler.RegisterInstance({instances[i]});
    }
}

void UpdateDInstancesOnebyOne(DefaultScheduler& scheduler, std::vector<uint32_t> nodeIdVec, size_t availSlotsNum,
    size_t availBlockNum, std::vector<std::vector<uint64_t>> &peers)
{
    std::vector<MINDIE::MS::DIGSInstanceDynamicInfo> instances(nodeIdVec.size());
    for (uint32_t i = 0; i < nodeIdVec.size(); ++i) {
        instances[i].id = nodeIdVec[i];
        instances[i].availSlotsNum = availSlotsNum;
        instances[i].availBlockNum = availBlockNum;
        instances[i].peers = peers[i];
        scheduler.UpdateInstance({instances[i]});
    }
}

nlohmann::json CreateRequest(uint32_t id, std::vector<std::string> contents)
{
    // OpenAI request
    nlohmann::json request;
    request["model"] = "llama2_7b";
    request["messages"] = nlohmann::json::array({});
    for (const auto &content : contents) {
        nlohmann::json message;
        message["role"] = "user";
        message["content"] = content;
        request["messages"].push_back(message);
    }
    request["max_tokens"] = 20;
    request["presence_penalty"] = 1.03;
    request["frequency_penalty"] = 1.0;
    request["seed"] = nullptr;
    request["temperature"] = 0.5;
    request["top_p"] = 0.95;
    request["stream"] = false;
    return request;
}

void PreProcReq(const nlohmann::json &bodyJson, std::string &prompt)
{
    if (bodyJson.contains("messages")) {
        auto tmp = bodyJson.at("messages");
        prompt = tmp.dump();
    }
}

/*
测试描述: 默认调度器使用单机prefixcache算法正常调度
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestPrefixCacheTC01)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "100";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "0.05";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();
    
    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt;
    PreProcReq(req1json, prompt);
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent2 = {"123", "456", "789"};
    nlohmann::json req2json = CreateRequest(2, reqContent2);
    PreProcReq(req2json, prompt);
    scheduler.ProcReq("2", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent3 = {"abc"};
    nlohmann::json req3json = CreateRequest(3, reqContent3);
    PreProcReq(req3json, prompt);
    scheduler.ProcReq("3", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent4 = {"abc", "def", "ghi"};
    nlohmann::json req4json = CreateRequest(4, reqContent4);
    PreProcReq(req4json, prompt);
    scheduler.ProcReq("4", prompt, MINDIE::MS::ReqType::OPENAI);

    sleep(1);
    EXPECT_EQ(reqNode["1"], reqNode["2"]);
    EXPECT_EQ(reqNode["3"], reqNode["4"]);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用单机prefixcache算法正常调度，资源不足情况
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestPrefixCacheTC02)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "100";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "0.05";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();
    
    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    
    // 资源充足，能够调度同一个
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt;
    PreProcReq(req1json, prompt);
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent2 = {"123", "456", "789"};
    nlohmann::json req2json = CreateRequest(2, reqContent2);
    PreProcReq(req2json, prompt);
    scheduler.ProcReq("2", prompt, MINDIE::MS::ReqType::OPENAI);

    // 资源不足，调度不同的
    std::vector<std::string> reqContent3 = {"123"};
    nlohmann::json req3json = CreateRequest(3, reqContent3);
    PreProcReq(req3json, prompt);
    scheduler.ProcReq("3", prompt, MINDIE::MS::ReqType::OPENAI);

    sleep(1);
    UpdateInstance(scheduler, reqNode["3"], 0, 0);

    std::vector<std::string> reqContent4 = {"123", "456", "789"};
    nlohmann::json req4json = CreateRequest(4, reqContent4);
    PreProcReq(req4json, prompt);
    scheduler.ProcReq("4", prompt, MINDIE::MS::ReqType::OPENAI);

    sleep(1);
    EXPECT_EQ(reqNode["1"], reqNode["2"]);
    EXPECT_NE(reqNode["3"], reqNode["4"]);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用单机prefixcache算法正常调度，不命中的情况
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestPrefixCacheTC03)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "100";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "0.05";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();
    
    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt;
    PreProcReq(req1json, prompt);
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent2 = {"456"};
    nlohmann::json req2json = CreateRequest(2, reqContent2);
    PreProcReq(req2json, prompt);
    scheduler.ProcReq("2", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent3 = {"abc"};
    nlohmann::json req3json = CreateRequest(3, reqContent3);
    PreProcReq(req3json, prompt);
    scheduler.ProcReq("3", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent4 = {"def"};
    nlohmann::json req4json = CreateRequest(4, reqContent4);
    PreProcReq(req4json, prompt);
    scheduler.ProcReq("4", prompt, MINDIE::MS::ReqType::OPENAI);

    sleep(1);
    EXPECT_NE(reqNode["1"], reqNode["2"]);
    EXPECT_NE(reqNode["2"], reqNode["3"]);
    EXPECT_NE(reqNode["3"], reqNode["4"]);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用单机prefixcache算法正常调度，在cache缓存已满，并接收新的推理请求时，将最近未处理的请求移除cache，缓存新的请求
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestPrefixCacheTC04)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "1";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "0.05";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();
    
    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt;
    PreProcReq(req1json, prompt);
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent2 = {"456"};
    nlohmann::json req2json = CreateRequest(2, reqContent2);
    PreProcReq(req2json, prompt);
    scheduler.ProcReq("2", prompt, MINDIE::MS::ReqType::OPENAI);

    std::vector<std::string> reqContent3 = {"123", "456", "789"};
    nlohmann::json req3json = CreateRequest(3, reqContent3);
    PreProcReq(req3json, prompt);
    scheduler.ProcReq("3", prompt, MINDIE::MS::ReqType::OPENAI);

    sleep(1);
    EXPECT_NE(reqNode["3"], reqNode["1"]);
    
    scheduler.Stop();
}

/*
测试描述: 默认调度器使用单机prefixcache算法负例，当推理请求中messages字段内容为空时，调度失败
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestPrefixCacheMessageNullTCa)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "100";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "0.05";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();
    
    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    
    std::vector<std::string> reqContent1 = {};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt;
    PreProcReq(req1json, prompt);
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::OPENAI);

    sleep(1);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用单机prefixcache算法负例：cache_size 超范围
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法，配置cache_size = 0
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 启动失败
*/
TEST_F(TestCoordinatorScheduler, DISABLED_TestPrefixCacheCacheSizeErrorTCa)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "0";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "0.05";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();

    sleep(1);

    scheduler.Stop();

}

/*
测试描述: 默认调度器使用单机prefixcache算法负例：slots_thresh 超范围
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法，配置slots_thresh = -1
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 启动失败
*/
TEST_F(TestCoordinatorScheduler, DISABLED_TestPrefixCacheSlotsThreshErrorTCa)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "100";
    config["slots_thresh"] = "-1";
    config["block_thresh"] = "0.05";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();

    sleep(1);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用单机prefixcache算法负例：block_thresh 超范围
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法，配置block_thresh = -1
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 启动失败
*/
TEST_F(TestCoordinatorScheduler, DISABLED_TestPrefixCacheBlockThreshErrorTCa)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "100";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "-1";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();

    sleep(1);

    scheduler.Stop();
}

void GenerateTexts(std::vector<std::string>& texts, int num)
{
    std::mt19937 generator;
    std::uniform_int_distribution<int> distribution(97, 122);
    for (size_t idx = 0; idx < num; ++idx) {
        std::string text;
        for (int i = 0; i < 30; ++i) {
            char c = static_cast<char>(distribution(generator));
            text += c;
        }
        texts.push_back(text);
    }
}

/*
测试描述: 默认调度器使用单机prefixcache算法并行调度
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送多条命中缓存的并发请求，预期结果1
预期结果:
    1. 程序正常运行，命中缓存的请求被调度到相同节点
*/
TEST_F(TestCoordinatorScheduler, TestPrefixCacheParallelTC01)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "1000";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "0.05";

    DefaultScheduler scheduler(config);
    std::map<int, uint64_t> reqNode;
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[std::stoi(reqId)] = nodeId;
        return 0;
    });
    scheduler.Start();

    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);

    std::vector<std::string> texts(500);
    for (int i = 0; i < 500; ++i) {
        texts[i] = "texts ";
        texts[i] += std::to_string(i);
    }

    ThreadPool pool;
    std::vector<uint32_t> cores;
    pool.Init(8, cores);

    std::pair<std::function<void(uint32_t)>, uint32_t> tasks;
    tasks.second = 500;
    tasks.first = [&scheduler, &texts](uint32_t tid) {
        std::vector<std::string> reqContent = {texts[tid]};
        nlohmann::json reqJson = CreateRequest(1, reqContent);
        std::string prompt;
        PreProcReq(reqJson, prompt);
        scheduler.ProcReq(std::to_string(tid), prompt, MINDIE::MS::ReqType::OPENAI);
    };
    pool.Active();
    pool.Enqueue(std::move(tasks));
    pool.Deactive();
    sleep(1);

    std::pair<std::function<void(uint32_t)>, uint32_t> tasks2;
    tasks2.second = 500;
    tasks2.first = [&scheduler, &texts](uint32_t tid) {
        std::vector<std::string> reqContent = {texts[tid]};
        reqContent.push_back("hello");
        reqContent.push_back("world");
        nlohmann::json reqJson = CreateRequest(1, reqContent);
        std::string prompt;
        PreProcReq(reqJson, prompt);
        scheduler.ProcReq(std::to_string(tid + 500), prompt, MINDIE::MS::ReqType::OPENAI); // 500表示请求数
    };
    pool.Active();
    pool.Enqueue(std::move(tasks2));
    pool.Deactive();
    sleep(1);

    scheduler.Stop();
    std::cout << "size of reqNode: " << reqNode.size() << std::endl;
    for (int i = 0; i < 500; ++i) {
        EXPECT_EQ(reqNode[i], reqNode[i + 500]);
    }
}

/*
测试描述: 默认调度器使用单机prefixcache算法并行调度
测试步骤:
    1. 配置DefaultScheduler，使用prefixcache算法
    2. 向DefaultScheduler发送多条不命中缓存的并发请求，预期结果1
预期结果:
    1. 程序正常运行，请求被调度到不同节点
*/
TEST_F(TestCoordinatorScheduler, TestPrefixCacheParallelTC02)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "cache_affinity";
    config["cache_size"] = "100";
    config["slots_thresh"] = "0.05";
    config["block_thresh"] = "0.05";

    DefaultScheduler scheduler(config);
    std::map<std::string, uint64_t> reqNode;
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();

    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);

    ThreadPool pool;
    std::vector<uint32_t> cores;
    pool.Init(8, cores);

    std::pair<std::function<void(uint32_t)>, uint32_t> tasks;
    tasks.second = 1002;
    tasks.first = [&scheduler](uint32_t tid) {
        std::vector<std::string> reqContent = {"123"};
        nlohmann::json reqJson = CreateRequest(1, reqContent);
        std::string prompt;
        PreProcReq(reqJson, prompt);
        scheduler.ProcReq(std::to_string(tid), prompt, MINDIE::MS::ReqType::OPENAI);
    };
    pool.Active();
    pool.Enqueue(std::move(tasks));
    pool.Deactive();
    sleep(1);

    scheduler.Stop();
    std::cout << "size of reqNode: " << reqNode.size() << std::endl;
    std::vector<int> count(3, 0);
    for (auto iter = reqNode.begin(); iter != reqNode.end(); ++iter) {
        count[iter->second]++;
    }
    std::cout << "num of node-0: " << count[0] << std::endl;
    std::cout << "num of node-1: " << count[1] << std::endl;
    std::cout << "num of node-2: " << count[2] << std::endl;
    EXPECT_EQ(count[0], count[1]);
    EXPECT_EQ(count[0], count[2]);
}

/*
测试描述: 默认调度器使用单机roundrobin算法正常调度
测试步骤:
    1. 配置DefaultScheduler，使用roundrobin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestRoundRobinTC01)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "round_robin";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();
    
    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt = req1json["messages"].dump();
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent2 = {"456", "789"};
    nlohmann::json req2json = CreateRequest(2, reqContent2);
    prompt = req2json["messages"].dump();
    scheduler.ProcReq("2", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent3 = {"abc"};
    nlohmann::json req3json = CreateRequest(3, reqContent3);
    prompt = req3json["messages"].dump();
    scheduler.ProcReq("3", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent4 = {"abc", "ghi"};
    nlohmann::json req4json = CreateRequest(4, reqContent4);
    prompt = req4json["messages"].dump();
    scheduler.ProcReq("4", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent5 = {"abc", "ghi"};
    nlohmann::json req5json = CreateRequest(4, reqContent5);
    prompt = req5json["messages"].dump();
    scheduler.ProcReq("5", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent6 = {"abc", "ghi"};
    nlohmann::json req6json = CreateRequest(6, reqContent6);
    prompt = req6json["messages"].dump();
    scheduler.ProcReq("6", prompt, MINDIE::MS::ReqType::UNKNOWN);

    sleep(1);
    EXPECT_NE(reqNode["1"], reqNode["2"]);
    EXPECT_NE(reqNode["2"], reqNode["3"]);
    EXPECT_EQ(reqNode["1"], reqNode["4"]);
    EXPECT_EQ(reqNode["2"], reqNode["5"]);
    EXPECT_EQ(reqNode["3"], reqNode["6"]);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用pd分离的roundrobin算法正常调度
测试步骤:
    1. 配置DefaultScheduler，使用roundrobin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestRoundRobinTC02)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "pd_separate";
    config["algorithm_type"] = "round_robin";
    
    std::map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterPDNotifyAllocation([&reqNode](std::string reqId, uint64_t pNode, uint64_t dNode) {
        std::cout << "ReqId: " << reqId << ", pNode: " << pNode << ", dNode: " << dNode << std::endl;
        reqNode[reqId] = std::make_pair(pNode, dNode);
        return 0;
    });
    scheduler.Start();
    
    RegisterPDInstance(scheduler, 3, 2);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    std::vector<uint64_t> peers3 = {0, 2};
    std::vector<uint64_t> peers4 = {1};
    std::vector<uint32_t> ids = {3, 4};
    std::vector<std::vector<uint64_t>> peers;
    peers.emplace_back(peers3);
    peers.emplace_back(peers4);
    UpdateDInstances(scheduler, ids, 200, 1024, peers);
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt = req1json["messages"].dump();
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent2 = {"456", "789"};
    nlohmann::json req2json = CreateRequest(2, reqContent2);
    prompt = req2json["messages"].dump();
    scheduler.ProcReq("2", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent3 = {"abc"};
    nlohmann::json req3json = CreateRequest(3, reqContent3);
    prompt = req3json["messages"].dump();
    scheduler.ProcReq("3", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent4 = {"abc", "ghi"};
    nlohmann::json req4json = CreateRequest(4, reqContent4);
    prompt = req4json["messages"].dump();
    scheduler.ProcReq("4", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent5 = {"abc", "ghi"};
    nlohmann::json req5json = CreateRequest(4, reqContent5);
    prompt = req5json["messages"].dump();
    scheduler.ProcReq("5", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent6 = {"abc", "ghi"};
    nlohmann::json req6json = CreateRequest(6, reqContent6);
    prompt = req6json["messages"].dump();
    scheduler.ProcReq("6", prompt, MINDIE::MS::ReqType::UNKNOWN);

    sleep(1);
    EXPECT_NE(reqNode["1"].first, reqNode["2"].first);
    EXPECT_NE(reqNode["1"].first, reqNode["3"].first);
    EXPECT_EQ(reqNode["1"].first, reqNode["4"].first);
    EXPECT_EQ(reqNode["2"].first, reqNode["5"].first);
    EXPECT_EQ(reqNode["3"].first, reqNode["6"].first);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用单机roundrobin算法，没有可用节点
测试步骤:
    1. 配置DefaultScheduler，使用roundrobin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestRoundRobinTC03)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "round_robin";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt = req1json["messages"].dump();
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::UNKNOWN);
    usleep(10000);

    EXPECT_EQ(reqNode.size(), 0);

    RegisterInstance(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);

    sleep(1);
    EXPECT_EQ(reqNode.size(), 1);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用pd分离的roundrobin算法，没有可用节点
测试步骤:
    1. 配置DefaultScheduler，使用roundrobin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestRoundRobinTC04)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "pd_separate";
    config["algorithm_type"] = "round_robin";
    
    std::map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterPDNotifyAllocation([&reqNode](std::string reqId, uint64_t pNode, uint64_t dNode) {
        std::cout << "ReqId: " << reqId << ", pNode: " << pNode << ", dNode: " << dNode << std::endl;
        reqNode[reqId] = std::make_pair(pNode, dNode);
        return 0;
    });
    scheduler.Start();
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt = req1json["messages"].dump();
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::UNKNOWN);
    usleep(10000);
    EXPECT_EQ(reqNode.size(), 0);

    RegisterPDInstance(scheduler, 3, 2);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    std::vector<uint64_t> peers3 = {0, 2};
    std::vector<uint64_t> peers4 = {1};
    std::vector<uint32_t> ids = {3, 4};
    std::vector<std::vector<uint64_t>> peers;
    peers.emplace_back(peers3);
    peers.emplace_back(peers4);
    UpdateDInstances(scheduler, ids, 200, 1024, peers);
    
    sleep(1);
    EXPECT_EQ(reqNode.size(), 1);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用pd分离的roundrobin算法，没有可用d节点
测试步骤:
    1. 配置DefaultScheduler，使用roundrobin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestRoundRobinTC05)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "pd_separate";
    config["algorithm_type"] = "round_robin";
    
    std::map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterPDNotifyAllocation([&reqNode](std::string reqId, uint64_t pNode, uint64_t dNode) {
        std::cout << "ReqId: " << reqId << ", pNode: " << pNode << ", dNode: " << dNode << std::endl;
        reqNode[reqId] = std::make_pair(pNode, dNode);
        return 0;
    });
    scheduler.Start();
    
    RegisterPDInstance(scheduler, 3, 2);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);

    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt = req1json["messages"].dump();
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::UNKNOWN);
    usleep(10000);
    EXPECT_EQ(reqNode.size(), 0);
    
    std::vector<uint64_t> peers3 = {0, 2};
    std::vector<uint64_t> peers4 = {1};
    std::vector<uint32_t> ids = {3, 4};
    std::vector<std::vector<uint64_t>> peers;
    peers.emplace_back(peers3);
    peers.emplace_back(peers4);
    UpdateDInstances(scheduler, ids, 200, 1024, peers);
    
    sleep(1);
    EXPECT_EQ(reqNode.size(), 1);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用单机roundrobin算法正常调度, 一个一个添加实例
测试步骤:
    1. 配置DefaultScheduler，使用roundrobin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestRoundRobinTC06)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "single_node";
    config["algorithm_type"] = "round_robin";
    
    std::map<std::string, uint64_t> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterSingleNodeNotifyAllocation([&reqNode](std::string reqId, uint64_t nodeId) {
        std::cout << "ReqId: " << reqId << ", NodeId: " << nodeId << std::endl;
        reqNode[reqId] = nodeId;
        return 0;
    });
    scheduler.Start();
    
    RegisterInstanceOneByOne(scheduler, 3);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt = req1json["messages"].dump();
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent2 = {"456", "789"};
    nlohmann::json req2json = CreateRequest(2, reqContent2);
    prompt = req2json["messages"].dump();
    scheduler.ProcReq("2", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent3 = {"abc"};
    nlohmann::json req3json = CreateRequest(3, reqContent3);
    prompt = req3json["messages"].dump();
    scheduler.ProcReq("3", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent4 = {"abc", "ghi"};
    nlohmann::json req4json = CreateRequest(4, reqContent4);
    prompt = req4json["messages"].dump();
    scheduler.ProcReq("4", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent5 = {"abc", "ghi"};
    nlohmann::json req5json = CreateRequest(4, reqContent5);
    prompt = req5json["messages"].dump();
    scheduler.ProcReq("5", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent6 = {"abc", "ghi"};
    nlohmann::json req6json = CreateRequest(6, reqContent6);
    prompt = req6json["messages"].dump();
    scheduler.ProcReq("6", prompt, MINDIE::MS::ReqType::UNKNOWN);

    sleep(1);
    EXPECT_NE(reqNode["1"], reqNode["2"]);
    EXPECT_NE(reqNode["2"], reqNode["3"]);
    EXPECT_EQ(reqNode["1"], reqNode["4"]);
    EXPECT_EQ(reqNode["2"], reqNode["5"]);
    EXPECT_EQ(reqNode["3"], reqNode["6"]);

    scheduler.Stop();
}

/*
测试描述: 默认调度器使用pd分离的roundrobin算法正常调度, 一个一个添加实例
测试步骤:
    1. 配置DefaultScheduler，使用roundrobin算法
    2. 向DefaultScheduler发送请求，预期结果1
预期结果:
    1. 请求被正确调度
*/
TEST_F(TestCoordinatorScheduler, TestRoundRobinTC07)
{
    MINDIE::MS::DIGSScheduler::Config config;
    config["deploy_mode"] = "pd_separate";
    config["algorithm_type"] = "round_robin";
    
    std::map<std::string, std::pair<uint64_t, uint64_t>> reqNode;
    DefaultScheduler scheduler(config);
    scheduler.RegisterPDNotifyAllocation([&reqNode](std::string reqId, uint64_t pNode, uint64_t dNode) {
        std::cout << "ReqId: " << reqId << ", pNode: " << pNode << ", dNode: " << dNode << std::endl;
        reqNode[reqId] = std::make_pair(pNode, dNode);
        return 0;
    });
    scheduler.Start();
    
    RegisterPDInstanceOnebyOne(scheduler, 3, 2);
    UpdateInstance(scheduler, 0, 200, 1024);
    UpdateInstance(scheduler, 1, 200, 1024);
    UpdateInstance(scheduler, 2, 200, 1024);
    std::vector<uint64_t> peers3 = {0, 2};
    std::vector<uint64_t> peers4 = {1};
    std::vector<uint32_t> ids = {3, 4};
    std::vector<std::vector<uint64_t>> peers;
    peers.emplace_back(peers3);
    peers.emplace_back(peers4);
    UpdateDInstancesOnebyOne(scheduler, ids, 200, 1024, peers);
    
    std::vector<std::string> reqContent1 = {"123"};
    nlohmann::json req1json = CreateRequest(1, reqContent1);
    std::string prompt = req1json["messages"].dump();
    scheduler.ProcReq("1", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent2 = {"456", "789"};
    nlohmann::json req2json = CreateRequest(2, reqContent2);
    prompt = req2json["messages"].dump();
    scheduler.ProcReq("2", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent3 = {"abc"};
    nlohmann::json req3json = CreateRequest(3, reqContent3);
    prompt = req3json["messages"].dump();
    scheduler.ProcReq("3", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent4 = {"abc", "ghi"};
    nlohmann::json req4json = CreateRequest(4, reqContent4);
    prompt = req4json["messages"].dump();
    scheduler.ProcReq("4", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent5 = {"abc", "ghi"};
    nlohmann::json req5json = CreateRequest(4, reqContent5);
    prompt = req5json["messages"].dump();
    scheduler.ProcReq("5", prompt, MINDIE::MS::ReqType::UNKNOWN);

    std::vector<std::string> reqContent6 = {"abc", "ghi"};
    nlohmann::json req6json = CreateRequest(6, reqContent6);
    prompt = req6json["messages"].dump();
    scheduler.ProcReq("6", prompt, MINDIE::MS::ReqType::UNKNOWN);

    sleep(1);
    EXPECT_NE(reqNode["1"].first, reqNode["2"].first);
    EXPECT_NE(reqNode["1"].first, reqNode["3"].first);
    EXPECT_EQ(reqNode["1"].first, reqNode["4"].first);
    EXPECT_EQ(reqNode["2"].first, reqNode["5"].first);
    EXPECT_EQ(reqNode["3"].first, reqNode["6"].first);

    scheduler.Stop();
}