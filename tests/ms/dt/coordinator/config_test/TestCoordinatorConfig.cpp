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
#include <cstdlib> // #include <stdlib.h>
#include <string>
#include <memory>
#include <map>
#include <random>
#include <unistd.h>
#include <variant>
#include <vector>
#include <pthread.h>
#include "gtest/gtest.h"
#define private public
#define protected public
#include "Configure.h"
#include "Helper.h"
#include "JsonFileManager.h"
using namespace MINDIE::MS;

// 修改Json文件的值Param，分别是mod/del、key、value
using ParamType = std::tuple<std::string, std::vector<std::string>, std::variant<int, float, std::string>>;

// 封装修改Json文件的函数
static int ModifyJsonFile(std::string json_path, ParamType param)
{
    JsonFileManager manager(json_path);
    manager.Load();

    auto mode = std::get<0>(param);
    auto keys = std::get<1>(param);
    auto value = std::get<2>(param);

    if (mode.compare("mod") == 0) {
        std::visit([&manager, &keys](auto&& val) {
            manager.SetList(keys, val);
            }, value);
    } else if (mode.compare("del") == 0) {
        manager.EraseList(keys);
    }

    manager.Save();
    
    return 0;
}

// 最普通的Fixture
class TestCoordinatorConfig : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 复制临时文件
        CopyFile(GetMSCoordinatorConfigJsonPath(), GetMSCoordinatorTestJsonPath());
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", coordinatorJson.c_str(), 1);
        std::cout << "MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH=" << coordinatorJson << std::endl;
        
        // 初始化配置
        SetMSCoordinatorJsonDefault(coordinatorJson);
        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({ "tls_config", "controller_server_tls_enable" }, true);
        manager.SetList({ "tls_config", "request_server_tls_enable" }, true);
        manager.SetList({ "tls_config", "mindie_client_tls_enable" }, true);
        manager.Save();

        unsetenv("MINDIE_MS_COORDINATOR_CONFIG_MAX_REQ");
        unsetenv("MINDIE_MS_COORDINATOR_CONFIG_SINGLE_NODE_MAX_REQ");
    }
    void TearDown() override
    {
        unsetenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH");
        unsetenv("MINDIE_MS_COORDINATOR_CONFIG_MAX_REQ");
        unsetenv("MINDIE_MS_COORDINATOR_CONFIG_SINGLE_NODE_MAX_REQ");
    }
};
// 配置文件通用项初始化
class TestCoordinatorConfigCommonParam : public TestCoordinatorConfig, public ::testing::WithParamInterface<ParamType> {
};
class TestCoordinatorLogConfigCommonParam : public TestCoordinatorConfig,
    public ::testing::WithParamInterface<ParamType> {
};

// cache_affinity配置项初始化
class TestCoordinatorConfigCAParam : public TestCoordinatorConfig, public ::testing::WithParamInterface<ParamType> {
protected:
    void SetUp()
    {
        TestCoordinatorConfig::SetUp();
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        
        // 初始化配置
        SetMSCoordinatorJsonDefault(coordinatorJson);
        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"digs_scheduler_config", "deploy_mode"}, "single_node");
        manager.SetList({"digs_scheduler_config", "algorithm_type"}, "cache_affinity");
        manager.Save();
    }
};

// load_balance配置项初始化
class TestCoordinatorConfigLBParam : public TestCoordinatorConfig, public ::testing::WithParamInterface<ParamType> {
protected:
    void SetUp()
    {
        TestCoordinatorConfig::SetUp();
        auto coordinatorJson = GetMSCoordinatorTestJsonPath();
        
        // 初始化配置
        SetMSCoordinatorJsonDefault(coordinatorJson);
        JsonFileManager manager(coordinatorJson);
        manager.Load();
        manager.SetList({"digs_scheduler_config", "deploy_mode"}, "pd_separate");
        manager.SetList({"digs_scheduler_config", "algorithm_type"}, "load_balance");
        manager.Save();
    }
};

/*
测试描述: Coordinator配置文件测试，正例
测试步骤:
    1. 传入正常的配置文件路径，预期结果1
预期结果:
    1. 解析配置成功
*/
TEST_F(TestCoordinatorConfig, LoadNormalConfigTC)
{
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, 0);
}

