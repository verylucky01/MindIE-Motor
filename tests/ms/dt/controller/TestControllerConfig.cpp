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
#include <cmath>
#include <fstream>
#include <cstdlib> // #include <stdlib.h>
#include <string>
#include <memory>
#include <pthread.h>
#include <nlohmann/json.hpp>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include "gtest/gtest.h"
#include "ControllerConfig.h"
#include "Helper.h"
#include "JsonFileManager.h"
#include "Controller.h"
using namespace MINDIE::MS;


class TestControllerConfig : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }

    void SetUp()
    {
        CopyDefaultConfig();
        std::string controllerJson = GetMSControllerTestJsonPath();
        JsonFileManager manager(controllerJson);
        manager.Load();
        manager.SetList({"http_server", "port"}, 2028);
        manager.SetList({"tls_config", "request_coordinator_tls_enable"}, false);
        manager.SetList({"tls_config", "request_server_tls_enable"}, false);
        manager.SetList({"tls_config", "http_server_tls_enable"}, false);
        manager.SetList({"tls_config", "cluster_tls_enable"}, false);
        manager.Save();
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", controllerJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << controllerJson << std::endl;
        ChangeFileMode(controllerJson);
    }
};


int EraseJsonItem(std::string key1, std::string key2)
{
    std::string jsonPath = GetMSControllerTestJsonPath();

    std::string envParam = "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH";
    auto env = std::getenv(envParam.c_str());
    if (jsonPath != env) {
        std::cout << "环境变量的json路径设置不正确！\n现在是：" << env << std::endl;
        return -1;
    }

    std::ifstream ifs(jsonPath);
    nlohmann::json jsonData;
    ifs >> jsonData;
    if (jsonData.contains(key1)) {
        if (key2 != "") {
            if (jsonData[key1].contains(key2)) {
                jsonData[key1].erase(key2);
            } else {
                std::cout << "error: key2 not exit !" << std::endl;
                return -1;
            }
        } else {
            jsonData.erase(key1);
        }
    } else {
        std::cout << "error: key1 not exit !" << std::endl;
    }

// 将修改后的JSON数据写回文件
    std::ofstream ofs(jsonPath);
    ofs << jsonData.dump(2);
    std::cout << "Erase success !" << std::endl;
    return 0;
}

template <typename T>
int ChangeJsonItem(std::string key1, std::string key2, std::string key3, T value)
{
    std::string jsonPath = GetMSControllerTestJsonPath();

    std::string envParam = "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH";
    auto env = std::getenv(envParam.c_str());
    if (jsonPath != env) {
        std::cout << "环境变量的json路径设置不正确！\n现在是：" << env << std::endl;
        return -1;
    }

    std::ifstream ifs(jsonPath);
    nlohmann::json jsonData;
    ifs >> jsonData;
    if (!jsonData.contains(key1)) {
        std::cout << "error: key1 not exit !" << std::endl;
    } else {
        if (key2 == "") {
            jsonData[key1] = value;
        } else {
            if (!jsonData[key1].contains(key2)) {
                std::cout << "error: key2 not exit !" << std::endl;
            } else {
                if (key3 == "") {
                    jsonData[key1][key2] = value;
                } else {
                    if (!jsonData[key1][key2].contains(key3)) {
                        std::cout << "error: key3 not exit !" << std::endl;
                    } else {
                        jsonData[key1][key2][key3] = value;
                    }
                }
            }
        }
    }

// 将修改后的JSON数据写回文件
    std::ofstream ofs(jsonPath);
    ofs << jsonData.dump(2);
    std::cout << "change success !" << std::endl;
    return 0;
}


/*
测试描述: 服务端配置文件测试，测试文件权限合法值与非法值
测试步骤:
    1. 默认ms_controller.json文件的权限为0640，预期结果1
    2. 设置ms_controller.json文件的权限为0660，预期结果2
    3. 恢复为0640权限
预期结果:
    1. 启动成功
    2. 启动失败
*/
TEST_F(TestControllerConfig, TestInitTC01)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);
    std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << tempControllerJosn << std::endl;
    
    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);
    // 修改文件权限，启动
    mode_t newPermissions = 0660;
    if (chmod(tempControllerJosn.c_str(), newPermissions) == -1) {
        std::cerr << "Failed to change file permissions: " << std::strerror(errno) << std::endl;
    } else {
        std::cout << "File permissions changed successfully" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    // 恢复为0640权限
    newPermissions = 0640;
    if (chmod(tempControllerJosn.c_str(), newPermissions) == -1) {
        std::cerr << "Failed to change file permissions: " << std::strerror(errno) << std::endl;
    } else {
        std::cout << "File permissions changed successfully" << std::endl;
    }
}

