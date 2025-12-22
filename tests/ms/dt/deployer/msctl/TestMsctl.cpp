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
#include <nlohmann/json.hpp>
#include <cstdio>
#include <string>
#include <memory>
#include <map>
#include <random>
#include <unistd.h>
#include <variant>
#include <vector>
#include <pthread.h>
#include "gtest/gtest.h"
#define main __main_ctl__
#define PrintResponse __print_response_ctl__
#include "deployer/msctl/main.cpp"
#include "Helper.h"
#include "JsonFileManager.h"
using namespace MINDIE::MS;

// 修改Json文件的值Param，分别是mod/del、key、value
using ParamType = std::tuple<std::string, std::vector<std::string>, std::variant<int, float, std::string>>;

using CommandLine = std::vector<std::string>;
using CommandExp = std::tuple<CommandLine, int>;

static auto inferJson = GetMSDeployTestJsonPath();
static auto clientJson = GetMSClientTestJsonPath();

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

static int CallMain(std::vector<std::string> command)
{
    // Ensure there's a dummy argv[0]
    command.insert(command.begin(), "dummy_program");

    // Set argc to the size of the command vector
    auto argc = command.size();
    
    // Convert std::vector<std::string> to char* array
    std::vector<const char*> argv(argc);
    std::transform(command.begin(), command.end(), argv.begin(), [](const std::string& str) {
        return str.c_str();
    });

    // Call the main function
    auto ret = __main_ctl__(argc, const_cast<char**>(argv.data()));
    return ret;
}

// 最普通的Fixture
class TestMsctl : public ::testing::Test {
protected:
    void SetUp() override
    {
        // 复制临时文件
        CopyFile(GetMSClientConfigJsonPath(), clientJson);
        CopyFile(GetMSDeployConfigJsonPath(), inferJson);
        auto testHome = GetMSTestHomePath();
        setenv("HOME", testHome.c_str(), 1);
        std::cout << "HOME=" << testHome << std::endl;
        auto hseceasyPath = GetHSECEASYPATH();
        setenv("HSECEASY_PATH", hseceasyPath.c_str(), 1);
        std::cout << "HSECEASY_PATH=" << hseceasyPath << std::endl;
        
        // 初始化配置
        SetMSClientJsonDefault(clientJson);
    }
    void TearDown() override
    {
        unsetenv("HOME");
    }
};

class TestMsctlCommand : public TestMsctl,
    public ::testing::WithParamInterface<CommandExp> {};

class TestMsctlJson : public TestMsctl,
    public ::testing::WithParamInterface<ParamType> {};

class TestMsctlInferSererJson : public TestMsctl,
    public ::testing::WithParamInterface<ParamType> {};