/*
测试描述: Coordinator配置文件测试，正例，singleNodeMaxReqEnv != nullptr
测试步骤:
    1. 传入正常的配置文件路径，预期结果1
预期结果:
    1. 解析配置成功
*/
TEST_F(TestCoordinatorConfig, LoadNormalConfigsingleNodeMaxReqEnvTCa)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_MAX_REQ", "123", 1);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, 0);
}

/*
测试描述: Coordinator配置文件测试，负例，singleNodeMaxReqEnv != nullptr
测试步骤:
    1. 传入正常的配置文件路径，预期结果1
预期结果:
    1. 解析配置失败
*/
TEST_F(TestCoordinatorConfig, LoadNormalConfigsingleNodeMaxReqEnvTCb)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_MAX_REQ", "abc", 1);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Coordinator配置文件测试，负例，singleNodeMaxReqEnv != nullptr
测试步骤:
    1. 传入正常的配置文件路径，预期结果1
预期结果:
    1. 解析配置失败，使用默认配置
*/
TEST_F(TestCoordinatorConfig, LoadNormalConfigsingleNodeMaxReqEnvTCc)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_MAX_REQ", "0", 1);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, 0);
}

/*
测试描述: Coordinator配置文件测试，正例，singleNodeMaxReqEnv != nullptr
测试步骤:
    1. 传入正常的配置文件路径，预期结果1
预期结果:
    1. 解析配置成功
*/
TEST_F(TestCoordinatorConfig, LoadNormalConfigmaxReqStrTCa)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_SINGLE_NODE_MAX_REQ", "123", 1);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, 0);
}

/*
测试描述: Coordinator配置文件测试，负例，singleNodeMaxReqEnv != nullptr
测试步骤:
    1. 传入正常的配置文件路径，预期结果1
预期结果:
    1. 解析配置失败
*/
TEST_F(TestCoordinatorConfig, LoadNormalConfigmaxReqStrTCb)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_SINGLE_NODE_MAX_REQ", "abc", 1);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Coordinator配置文件测试，负例，singleNodeMaxReqEnv != nullptr
测试步骤:
    1. 传入正常的配置文件路径，预期结果1
预期结果:
    1. 解析配置失败，使用默认配置
*/
TEST_F(TestCoordinatorConfig, LoadNormalConfigmaxReqStrTCc)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_SINGLE_NODE_MAX_REQ", "0", 1);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, 0);
}

/*
测试描述: Coordinator配置文件测试，FileToBuffer错误
测试步骤:
    1. 传入错误的配置文件路径，预期结果1
预期结果:
    1. 解析配置失败，返回-1
*/
TEST_F(TestCoordinatorConfig, LoadFileToBufferErrorTC)
{
    setenv("MINDIE_MS_COORDINATOR_CONFIG_FILE_PATH", "/path/not/exist", 1);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Coordinator配置文件测试，覆盖不同deploymode的algorithm_type分支
测试步骤:
    1. 传入错误的配置文件路径，预期结果1
预期结果:
    1. 解析配置失败，返回-1
*/
TEST_F(TestCoordinatorConfig, DeployModeParamErrorTCa)
{
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"digs_scheduler_config", "deploy_mode"}, "single_node");
    manager.SetList({"digs_scheduler_config", "algorithm_type"}, "abc");
    manager.Save();
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Coordinator配置文件测试，覆盖不同deploymode的algorithm_type分支
测试步骤:
    1. 传入错误的配置文件路径，预期结果1
预期结果:
    1. 解析配置失败，返回-1
*/
TEST_F(TestCoordinatorConfig, DeployModeParamErrorTCb)
{
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"digs_scheduler_config", "deploy_mode"}, "pd_separate");
    manager.SetList({"digs_scheduler_config", "algorithm_type"}, "abc");
    manager.Save();
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Coordinator配置文件测试，不放行全零监听但推理请求监听ip为全零
测试步骤:
    1. 传入错误的配置文件路径，预期结果1
预期结果:
    1. 解析配置失败，返回-1
