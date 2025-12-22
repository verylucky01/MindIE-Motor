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
#include <string>
#include <memory>
#include <pthread.h>
#include "gtest/gtest.h"
#include "KubeResourceDef.h"
#include "ConfigParams.h"

using namespace MINDIE::MS;

class TestKubeResource : public testing::Test {
protected:
    void SetUp() override
    {
        podSpec = nlohmann::json::object();
        containers = nlohmann::json::object();
        serverParams.mountMap["/path1"] = std::make_tuple("/host/path1", "DirectoryOrCreate", true);
        serverParams.mountMap["/path2"] = std::make_tuple("/host/path2", "DirectoryOrCreate", false);
    }

    nlohmann::json podSpec;
    nlohmann::json containers;
    LoadServiceParams serverParams;
    std::map<std::string, std::tuple<std::string, std::string, bool>> mountMap;
};

TEST_F(TestKubeResource, UTSetMountPath)
{
    LoadServiceParams serverParams;
    int32_t result = SetMountPath(serverParams);
    EXPECT_EQ(result, 0);
    EXPECT_EQ(serverParams.mountMap.size(), 4);
    EXPECT_EQ(serverParams.mountMap["/mnt/mindie-service/ms/model"],
        std::make_tuple("/mnt/mindie-service/ms/model", "Directory", true));
    EXPECT_EQ(serverParams.mountMap["/mnt/mindie-service/ms/writable-data"],
        std::make_tuple("/mnt/mindie-service/ms/writable-data", "Directory", false));
    EXPECT_EQ(serverParams.mountMap["/mnt/mindie-service/ms/run/run.sh"],
        std::make_tuple("/mnt/mindie-service/ms/run/run.sh", "File", true));
    EXPECT_EQ(serverParams.mountMap["/mnt/mindie-service/ms/config/config.json"],
        std::make_tuple("/mnt/mindie-service/ms/config/config.json", "File", true));
}


/*
测试描述: 设置主机路径测试用例
测试步骤:
    1. 配置mountMap为空，预期结果1
预期结果:
    1. podSpec和containers中mount为空
*/
TEST_F(TestKubeResource, UTSetHostPathVolumnsEmpty)
{
    LoadServiceParams serverParams;
    SetUp();
    serverParams.mountMap.clear();
    SetHostPathVolumns(podSpec, containers, serverParams);
    EXPECT_TRUE(podSpec["volumes"].empty());
    EXPECT_TRUE(containers["volumeMounts"].empty());
}

/*
测试描述: 设置主机路径测试用例
测试步骤:
    1. 配置serverParams，预期结果1
预期结果:
    1. 读取结果和配置的一致
*/
TEST_F(TestKubeResource, UTSetHostPathVolumns)
{
    SetUp();
    SetHostPathVolumns(podSpec, containers, serverParams);
    EXPECT_EQ(podSpec["volumes"].size(), 2);
    EXPECT_EQ(containers["volumeMounts"].size(), 2);
    EXPECT_EQ(containers["volumeMounts"][0]["readOnly"], true);
    EXPECT_EQ(containers["volumeMounts"][1]["readOnly"], false);
}