/*
测试描述: Msctl删除HOME环境变量，得到预期结果
测试步骤:
    1. 传入指令，预期结果1
预期结果:
    1. 返回值为-1
*/
TEST_F(TestMsctl, DeleteHomeEnvTC)
{
    unsetenv("HOME");

    auto command = CommandLine{"update", "infer_server", "-f", inferJson};

    auto ret = CallMain(command);
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Msctl变更HOME环境变量，得到预期结果
测试步骤:
    1. 传入指令，预期结果1
预期结果:
    1. 返回值为-1
*/
TEST_F(TestMsctl, ModHomeEnvTC)
{
    setenv("HOME", "path/not/found", 1);

    auto command = CommandLine{"update", "infer_server", "-f", inferJson};

    auto ret = CallMain(command);
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Msctl删除msctl.json的数据，得到预期结果
测试步骤:
    1. 传入指令，预期结果1
预期结果:
    1. 返回值为-1
*/
TEST_F(TestMsctl, DeleteMsctlJsonDataTC)
{
    nlohmann::json emptyJson = nlohmann::json::object();
    std::ofstream file(clientJson);
    file << "abc";
    file.close();

    auto command = CommandLine{"update", "infer_server", "-f", inferJson};

    auto ret = CallMain(command);
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Msctl删除infer_server.json的数据，得到预期结果
测试步骤:
    1. 传入指令，预期结果1
预期结果:
    1. 返回值为-1
*/
TEST_F(TestMsctl, DeleteInferJsonDataTC)
{
    nlohmann::json emptyJson = nlohmann::json::object();
    std::ofstream file(inferJson);
    file << "abc";
    file.close();

    auto command = CommandLine{"update", "infer_server", "-f", inferJson};

    auto ret = CallMain(command);
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Msctl更改msctl.json，得到预期结果
测试步骤:
    1. 传入指令，预期结果1
预期结果:
    1. 返回值为-1
*/
TEST_P(TestMsctlJson, MsctlJsonTC)
{
    auto param = GetParam();
    ModifyJsonFile(clientJson, param);

    auto command = CommandLine{"update", "infer_server", "-f", inferJson};

    auto ret = CallMain(command);
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Msctl更改infer_server.json，得到预期结果
测试步骤:
    1. 传入指令，预期结果1
预期结果:
    1. 返回值为-1
*/
TEST_P(TestMsctlInferSererJson, InferSererJsonTC)
{
    auto param = GetParam();
    ModifyJsonFile(inferJson, param);

    auto command = CommandLine{"update", "infer_server", "-f", inferJson};

    auto ret = CallMain(command);
    EXPECT_EQ(ret, -1);
}

/*
测试描述: 传输指令，得到预期结果
测试步骤:
    1. 传入Json，预期结果1
预期结果:
    1. 返回值与参数当中的相同
*/
TEST_P(TestMsctlCommand, CommandLineTC)
{
    auto commandExp = GetParam();

    auto command = std::get<0>(commandExp);
    auto exp = std::get<1>(commandExp);
    auto ret = CallMain(command);

    EXPECT_EQ(ret, exp);
}

INSTANTIATE_TEST_SUITE_P(
    TestMsctlCommands,
    TestMsctlCommand,
    ::testing::Values(
        // 打印help
        CommandExp{{"-h"}, 0},
        CommandExp{CommandLine{}, -1},

        // 正常json，返回值0
        CommandExp{{"create", "infer_server", "-f", inferJson}, -1},
        CommandExp{{"delete", "infer_server", "-n", "dummy_name"}, -1},
        CommandExp{{"get", "infer_server", "-n", "dummy_name"}, -1},
        CommandExp{{"update", "infer_server", "-f", inferJson}, -1},

        // 异常文件路径，返回值-1
        CommandExp{{"create", "infer_server", "-f", "path/not/exsist"}, -1},
        CommandExp{{"update", "infer_server", "-f", "path/not/exsist"}, -1}
    )
);

INSTANTIATE_TEST_SUITE_P(
    TestMsctlJsons,
    TestMsctlJson,
    ::testing::Values(
        ParamType{"mod", {"http"}, "123"},
        ParamType{"del", {"http"}, 123},

        ParamType{"mod", {"http", "dstIP"}, 123},
        ParamType{"mod", {"http", "dstIP"}, "123"},
        ParamType{"del", {"http", "dstIP"}, 123},

        ParamType{"mod", {"http", "dstPort"}, 123},
        ParamType{"del", {"http", "dstPort"}, 123},

        ParamType{"mod", {"http", "timeout"}, 123},
        ParamType{"del", {"http", "timeout"}, 123},

        ParamType{"mod", {"http", "ca_cert"}, 123},
        ParamType{"del", {"http", "ca_cert"}, 123},

        ParamType{"mod", {"http", "tls_cert"}, 123},
        ParamType{"del", {"http", "tls_cert"}, 123},

        ParamType{"mod", {"http", "tls_key"}, 123},
        ParamType{"del", {"http", "tls_key"}, 123},

        ParamType{"mod", {"http", "tls_crl"}, 123},
        ParamType{"del", {"http", "tls_crl"}, 123}
    )
);

INSTANTIATE_TEST_SUITE_P(
    TestMsctlInferSererJsons,
    TestMsctlInferSererJson,
    ::testing::Values(
        ParamType{"mod", {"server_name"}, 123},
        ParamType{"del", {"server_name"}, 123}
    )
);