*/
TEST_F(TestCoordinatorConfig, AllZeroIpConflictSet)
{
    auto coordinatorJson = GetMSCoordinatorTestJsonPath();
    JsonFileManager manager(coordinatorJson);
    manager.Load();
    manager.SetList({"http_config", "allow_all_zero_ip_listening"}, false);
    manager.SetList({"http_config", "predict_ip"}, "0.0.0.0");
    manager.Save();
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Coordinator配置文件测试，配置文件中通用的部分修改为异常项
测试步骤:
    1. 修改正常配置文件中的项为异常
预期结果:
    1. 解析配置失败，返回-1
*/
TEST_P(TestCoordinatorConfigCommonParam, LoadModCommonConfigTC)
{
    auto param = GetParam();
    ModifyJsonFile(GetMSCoordinatorTestJsonPath(), param);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Coordinator配置文件测试，配置文件中通用的部分修改为异常项
测试步骤:
    1. 修改正常配置文件中的项为异常
预期结果:
    1. 解析配置成功，返回0
*/
TEST_P(TestCoordinatorLogConfigCommonParam, LoadModLogCommonConfigTC)
{
    auto param = GetParam();
    ModifyJsonFile(GetMSCoordinatorTestJsonPath(), param);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, 0);
}

/*
测试描述: Coordinator配置文件测试，配置文件中cache_affinity配置项修改为异常项
测试步骤:
    1. 修改正常配置文件中的项为异常
预期结果:
    1. 解析配置失败，返回-1
*/
TEST_P(TestCoordinatorConfigCAParam, LoadModCAConfigTC)
{
    auto param = GetParam();
    ModifyJsonFile(GetMSCoordinatorTestJsonPath(), param);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Coordinator配置文件测试，配置文件中load_balance配置项修改为异常项
测试步骤:
    1. 修改正常配置文件中的项为异常
预期结果:
    1. 解析配置失败，返回-1
*/
TEST_P(TestCoordinatorConfigLBParam, LoadModLBConfigTC)
{
    auto param = GetParam();
    ModifyJsonFile(GetMSCoordinatorTestJsonPath(), param);
    Configure confi;
    int32_t ret = confi.Init();
    EXPECT_EQ(ret, -1);
}

INSTANTIATE_TEST_SUITE_P(
    TestCoordinatorLogConfigCommonParams,
    TestCoordinatorLogConfigCommonParam,
    ::testing::Values(
        // log_info
        ParamType{"mod", {"log_info"}, 123}, // 类型错误
        ParamType{"del", {"log_info"}, 123}, // 删除log_info

        ParamType{"mod", {"log_info", "to_file"}, 123}, // 类型错误
        ParamType{"del", {"log_info", "to_file"}, 123}, // 删除to_file

        ParamType{"mod", {"log_info", "to_stdout"}, 123}, // 类型错误
        ParamType{"del", {"log_info", "to_stdout"}, 123}, // 删除to_stdout

        ParamType{"mod", {"log_info", "run_log_path"}, 123}, // 类型错误
        ParamType{"del", {"log_info", "run_log_path"}, 123}, // 删除run_log_path

        ParamType{"mod", {"log_info", "operation_log_path"}, 123}, // 类型错误
        ParamType{"del", {"log_info", "operation_log_path"}, 123}, // 删除operation_log_path

        ParamType{"mod", {"log_info", "log_level"}, 123}, // 类型错误
        ParamType{"mod", {"log_info", "log_level"}, "123"}, // unknown level
        ParamType{"mod", {"log_info", "log_level"}, "abc"}, // unknown level
        ParamType{"del", {"log_info", "log_level"}, 123}, // 删除log_level

        ParamType{"mod", {"log_info", "max_log_str_size"}, "123"}, // 类型错误
        ParamType{"mod", {"log_info", "max_log_str_size"}, 0}, // 超出范围
        ParamType{"del", {"log_info", "max_log_str_size"}, 123}, // 删除run_log_path

        ParamType{"mod", {"log_info", "max_log_file_size"}, "123"}, // 类型错误
        ParamType{"mod", {"log_info", "max_log_file_size"}, 0}, // 超出范围
        ParamType{"del", {"log_info", "max_log_file_size"}, 123}, // 删除run_log_path

        ParamType{"mod", {"log_info", "max_log_file_num"}, "123"}, // 类型错误
        ParamType{"mod", {"log_info", "max_log_file_num"}, 0}, // 超出范围
        ParamType{"del", {"log_info", "max_log_file_num"}, 123} // 删除run_log_path
    )
);

// 通用配置项的参数
INSTANTIATE_TEST_SUITE_P(
    TestCoordinatorConfigCommonParams,
    TestCoordinatorConfigCommonParam,
    ::testing::Values(
        // http_config
        ParamType{"mod", {"http_config"}, 123}, // 类型错误
        ParamType{"del", {"http_config"}, 123}, // 删除http_config

        ParamType{"del", {"http_config", "allow_all_zero_ip_listening"}, 123}, // 删除全零监听配置项

        ParamType{"mod", {"http_config", "predict_ip"}, 123}, // 类型错误
        ParamType{"mod", {"http_config", "predict_ip"}, "123"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_ip"}, "-1.0.0.0"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_ip"}, "0.0.0.0"}, // 非法
        ParamType{"mod", {"http_config", "predict_ip"}, "256.0.5.5"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_ip"}, "127.0.0.1."}, // 格式非法
        ParamType{"mod", {"http_config", "predict_ip"}, "127.0.0.x"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_ip"}, "asd"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_ip"}, ".0.0.1.1"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_ip"}, "..0."}, // 格式非法
        ParamType{"del", {"http_config", "predict_ip"}, 123}, // 删除predict_ip

        ParamType{"mod", {"http_config", "predict_port"}, 123}, // 类型错误
        ParamType{"mod", {"http_config", "predict_port"}, "-1"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_port"}, "1023"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_port"}, "655385"}, // 格式非法
        ParamType{"mod", {"http_config", "predict_port"}, "0.3"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_port"}, "abc"}, // 格式非法
        ParamType{"del", {"http_config", "predict_port"}, "-1"}, // 删除predict_port

        ParamType{"mod", {"http_config", "manage_ip"}, 123}, // 类型错误
        ParamType{"mod", {"http_config", "manage_ip"}, "123"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_ip"}, "-1.0.0.0"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_ip"}, "0.0.0.0"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_ip"}, "256.0.5.5"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_ip"}, "127.0.0.1."}, // 格式非法
        ParamType{"mod", {"http_config", "manage_ip"}, "127.0.0.x"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_ip"}, "asd"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_ip"}, ".0.0.1.1"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_ip"}, "..0."}, // 格式非法
        ParamType{"del", {"http_config", "manage_ip"}, 123}, // 删除manage_ip

        ParamType{"mod", {"http_config", "manage_port"}, 123}, // 类型错误
        ParamType{"mod", {"http_config", "manage_port"}, "-1"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_port"}, "0.3"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_port"}, "1023"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_port"}, "656538"}, // 格式非法
        ParamType{"mod", {"http_config", "manage_port"}, "abc"}, // 格式非法
        ParamType{"del", {"http_config", "manage_port"}, "-1"}, // 删除manage_port

        ParamType{"mod", {"http_config", "server_thread_num"}, "123"}, // 类型错误
        ParamType{"mod", {"http_config", "server_thread_num"}, 0}, // 超出范围
        ParamType{"mod", {"http_config", "server_thread_num"}, 100005}, // 超出范围
        ParamType{"del", {"http_config", "server_thread_num"}, 123}, // 删除server_thread_num

        ParamType{"mod", {"http_config", "client_thread_num"}, "123"}, // 类型错误
        ParamType{"mod", {"http_config", "client_thread_num"}, 0}, // 超出范围
        ParamType{"mod", {"http_config", "client_thread_num"}, 100005}, // 超出范围
        ParamType{"del", {"http_config", "client_thread_num"}, 123}, // 删除client_thread_num

        ParamType{"mod", {"http_config", "http_timeout_seconds"}, "123"}, // 类型错误
        ParamType{"mod", {"http_config", "http_timeout_seconds"}, -1}, // 超出范围
        ParamType{"mod", {"http_config", "http_timeout_seconds"}, 3601}, // 超出范围
        ParamType{"del", {"http_config", "http_timeout_seconds"}, 123}, // 删除http_timeout_seconds

        ParamType{"mod", {"http_config", "keep_alive_seconds"}, "123"}, // 类型错误
        ParamType{"mod", {"http_config", "keep_alive_seconds"}, -1}, // 超出范围
        ParamType{"mod", {"http_config", "keep_alive_seconds"}, 3601}, // 超出范围
        ParamType{"del", {"http_config", "keep_alive_seconds"}, 123}, // 删除keep_alive_seconds

        ParamType{"mod", {"http_config", "server_name"}, 123}, // 类型错误
        ParamType{"del", {"http_config", "server_name"}, "-1"}, // 删除server_name

        ParamType{"mod", {"http_config", "user_agent"}, 123}, // 类型错误
        ParamType{"del", {"http_config", "user_agent"}, "-1"}, // 删除user_agent

        // metrics_config
        ParamType{"mod", {"metrics_config"}, 123}, // 类型错误
        ParamType{"del", {"metrics_config"}, 123}, // 删除metrics_config

        ParamType{"mod", {"metrics_config", "enable"}, 123}, // 类型错误
        ParamType{"del", {"metrics_config", "enable"}, 123}, // 删除enable

        // exception_config
        ParamType{"mod", {"exception_config"}, 123}, // 类型错误
        ParamType{"del", {"exception_config"}, 123}, // 删除exception_config

        ParamType{"mod", {"exception_config", "max_retry"}, "123"}, // 类型错误
        ParamType{"mod", {"exception_config", "max_retry"}, -1}, // 超出范围
        ParamType{"mod", {"exception_config", "max_retry"}, 11}, // 超出范围
        ParamType{"del", {"exception_config", "max_retry"}, 123}, // 删除max_retry

        ParamType{"mod", {"exception_config", "schedule_timeout"}, "123"}, // 类型错误
        ParamType{"mod", {"exception_config", "schedule_timeout"}, -1}, // 超出范围
        ParamType{"mod", {"exception_config", "schedule_timeout"}, 3601}, // 超出范围
        ParamType{"del", {"exception_config", "schedule_timeout"}, 123}, // 删除schedule_timeout

        ParamType{"mod", {"exception_config", "first_token_timeout"}, "123"}, // 类型错误
        ParamType{"mod", {"exception_config", "first_token_timeout"}, -1}, // 超出范围
        ParamType{"mod", {"exception_config", "first_token_timeout"}, 3601}, // 超出范围
        ParamType{"del", {"exception_config", "first_token_timeout"}, 123}, // 删除first_token_timeout

        ParamType{"mod", {"exception_config", "infer_timeout"}, "123"}, // 类型错误
        ParamType{"mod", {"exception_config", "infer_timeout"}, -1}, // 超出范围
        ParamType{"mod", {"exception_config", "infer_timeout"}, 65536}, // 超出范围
        ParamType{"del", {"exception_config", "infer_timeout"}, 123}, // 删除infer_timeout

        ParamType{"mod", {"exception_config", "tokenizer_timeout"}, "123"}, // 类型错误
        ParamType{"mod", {"exception_config", "tokenizer_timeout"}, -1}, // 超出范围
        ParamType{"mod", {"exception_config", "tokenizer_timeout"}, 3601}, // 超出范围
        ParamType{"del", {"exception_config", "tokenizer_timeout"}, 123}, // 删除tokenizer_timeout

        // request_limit
        ParamType{"mod", {"request_limit"}, 123}, // 类型错误
        ParamType{"del", {"request_limit"}, 123}, // 删除request_limit

        ParamType{"mod", {"request_limit", "single_node_max_requests"}, "123"}, // 类型错误
        ParamType{"mod", {"request_limit", "single_node_max_requests"}, 0}, // 超出范围
        ParamType{"mod", {"request_limit", "single_node_max_requests"}, 15001}, // 超出范围
        ParamType{"del", {"request_limit", "single_node_max_requests"}, 123}, // 删除single_node_max_requests

        ParamType{"mod", {"request_limit", "max_requests"}, "123"}, // 类型错误
        ParamType{"mod", {"request_limit", "max_requests"}, 0}, // 超出范围
        ParamType{"mod", {"request_limit", "max_requests"}, 900001}, // 超出范围
        ParamType{"del", {"request_limit", "max_requests"}, 123}, // 删除max_requests

        // tls_config
        // 文件路径仅校验类型，对相应测试进行了注释
        ParamType{"mod", {"tls_config"}, 123}, // 类型错误
        ParamType{"del", {"tls_config"}, 123}, // 删除tls_config

        ParamType{"mod", {"tls_config", "controller_server_tls_enable"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_enable"}, 123}, // 删除controller_server_tls_enable

        ParamType{"mod", {"tls_config", "controller_server_tls_items"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_items"}, 123}, // 删除controller_server_tls_items

        ParamType{"mod", {"tls_config", "controller_server_tls_items", "ca_cert"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_items", "ca_cert"}, 123}, // 删除ca_cert

        ParamType{"mod", {"tls_config", "controller_server_tls_items", "tls_cert"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_items", "tls_cert"}, 123}, // 删除tls_cert

        ParamType{"mod", {"tls_config", "controller_server_tls_items", "tls_key"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_items", "tls_key"}, 123}, // 删除tls_key

        ParamType{"mod", {"tls_config", "controller_server_tls_items", "tls_passwd"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_items", "tls_passwd"}, 123}, // 删除tls_passwd

        ParamType{"mod", {"tls_config", "controller_server_tls_items", "kmcKsfMaster"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_items", "kmcKsfMaster"}, 123}, // 删除kmcKsfMaster

        ParamType{"mod", {"tls_config", "controller_server_tls_items", "kmcKsfStandby"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_items", "kmcKsfStandby"}, 123}, // 删除kmcKsfMaster

        ParamType{"mod", {"tls_config", "controller_server_tls_items", "tls_crl"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "controller_server_tls_items", "tls_crl"}, 123}, // 删除kmcKsfMaster

        ParamType{"mod", {"tls_config", "request_server_tls_enable"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_enable"}, 123}, // 删除controller_server_tls_enable

        ParamType{"mod", {"tls_config", "request_server_tls_items"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_items"}, 123}, // 删除request_server_tls_items

        ParamType{"mod", {"tls_config", "request_server_tls_items", "ca_cert"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_items", "ca_cert"}, 123}, // 删除ca_cert

        ParamType{"mod", {"tls_config", "request_server_tls_items", "tls_cert"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_items", "tls_cert"}, 123}, // 删除tls_cert

        ParamType{"mod", {"tls_config", "request_server_tls_items", "tls_key"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_items", "tls_key"}, 123}, // 删除tls_key

        ParamType{"mod", {"tls_config", "request_server_tls_items", "tls_passwd"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_items", "tls_passwd"}, 123}, // 删除tls_passwd

        ParamType{"mod", {"tls_config", "request_server_tls_items", "kmcKsfMaster"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_items", "kmcKsfMaster"}, 123}, // 删除kmcKsfMaster

        ParamType{"mod", {"tls_config", "request_server_tls_items", "kmcKsfStandby"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_items", "kmcKsfStandby"}, 123}, // 删除kmcKsfMaster

        ParamType{"mod", {"tls_config", "request_server_tls_items", "tls_crl"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "request_server_tls_items", "tls_crl"}, 123}, // 删除kmcKsfMaster

        ParamType{"mod", {"tls_config", "mindie_client_tls_enable"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_enable"}, 123}, // 删除controller_server_tls_enable

        ParamType{"mod", {"tls_config", "mindie_client_tls_items"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_items"}, 123}, // 删除mindie_client_tls_items

        ParamType{"mod", {"tls_config", "mindie_client_tls_items", "ca_cert"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_items", "ca_cert"}, 123}, // 删除ca_cert

        ParamType{"mod", {"tls_config", "mindie_client_tls_items", "tls_cert"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_items", "tls_cert"}, 123}, // 删除tls_cert

        ParamType{"mod", {"tls_config", "mindie_client_tls_items", "tls_key"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_items", "tls_key"}, 123}, // 删除tls_key

        ParamType{"mod", {"tls_config", "mindie_client_tls_items", "tls_passwd"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_items", "tls_passwd"}, 123}, // 删除tls_passwd

        ParamType{"mod", {"tls_config", "mindie_client_tls_items", "kmcKsfMaster"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_items", "kmcKsfMaster"}, 123}, // 删除kmcKsfMaster

        ParamType{"mod", {"tls_config", "mindie_client_tls_items", "kmcKsfStandby"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_items", "kmcKsfStandby"}, 123}, // 删除kmcKsfMaster

        ParamType{"mod", {"tls_config", "mindie_client_tls_items", "tls_crl"}, 123}, // 类型错误
        ParamType{"del", {"tls_config", "mindie_client_tls_items", "tls_crl"}, 123}, // 删除kmcKsfMaster

        // digs_scheduler_config
        ParamType{"mod", {"digs_scheduler_config"}, 123}, // 类型错误
        ParamType{"del", {"digs_scheduler_config"}, 123}, // 删除http_config

        ParamType{"mod", {"digs_scheduler_config", "deploy_mode"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "deploy_mode"}, "123"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "deploy_mode"}, 123}, // 删除deploy_mode

        ParamType{"mod", {"digs_scheduler_config", "scheduler_type"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "scheduler_type"}, "123"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "scheduler_type"}, 123}, // 删除scheduler_type

        ParamType{"mod", {"digs_scheduler_config", "algorithm_type"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "algorithm_type"}, "123"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "algorithm_type"}, 123} // 删除algorithm_type
    )
);

// cache_affinity的参数
INSTANTIATE_TEST_SUITE_P(
    TestCoordinatorConfigCAParams,
    TestCoordinatorConfigCAParam,
    ::testing::Values(
        ParamType{"mod", {"digs_scheduler_config", "cache_size"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "cache_size"}, "-1"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "cache_size"}, 123}, // 删除cache_size

        ParamType{"mod", {"digs_scheduler_config", "slots_thresh"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "slots_thresh"}, "-1"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "slots_thresh"}, 123}, // 删除slots_thresh

        ParamType{"mod", {"digs_scheduler_config", "block_thresh"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "block_thresh"}, "-1"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "block_thresh"}, 123} // 删除block_thresh
        )
);

// load_balance的参数
INSTANTIATE_TEST_SUITE_P(
    TestCoordinatorConfigLBParams,
    TestCoordinatorConfigLBParam,
    ::testing::Values(
        ParamType{"mod", {"digs_scheduler_config", "max_schedule_count"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "max_schedule_count"}, "-1"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "max_schedule_count"}, "0"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "max_schedule_count"}, "1.5"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "max_schedule_count"}, 123}, // 删除max_schedule_count

        ParamType{"mod", {"digs_scheduler_config", "reordering_type"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "reordering_type"}, "-1"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "reordering_type"}, "1.5"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "reordering_type"}, "abc"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "reordering_type"}, 123}, // 删除reordering_type

        ParamType{"mod", {"digs_scheduler_config", "max_res_num"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "max_res_num"}, "-1"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "max_res_num"}, "1.5"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "max_res_num"}, "abc"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "max_res_num"}, 123}, // 删除max_res_num

        ParamType{"mod", {"digs_scheduler_config", "res_limit_rate"}, 123}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "res_limit_rate"}, "-1"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "res_limit_rate"}, "abc"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "res_limit_rate"}, 123}, // 删除res_limit_rate

        ParamType{"mod", {"digs_scheduler_config", "select_type"}, 1}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "select_type"}, "3"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "select_type"}, "3.5"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "select_type"}, "abc"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "select_type"}, 123}, // 删除select_type

        ParamType{"mod", {"digs_scheduler_config", "load_cost_values"}, 1}, // 类型错误
        ParamType{"mod", {"digs_scheduler_config", "load_cost_values"}, "1,1"}, // 输入不对
        ParamType{"mod", {"digs_scheduler_config", "load_cost_values"}, "abc"}, // 输入不对
        ParamType{"del", {"digs_scheduler_config", "load_cost_values"}, 123}, // 删除metaResource_values

        ParamType{"mod", {"digs_scheduler_config", "load_cost_coefficient"}, 1}, // 类型错误
        ParamType{"del", {"digs_scheduler_config", "load_cost_coefficient"}, "abc"} // 删除metaResource_weights
        )
);