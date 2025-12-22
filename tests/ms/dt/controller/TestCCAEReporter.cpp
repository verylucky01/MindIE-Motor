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
#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <chrono>
#include <thread>

#include "gtest/gtest.h"
#include "stub.h"
#include "gmock/gmock.h"
#include "Helper.h"
#include "RequestCCAEStub.h"
#include "Util.h"
#include "JsonFileManager.h"
#include "ControllerConfig.h"
#include "CCAEStatus.h"
#include "CCAEReporter.h"

using namespace MINDIE::MS;

class TestCCAEReporter : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }

    void OneModelCheckUpdated(bool result, size_t stackSize, std::string updateModelID, std::string modelID)
    {
        EXPECT_TRUE(result);
        EXPECT_EQ(stackSize, 1);
        EXPECT_EQ(updateModelID, modelID);
    }

    void OneModelCheckNotUpdated(bool result, size_t stackSize)
    {
        EXPECT_TRUE(result);
        EXPECT_EQ(stackSize, 1);
    }

    std::unique_ptr<Coordinator> CreateCoordinator(const std::string& ip,
        const std::string& port, bool healthy, bool master)
    {
        auto coord = std::make_unique<Coordinator>();
        coord->ip = ip;
        coord->port = port;
        coord->isHealthy = healthy;
        coord->isMaster = master;
        return coord;
    }
    
    // 辅助函数：创建 Coordinator 向量
    std::vector<std::unique_ptr<Coordinator>> CreateCoordinators(
        const std::vector<std::tuple<std::string, std::string, bool, bool>>& specs)
    {
        std::vector<std::unique_ptr<Coordinator>> coordinators;
        constexpr size_t ipIdx = 0;
        constexpr size_t portIdx = 1;
        constexpr size_t isHealthyIdx = 2;
        constexpr size_t isMasterIdx = 3;
        for (const auto& spec : specs) {
            coordinators.push_back(CreateCoordinator(
                std::get<ipIdx>(spec),
                std::get<portIdx>(spec),
                std::get<isHealthyIdx>(spec),
                std::get<isMasterIdx>(spec)
            ));
        }
        return coordinators;
    }
};

#ifdef UT_FLAG
TEST_F(TestCCAEReporter, TestUpdateOneModelWithMetricsPeriod)
{
    auto ccaeStatus = std::make_shared<CCAEStatus>();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto ccaeReporter = std::make_unique<CCAEReporter>(ccaeStatus, coordinatorStore, coordinatorStore, nodeStatus);
    auto ctrlConf = ControllerConfig::GetInstance();
    ctrlConf->mModelIDVec.clear();
    ctrlConf->mModelIDVec.emplace_back(GetUUID());

    std::cout << "Set period to 1/s, update immediately." << std::endl;
    ccaeStatus->SetMetricPeriod(ctrlConf->GetModelID(0), 1);
    ccaeStatus->SetForcedUpdate(ctrlConf->GetModelID(0), false);

    auto result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckUpdated(
        result, ccaeReporter->mModelIDStack.size(), ccaeReporter->mModelIDStack[0], ctrlConf->GetModelID(0));
    
    std::cout << "Clear stack of updated model." << std::endl;
    ccaeReporter->ResetReportStack();
    EXPECT_EQ(ccaeReporter->mModelIDStack.size(), 0);
    
    std::cout << "After 100 milliseconds, should not update at this moment." << std::endl;
    auto nowMs = GetTimeStampNowInMillisec();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait 100 ms
    // 700 milliseconds is the threshold for considering an update
    if (nowMs - ccaeReporter->mMemTimeStamp.at(ctrlConf->GetModelID(0)) < 700) {
        result = ccaeReporter->UpdateModelID2Report();
        OneModelCheckNotUpdated(result, ccaeReporter->mModelIDStack.size());
    }

    std::cout << "Set period to 5/s, update immediately." << std::endl;
    ccaeStatus->SetMetricPeriod(ctrlConf->GetModelID(0), 5); // set metric period to 5
    result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckUpdated(
        result, ccaeReporter->mModelIDStack.size(), ccaeReporter->mModelIDStack[0], ctrlConf->GetModelID(0));
    
    std::cout << "After 200 ms, should update now." << std::endl;
    ccaeReporter->ResetReportStack();
    std::this_thread::sleep_for(std::chrono::milliseconds(200)); // wait 200 ms
    result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckUpdated(
        result, ccaeReporter->mModelIDStack.size(), ccaeReporter->mModelIDStack[0], ctrlConf->GetModelID(0));

    std::cout << "After 100 ms, should not update now." << std::endl;
    ccaeReporter->ResetReportStack();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait 100 ms
    result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckNotUpdated(result, ccaeReporter->mModelIDStack.size());
    
    std::cout << "After another 100 ms, should update now." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // wait 100 ms
    result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckUpdated(
        result, ccaeReporter->mModelIDStack.size(), ccaeReporter->mModelIDStack[0], ctrlConf->GetModelID(0));
}

