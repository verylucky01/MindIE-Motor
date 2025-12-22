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
#include <memory>
#include <iostream>
#include "gtest/gtest.h"
#include "ControllerConfig.h"
#include "stub.h"
#include "Helper.h"
#include "CoordinatorBackupStub.h"
#include "CoordinatorStore.h"
#include "CoordinatorBackupHandler.h"

using namespace MINDIE::MS;

class TestCoordinatorBackupHandler : public testing::Test {
public:
    void SetUp() override
    {
        InitHttpClient();
        CopyDefaultConfig();
        auto controllerJson = GetMSControllerTestJsonPath();
        auto testJson = GetProbeServerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "tls_config", "request_server_tls_enable", false);
        ModifyJsonItem(testJson, "tls_config", "request_coordinator_tls_enable", false);
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        std::cout << "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH=" << testJson << std::endl;
        ControllerConfig::GetInstance()->Init();
    }

    void InitHttpClient()
    {
        client = std::make_shared<HttpClient>();
        tlsItems.tlsEnable = false;
        host = "127.0.0.1";
        port = "1026";
        client->Init(host, port, tlsItems, false);
    }

    TlsItems tlsItems;
    std::shared_ptr<HttpClient> client;
    std::string host;
    std::string port;
    std::string CCAEHost = "127.0.0.2";
    std::string CCAEPort = "8080";
    
    std::shared_ptr<CoordinatorStore> coordianatorStore = std::make_shared<CoordinatorStore>();
    std::shared_ptr<CoordinatorStore> coordianatorStoreWithMasterInfo = std::make_shared<CoordinatorStore>();
    std::shared_ptr<CoordinatorBackupHandler> coordinatorBackupHandler =
        std::make_shared<CoordinatorBackupHandler>(coordianatorStore, coordianatorStoreWithMasterInfo);
};

TEST_F(TestCoordinatorBackupHandler, TestInit)
{
    auto ret = coordinatorBackupHandler->Init();

    EXPECT_EQ(ret, 0);
}

TEST_F(TestCoordinatorBackupHandler, TestDealWithCoordinatorBackup)
{
    Stub stub1;
    stub1.set(ADDR(CoordinatorStore, GetCoordinators), &GetCoordinatorsMock01);
    Stub stub2;
    stub2.set(ADDR(HttpClient, SetHostAndPort), &SetHostAndPortMock);
    Stub stub3;
    stub3.set(ADDR(HttpClient, SendRequest), &SendRequestMock01);
    coordinatorBackupHandler->DealWithCoordinatorBackup();

    stub1.set(ADDR(CoordinatorStore, GetCoordinators), &GetCoordinatorsMock02);
    coordinatorBackupHandler->DealWithCoordinatorBackup();
    
    stub1.set(ADDR(CoordinatorStore, GetCoordinators), &GetCoordinatorsMock03);
    coordinatorBackupHandler->DealWithCoordinatorBackup();

    stub3.reset(ADDR(HttpClient, SendRequest));
    stub2.reset(ADDR(HttpClient, SetHostAndPort));
    stub1.reset(ADDR(CoordinatorStore, GetCoordinators));
}

TEST_F(TestCoordinatorBackupHandler, TestSendCoordinatorBackupRequest)
{
    std::unique_ptr<Coordinator> node = std::make_unique<Coordinator>();
    node->ip = "127.0.0.2";
    Stub stub1;
    stub1.set(ADDR(HttpClient, SetHostAndPort), &SetHostAndPortMock);
    std::string body = "";
    std::string response = "";
    Stub stub2;
    stub2.set(ADDR(HttpClient, SendRequest), &SendRequestMock01);
    auto ret = coordinatorBackupHandler->SendCoordinatorBackupRequest(node, boost::beast::http::verb::get,
        CoordinatorURI::GET_RECVS_INFO, body, response);
    EXPECT_EQ(ret, -1);

    stub2.set(ADDR(HttpClient, SendRequest), &SendRequestMock02);
    ret = coordinatorBackupHandler->SendCoordinatorBackupRequest(node, boost::beast::http::verb::get,
        CoordinatorURI::GET_RECVS_INFO, body, response);
    EXPECT_EQ(ret, -1);

    stub2.reset(ADDR(HttpClient, SendRequest));
    stub1.reset(ADDR(HttpClient, SetHostAndPort));
}