/*
测试描述: 服务端配置文件测试，测试server.ip合法值与非法值
测试步骤:
    1. 配置controllerTestJsonPath为正常文件，内容有效，预期结果1
    2. 修改controllerTestJsonPath的IP为'12.34.56.78'，预期结果1
    3. 修改controllerTestJsonPath的IP为'.7212.123.163'，预期结果2
    4. 修改controllerTestJsonPath的IP为'123456'，预期结果2
    5. 修改controllerTestJsonPath的IP为'123456'*2**10，预期结果2
    6. 修改controllerTestJsonPath的IP为123456，预期结果2
    7. 修改controllerTestJsonPath的IP为'333.333.333.333'，预期结果2
    8. 修改controllerTestJsonPath的IP为'0.0.0.0'，预期结果2
    9. 修改controllerTestJsonPath的IP为''，预期结果2
    10. 修改controllerTestJsonPath的IP为字段不存在，预期结果2
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 启动失败
*/
TEST_F(TestControllerConfig, TestInitTC02)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);
    std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << tempControllerJosn << std::endl;

    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);
    auto ipConfig = ControllerConfig::GetInstance()->GetPodIP();
    EXPECT_EQ(ipConfig, "127.0.0.1");
    // 2
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "ip", "", "12.34.56.78") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(ControllerConfig::GetInstance()->GetPodIP(), "12.34.56.78");
    // 3
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "ip", "", ".7212.123.163") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    // 4
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "ip", "", "123456") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    // 5
    {
        // 修改json文件
        std::string tempStr = "123456";
        for (int i = 0; i < std::pow(2, 10); ++i) {
            tempStr += "123456";
        }
        if (ChangeJsonItem("http_server", "ip", "", tempStr) != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    // 6
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "ip", "", 123456) != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    // 7
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "ip", "", "333.333.333.333") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    // 8
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "ip", "", "0.0.0.0") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    // 9
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "ip", "", "") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    // 10
    {
        // 修改json文件
        if (EraseJsonItem("http_server", "ip") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
}


/*
测试描述: 服务端配置文件测试，测试server.port合法值与非法值
测试步骤:
    1. 配置controllerTestJsonPath为正常文件，内容有效，预期结果1
    2. 修改controllerTestJsonPath的port为'12.34.56.78'，预期结果2
    3. 修改controllerTestJsonPath的port为'123456'，预期结果2
    4. 修改controllerTestJsonPath的port为'123456'*2**10，预期结果2
    5. 修改controllerTestJsonPath的port为65535，预期结果1
    6. 修改controllerTestJsonPath的port为-123456，预期结果2
    7. 修改controllerTestJsonPath的port为1023，预期结果2
    8. 修改controllerTestJsonPath的port为''，预期结果2
    9. 修改controllerTestJsonPath的port为字段不存在，预期结果2
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 启动失败
*/
TEST_F(TestControllerConfig, TestInitTC03)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    JsonFileManager manager(tempControllerJosn);
    manager.Load();
    manager.SetList({"http_server", "port"}, 1026);
    manager.SetList({"tls_config", "request_coordinator_tls_enable"}, false);
    manager.SetList({"tls_config", "request_server_tls_enable"}, false);
    manager.SetList({"tls_config", "http_server_tls_enable"}, false);
    manager.SetList({"tls_config", "cluster_tls_enable"}, false);
    manager.Save();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);
    std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << tempControllerJosn << std::endl;

    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);
    auto portConfig = ControllerConfig::GetInstance()->GetPort();
    EXPECT_EQ(portConfig, 1026);
    std::cout << "2: " << std::endl;
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "port", "", "12.34.56.78") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    std::cout << "3: " << std::endl;
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "port", "", "123456") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    std::cout << "4: " << std::endl;
    {
        // 修改json文件
        std::string tempStr = "123456";
        for (int i = 0; i < std::pow(2, 10); ++i) {
            tempStr += "123456";
        }
        if (ChangeJsonItem("http_server", "port", "", tempStr) != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    std::cout << "5: " << std::endl;
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "port", "", 65535) != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);
    portConfig = ControllerConfig::GetInstance()->GetPort();
    EXPECT_EQ(portConfig, 65535);
    std::cout << "6: " << std::endl;
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "port", "", -123456) != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    std::cout << "7: " << std::endl;
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "port", "", 1023) != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    std::cout << "8: " << std::endl;
    {
        // 修改json文件
        if (ChangeJsonItem("http_server", "port", "", "") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
    std::cout << "9: " << std::endl;
    {
        // 修改json文件
        if (EraseJsonItem("http_server", "port") != 0) {
            std::cout << "修改json文件失败！" << std::endl;
        }
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
}


/*
测试描述: 服务端配置文件测试，测试证书相关
测试步骤:
    1. 默认json中所有tls_enable = false，预期结果1
    2. 修改coordinator的tls_enable = true，运行，预期结果1
    3. 修改coordinator的tls_cert为不存在路径，预期结果2
    4. 修改coordinator的tls_cert为无效路径，预期结果2
    5. 修改coordinator的tls_cert为ca_cert的路径，预期结果2
预期结果:
    1. 解析配置成功
    2. 启动client失败
*/
TEST_F(TestControllerConfig, TestInitTC04)
{
    HttpClient httpClient;
    std::string ip = "127.0.0.1";
    std::string portStr = "1026";
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    JsonFileManager manager(tempControllerJosn);
    manager.Load();
    manager.SetList({"tls_config", "request_coordinator_tls_enable"}, false);
    manager.SetList({"tls_config", "request_server_tls_enable"}, false);
    manager.SetList({"tls_config", "http_server_tls_enable"}, false);
    manager.SetList({"tls_config", "cluster_tls_enable"}, false);
    manager.Save();

    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);
    std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << tempControllerJosn << std::endl;

    ControllerConfig::GetInstance()->Init();
    int32_t ret = httpClient.Init(ip, portStr, ControllerConfig::GetInstance()->GetRequestCoordinatorTlsItems(), true);
    EXPECT_EQ(ret, 0);
    // 2
    std::cout << "2: " << std::endl;
    if (ChangeJsonItem("tls_config", "request_coordinator_tls_enable", "", true) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    // 3
    std::cout << "3: " << std::endl;
    if (ChangeJsonItem("tls_config", "request_coordinator_tls_items", "tls_cert", "./abcd/aaaaa/cert.key.pem") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ControllerConfig::GetInstance()->Init();
    ret = httpClient.Init(ip, portStr, ControllerConfig::GetInstance()->GetRequestCoordinatorTlsItems(), true);
    EXPECT_NE(ret, 0);
    // 4
    std::cout << "4: " << std::endl;
    if (ChangeJsonItem("tls_config", "request_coordinator_tls_items", "tls_cert",
        "111abcasdfasdfsd163515..asdf") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ControllerConfig::GetInstance()->Init();
    ret = httpClient.Init(ip, portStr, ControllerConfig::GetInstance()->GetRequestCoordinatorTlsItems(), true);
    EXPECT_NE(ret, 0);
    // 5
    std::cout << "5: " << std::endl;
    if (ChangeJsonItem("tls_config", "request_coordinator_tls_items", "tls_cert",
        "./security/msserver/security/ca/ca.pem") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ControllerConfig::GetInstance()->Init();
    ret = httpClient.Init(ip, portStr, ControllerConfig::GetInstance()->GetRequestCoordinatorTlsItems(), true);
    EXPECT_NE(ret, 0);
}

/*
测试描述: 服务端配置文件测试，json文件中部署模式为异常值
测试步骤:
    1. 使用默认json，预期结果1
    2. 修改deploy_mode = "pd_disaggregation"，值非法，预期结果1
    3. 修改deploy_mode = "pd_disaggregation_single_container"，值非法，预期结果1
    4. 修改deploy_mode = "single_node"，值非法，预期结果1
    5. 修改deploy_mode = "invalid deploy_mode"，值非法，预期结果2
预期结果:
    1. 启动成功
    2. 启动失败
*/
TEST_F(TestControllerConfig, TestInitTC05)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);

    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    if (ChangeJsonItem("deploy_mode", "", "", "pd_disaggregation") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    if (ChangeJsonItem("deploy_mode", "", "", "pd_disaggregation_single_container") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    if (ChangeJsonItem("deploy_mode", "", "", "single_node") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    if (ChangeJsonItem("deploy_mode", "", "", "invalid deploy_mode") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
}

/*
测试描述: 服务端配置文件测试，身份决策算法相关
测试步骤:
    1. 使用默认json，预期结果1
    2. 修改role_decision_methods = "invalid role_decision_methods"，值非法，预期结果2
    3. 重置json
    4. 修改default_p_rate = 1且default_d_rate = 1，预期结果1
    5. 修改default_p_rate = 1且default_d_rate = 0，预期结果2
    6. 修改default_p_rate = 16且default_d_rate = 1，预期结果2
    7. 修改default_p_rate = 15且default_d_rate = 1，预期结果1
    8. 修改default_p_rate = 0且default_d_rate = 1，预期结果2
    9. 修改default_p_rate = 1且default_d_rate = 16，预期结果2
    10. 修改default_p_rate = 1且default_d_rate = 15，预期结果1
    11. 修改default_p_rate = 2且default_d_rate = 15，预期结果2
    12. 修改default_p_rate = 15且default_d_rate = 2，预期结果2
    13. 修改transfer_type = "Transfer"，值非法，预期结果2
预期结果:
    1. 启动成功
    2. 启动失败
*/
TEST_F(TestControllerConfig, TestInitTC06)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath(); // 1
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);
    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    std::cout << "2: " << std::endl; // 2
    if (ChangeJsonItem("role_decision_methods", "", "", "invalid role_decision_methods") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, -1);

    std::cout << "3: " << std::endl; // 3
    if (ChangeJsonItem("role_decision_methods", "", "", "digs") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    if (ChangeJsonItem("default_p_rate", "", "", 1) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    if (ChangeJsonItem("default_d_rate", "", "", 1) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    std::cout << "4: " << std::endl; // 4
    if (ChangeJsonItem("default_p_rate", "", "", 1) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    if (ChangeJsonItem("default_d_rate", "", "", 0) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, -1);

    std::cout << "5: " << std::endl; // 5
    if (ChangeJsonItem("default_p_rate", "", "", 1024) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    if (ChangeJsonItem("default_d_rate", "", "", 1) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, -1);

    std::cout << "6: " << std::endl; // 6
    if (ChangeJsonItem("default_p_rate", "", "", 15) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    std::cout << "7: " << std::endl; // 7
    if (ChangeJsonItem("default_p_rate", "", "", 0) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    if (ChangeJsonItem("default_d_rate", "", "", 1) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, -1);

    std::cout << "8: " << std::endl; // 8
    if (ChangeJsonItem("default_p_rate", "", "", 1) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    if (ChangeJsonItem("default_d_rate", "", "", 768) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, -1);

    std::cout << "9: " << std::endl; // 9
    if (ChangeJsonItem("default_d_rate", "", "", 15) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    std::cout << "11: " << std::endl; // 11
    if (ChangeJsonItem("default_p_rate", "", "", 2) != 0) { // P比例为2
        std::cout << "修改json文件失败！" << std::endl;
    }
    if (ChangeJsonItem("default_d_rate", "", "", 767) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, -1);

    std::cout << "12: " << std::endl; // 12
    if (ChangeJsonItem("default_p_rate", "", "", 767) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    if (ChangeJsonItem("default_d_rate", "", "", 2) != 0) { // D比例为2
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, -1);

    std::cout << "10: " << std::endl; // 10
    if (ChangeJsonItem("transfer_type", "", "", "2DTransfer") != 0) {
    std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, -1);
}

/*
测试描述: Controller配置文件测试，测试allow_all_zero_ip_listening字段缺失、与http_server的ip配置冲突两种情况
测试步骤:
    1. 配置controllerTestJsonPath为正常文件，内容有效，预期结果1
    2. 通过环境变量POD_IP修改controllerTestJsonPath的http_server的ip为全零，
        与allow_all_zero_ip_listening的默认配置false冲突，预期结果2
    3. 删除controllerTestJsonPath的allow_all_zero_ip_listening字段，预期结果3
预期结果:
    1. 解析配置成功，解析到的配置与设置的一致
    2. 解析配置成功，但GetPodIp校验POD_IP不通过，http_server的ip回退至controller配置文件中的默认值127.0.0.1
    3. 配置解析失败
*/
TEST_F(TestControllerConfig, TestAllZeroIpListening)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);

    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    setenv("POD_IP", "0.0.0.0", 1);
    std::string controllerIp = ControllerConfig::GetInstance()->GetPodIP();
    EXPECT_EQ(controllerIp, "127.0.0.1");

    if (EraseJsonItem("allow_all_zero_ip_listening", "") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
}

/*
测试描述: Controller配置文件测试，测试json文件不配置multi_node_infer_config
测试步骤:
    1. 配置controllerTestJsonPath为正常文件，内容有效，预期结果1
    2. 删除json中multi_node_infer_config部分
    3. 测试json文件是否能够成功解析
预期结果:
    1. 解析配置成功
    2. 不配置multi_node_infer_config内容，解析配置成功
*/
TEST_F(TestControllerConfig, TestReadingWithoutMultiNodeInferConfig)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);

    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    if (EraseJsonItem("multi_node_infer_config", "") != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);
}

/*
测试描述: Controller配置文件测试，配置multi_node_infer_config内容，测试json读取内容正确
测试步骤:
    1. 配置controllerTestJsonPath为正常文件，内容有效，预期结果1
    2. 修改跨机multi_node_infer_enable配置为true
预期结果:
    1. 解析配置成功，获取配置信息正确
*/
TEST_F(TestControllerConfig, TestReadingMultiNodeInferConfigValue)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);

    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    if (ChangeJsonItem("multi_node_infer_config", "multi_node_infer_enable", "", true) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);

    bool isMultiNode = ControllerConfig::GetInstance()->IsMultiNodeMode();
    EXPECT_TRUE(isMultiNode);
    auto pNodeNum = ControllerConfig::GetInstance()->GetPNodeNum();
    EXPECT_EQ(pNodeNum, 2);
    auto dNodeNum = ControllerConfig::GetInstance()->GetDNodeNum();
    EXPECT_EQ(dNodeNum, 2);
    auto pTpSize = ControllerConfig::GetInstance()->GetPTpSize();
    EXPECT_EQ(pTpSize, 4);
    auto dTpSize = ControllerConfig::GetInstance()->GetDTpSize();
    EXPECT_EQ(dTpSize, 4);
    auto pDpSize = ControllerConfig::GetInstance()->GetPDpSize();
    EXPECT_EQ(pDpSize, 4);
    auto dDpSize = ControllerConfig::GetInstance()->GetDDpSize();
    EXPECT_EQ(dDpSize, 4);
}

/*
测试描述: Controller配置文件测试，配置multi_node_infer_config内容，测试json读取内容正确
测试步骤:
    1. 配置controllerTestJsonPath为正常文件，内容有效，预期结果1
    2. 修改跨机multi_node_infer_enable配置为true
    3. 修改跨机node_machine_num配置为true,不合法
预期结果:
    1. 预期解析配置失败，返回0
*/
TEST_F(TestControllerConfig, TestReadingMultiNodeInferConfigInValid)
{
    std::string tempControllerJosn = GetMSControllerTestJsonPath();
    setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", tempControllerJosn.c_str(), 1);

    int32_t ret = ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(ret, 0);
    if (ChangeJsonItem("multi_node_infer_config", "multi_node_infer_enable", "", true) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }

    if (ChangeJsonItem("multi_node_infer_config", "p_node_config", "node_machine_num", true) != 0) {
        std::cout << "修改json文件失败！" << std::endl;
    }
    ret = ControllerConfig::GetInstance()->Init();
    EXPECT_NE(ret, 0);
}