TEST_F(TestCCAEReporter, TestUpdateOneModelWithForceUpdate)
{
    auto ccaeStatus = std::make_shared<CCAEStatus>();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto ccaeReporter = std::make_unique<CCAEReporter>(ccaeStatus, coordinatorStore, coordinatorStore, nodeStatus);
    auto ctrlConf = ControllerConfig::GetInstance();
    ctrlConf->mModelIDVec.clear();
    ctrlConf->mModelIDVec.emplace_back(GetUUID());

    std::cout << "Set force update when tiem period is 0, should update now." << std::endl;
    ccaeStatus->SetMetricPeriod(ctrlConf->GetModelID(0), 0);
    ccaeStatus->SetForcedUpdate(ctrlConf->GetModelID(0), true);

    auto result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckUpdated(
        result, ccaeReporter->mModelIDStack.size(), ccaeReporter->mModelIDStack[0], ctrlConf->GetModelID(0));
    
    std::cout << "Clear stack of updated model, should unset force update params of ccae status." << std::endl;
    ccaeReporter->ResetReportStack();
    EXPECT_FALSE(ccaeStatus->ISForcedUpdate(ctrlConf->GetModelID(0)));

    std::cout << "Set period to 1/s, update immediately." << std::endl;
    ccaeStatus->SetMetricPeriod(ctrlConf->GetModelID(0), 1);
    result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckUpdated(
        result, ccaeReporter->mModelIDStack.size(), ccaeReporter->mModelIDStack[0], ctrlConf->GetModelID(0));

    std::cout << "After another 300 ms, set force update, should update now." << std::endl;
    ccaeReporter->ResetReportStack();
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // wait 300 ms
    ccaeStatus->SetForcedUpdate(ctrlConf->GetModelID(0), true);
    result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckUpdated(
        result, ccaeReporter->mModelIDStack.size(), ccaeReporter->mModelIDStack[0], ctrlConf->GetModelID(0));
    
    std::cout << "After another 700 ms, should update now." << std::endl;
    ccaeReporter->ResetReportStack();
    std::this_thread::sleep_for(std::chrono::milliseconds(700)); // wait 700 ms
    result = ccaeReporter->UpdateModelID2Report();
    OneModelCheckUpdated(
        result, ccaeReporter->mModelIDStack.size(), ccaeReporter->mModelIDStack[0], ctrlConf->GetModelID(0));
}

// 测试用例1：空的Coordintaor列表
TEST_F(TestCCAEReporter, TestSelectMasterCoordinatorEmptyCoordinatorList)
{
    auto ccaeStatus = std::make_shared<CCAEStatus>();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto ccaeReporter = std::make_unique<CCAEReporter>(ccaeStatus, coordinatorStore, coordinatorStore, nodeStatus);

    std::vector<std::unique_ptr<Coordinator>> coordinators;
    std::vector<std::unique_ptr<Coordinator>> selectedCoordinators;

    ccaeReporter->mCoordinatorStore->UpdateCoordinators(coordinators);

    ccaeReporter->SelectMasterCoordinator(selectedCoordinators);
    EXPECT_EQ(selectedCoordinators.size(), 0);
}

// 测试用例2：只有一个主节点
TEST_F(TestCCAEReporter, TestSelectMasterCoordinatorSingleMasterNode)
{
    auto ccaeStatus = std::make_shared<CCAEStatus>();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto ccaeReporter = std::make_unique<CCAEReporter>(ccaeStatus, coordinatorStore, coordinatorStore, nodeStatus);

    std::vector<std::unique_ptr<Coordinator>> coordinators;
    std::vector<std::unique_ptr<Coordinator>> selectedCoordinators;

    coordinators = CreateCoordinators({
        {"192.168.1.1", "8080", true, true},   // 主节点
        {"192.168.1.2", "8080", true, false},  // 从节点
        {"192.168.1.3", "8080", true, false},  // 从节点
    });
    ccaeReporter->mCoordinatorStore->UpdateCoordinators(coordinators);

    ccaeReporter->SelectMasterCoordinator(selectedCoordinators);
    EXPECT_EQ(selectedCoordinators.size(), 1);
}