TEST_F(TestCoordinatorBackupHandler, TestGetCoordinatorBackupInfo)
{
    std::vector<std::unique_ptr<Coordinator>> coordinatorNodes;
    AddCoordinator(coordinatorNodes, "192.168.1.1", "8080", true);
    AddCoordinator(coordinatorNodes, "192.168.1.2", "8080", false);
    AddCoordinator(coordinatorNodes, "192.168.1.3", "8080", true);
    Stub stub1;
    stub1.set(ADDR(HttpClient, SetHostAndPort), &SetHostAndPortMock);
    Stub stub2;
    stub2.set(ADDR(HttpClient, SendRequest), &SendRequestMock01);
    coordinatorBackupHandler->GetCoordinatorBackupInfo(coordinatorNodes);
    stub2.reset(ADDR(HttpClient, SendRequest));
    stub1.reset(ADDR(HttpClient, SetHostAndPort));
}

TEST_F(TestCoordinatorBackupHandler, TestSetCoordinatorIsMasterBackup)
{
    std::vector<std::unique_ptr<Coordinator>> coordinatorNodes;
    AddCoordinator(coordinatorNodes, "192.168.1.1", "8080", true);
    AddCoordinator(coordinatorNodes, "192.168.1.2", "8080", true);

    coordinatorBackupHandler->SetCoordinatorIsMasterBackup(coordinatorNodes);
    EXPECT_TRUE(coordinatorNodes[0]->isMaster);
    EXPECT_FALSE(coordinatorNodes[1]->isMaster);

    coordinatorBackupHandler->mCoordinatorBackupStatus["192.168.1.1"] = true;
    coordinatorBackupHandler->mCoordinatorBackupStatus["192.168.1.2"] = false;
    coordinatorBackupHandler->SetCoordinatorIsMasterBackup(coordinatorNodes);
    EXPECT_FALSE(coordinatorNodes[0]->isMaster);
    EXPECT_TRUE(coordinatorNodes[1]->isMaster);
}

TEST_F(TestCoordinatorBackupHandler, TestUpdateCoordinatorMasterInfo)
{
    std::vector<std::unique_ptr<Coordinator>> coordinatorNodes;
    AddCoordinator(coordinatorNodes, "192.168.1.1", "8080", true);
    AddCoordinator(coordinatorNodes, "192.168.1.2", "8080", true);
    coordinatorNodes[0]->recvFlow = 0;
    coordinatorNodes[1]->recvFlow = 0;
    coordinatorNodes[0]->isMaster = true;
    coordinatorNodes[1]->isMaster = true;
    coordinatorBackupHandler->UpdateCoordinatorMasterInfo(coordinatorNodes);
    EXPECT_TRUE(coordinatorNodes[0]->isMaster);
    EXPECT_FALSE(coordinatorNodes[1]->isMaster);

    coordinatorNodes[0]->recvFlow = 1;
    coordinatorNodes[1]->recvFlow = 1;
    coordinatorNodes[0]->isMaster = true;
    coordinatorNodes[1]->isMaster = true;
    coordinatorBackupHandler->UpdateCoordinatorMasterInfo(coordinatorNodes);
    EXPECT_FALSE(coordinatorNodes[0]->isMaster);
    EXPECT_TRUE(coordinatorNodes[1]->isMaster);

    coordinatorNodes[0]->recvFlow = 1;
    coordinatorNodes[1]->recvFlow = 0;
    coordinatorBackupHandler->UpdateCoordinatorMasterInfo(coordinatorNodes);
    EXPECT_TRUE(coordinatorNodes[0]->isMaster);
    EXPECT_FALSE(coordinatorNodes[1]->isMaster);
}