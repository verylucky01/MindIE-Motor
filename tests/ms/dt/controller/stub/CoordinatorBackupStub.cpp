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
#include "CoordinatorBackupStub.h"

using namespace MINDIE::MS;

void AddCoordinator(std::vector<std::unique_ptr<Coordinator>> &coordinatorNodes,
    const std::string& ip, const std::string& port, bool isHealthy)
{
    auto coord = std::make_unique<Coordinator>();
    coord->ip = ip;
    coord->port = port;
    coord->isHealthy = isHealthy;
    coordinatorNodes.push_back(std::move(coord));
}

int32_t InitMock()
{
    std::cout << "enter InitMock" << std::endl;
    return 0;
}

void SetHostAndPortMock(const std::string &host, const std::string &port)
{
    std::cout << "enter SetHostAndPortMock" << std::endl;
}

int32_t SendRequestMock01()
{
    return 0;
}

int32_t SendRequestMock02()
{
    return 1;
}

std::vector<std::unique_ptr<Coordinator>> GetCoordinatorsMock01()
{
    std::vector<std::unique_ptr<Coordinator>> coordinatorNodes;
    AddCoordinator(coordinatorNodes, "192.168.1.1", "8080", true);
    AddCoordinator(coordinatorNodes, "192.168.1.3", "8080", true);
    return coordinatorNodes;
}

std::vector<std::unique_ptr<Coordinator>> GetCoordinatorsMock02()
{
    std::vector<std::unique_ptr<Coordinator>> coordinatorNodes;
    AddCoordinator(coordinatorNodes, "192.168.1.1", "8080", true);
    AddCoordinator(coordinatorNodes, "192.168.1.3", "8080", true);
    coordinatorNodes[0]->recvFlow = 1;
    coordinatorNodes[1]->recvFlow = 1;
    return coordinatorNodes;
}

std::vector<std::unique_ptr<Coordinator>> GetCoordinatorsMock03()
{
    std::vector<std::unique_ptr<Coordinator>> coordinatorNodes;
    AddCoordinator(coordinatorNodes, "192.168.1.1", "8080", true);
    AddCoordinator(coordinatorNodes, "192.168.1.2", "8080", true);
    AddCoordinator(coordinatorNodes, "192.168.1.3", "8080", true);
    return coordinatorNodes;
}