// 测试用例3：没有上次主节点记录, 没有主节点
TEST_F(TestCCAEReporter, TestSelectMasterCoordinatorNoMasterNode)
{
    auto ccaeStatus = std::make_shared<CCAEStatus>();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto ccaeReporter = std::make_unique<CCAEReporter>(ccaeStatus, coordinatorStore, coordinatorStore, nodeStatus);

    std::vector<std::unique_ptr<Coordinator>> coordinators;
    std::vector<std::unique_ptr<Coordinator>> selectedCoordinators;

    coordinators = CreateCoordinators({
        {"192.168.1.1", "8080", true, false},   // 从节点
        {"192.168.1.2", "8080", true, false},  // 从节点
        {"192.168.1.3", "8080", true, false},  // 从节点
    });
    ccaeReporter->mCoordinatorStore->UpdateCoordinators(coordinators);

    ccaeReporter->SelectMasterCoordinator(selectedCoordinators);
    EXPECT_EQ(selectedCoordinators.size(), 0);
}

// 测试用例4：没有上次主节点记录,两个主节点
TEST_F(TestCCAEReporter, TestSelectMasterCoordinatorTwoMasterNode)
{
    auto ccaeStatus = std::make_shared<CCAEStatus>();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto ccaeReporter = std::make_unique<CCAEReporter>(ccaeStatus, coordinatorStore, coordinatorStore, nodeStatus);

    std::vector<std::unique_ptr<Coordinator>> coordinators;
    std::vector<std::unique_ptr<Coordinator>> selectedCoordinators;

    coordinators = CreateCoordinators({
        {"192.168.1.1", "8080", true, true},   // 主节点
        {"192.168.1.2", "8080", true, true},  // 主节点
        {"192.168.1.3", "8080", true, false},  // 从节点
    });
    ccaeReporter->mCoordinatorStore->UpdateCoordinators(coordinators);

    ccaeReporter->SelectMasterCoordinator(selectedCoordinators);
    EXPECT_EQ(selectedCoordinators.size(), 0);
}

// 测试用例5：有上次的主节点记录, 没有主节点
TEST_F(TestCCAEReporter, TestSelectMasterCoordinatorNoMasterNodeWithLastMaster)
{
    auto ccaeStatus = std::make_shared<CCAEStatus>();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto ccaeReporter = std::make_unique<CCAEReporter>(ccaeStatus, coordinatorStore, coordinatorStore, nodeStatus);

    std::vector<std::unique_ptr<Coordinator>> coordinators;
    std::vector<std::unique_ptr<Coordinator>> selectedCoordinators;

    // Set up mLastMasterKey
    coordinators = CreateCoordinators({
        {"192.168.1.1", "8080", true, true},   // 主节点
        {"192.168.1.2", "8080", true, false}   // 从节点
    });
    ccaeReporter->mCoordinatorStore->UpdateCoordinators(coordinators);
    ccaeReporter->SelectMasterCoordinator(selectedCoordinators);
    coordinators.clear();
    selectedCoordinators.clear();

    coordinators = CreateCoordinators({
        {"192.168.1.1", "8080", true, false},   // 从节点
        {"192.168.1.2", "8080", true, false},  // 从节点
        {"192.168.1.3", "8080", true, false},  // 从节点
    });
    ccaeReporter->mCoordinatorStore->UpdateCoordinators(coordinators);

    ccaeReporter->SelectMasterCoordinator(selectedCoordinators);
    EXPECT_EQ(selectedCoordinators.size(), 1);
}

// 测试用例6：有上次的主节点记录, 两个主节点
TEST_F(TestCCAEReporter, TestSelectMasterCoordinatorTwoMasterNodeWithLastMaster)
{
    auto ccaeStatus = std::make_shared<CCAEStatus>();
    auto coordinatorStore = std::make_shared<CoordinatorStore>();
    auto nodeStatus = std::make_shared<NodeStatus>();
    auto ccaeReporter = std::make_unique<CCAEReporter>(ccaeStatus, coordinatorStore, coordinatorStore, nodeStatus);

    std::vector<std::unique_ptr<Coordinator>> coordinators;
    std::vector<std::unique_ptr<Coordinator>> selectedCoordinators;

    // Set up mLastMasterKey
    coordinators = CreateCoordinators({
        {"192.168.1.1", "8080", true, true},   // 主节点
        {"192.168.1.2", "8080", true, false}   // 从节点
    });
    ccaeReporter->mCoordinatorStore->UpdateCoordinators(coordinators);
    ccaeReporter->SelectMasterCoordinator(selectedCoordinators);
    coordinators.clear();
    selectedCoordinators.clear();

    coordinators = CreateCoordinators({
        {"192.168.1.1", "8080", true, true},   // 主节点
        {"192.168.1.2", "8080", true, true},  // 主节点
        {"192.168.1.3", "8080", true, false},  // 从节点
    });
    ccaeReporter->mCoordinatorStore->UpdateCoordinators(coordinators);

    ccaeReporter->SelectMasterCoordinator(selectedCoordinators);
    EXPECT_EQ(selectedCoordinators.size(), 1);
}
#endif