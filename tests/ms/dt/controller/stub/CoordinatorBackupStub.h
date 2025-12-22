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
#ifndef COORDINATORBACKUPSTUB_H
#define COORDINATORBACKUPSTUB_H

#include <string>
#include <nlohmann/json.hpp>
#include <memory>
#include <iostream>
#include "HttpClient.h"
#include "CoordinatorStore.h"

using namespace std;
using namespace MINDIE::MS;

void AddCoordinator(std::vector<std::unique_ptr<Coordinator>> &coordinatorNodes,
    const std::string& ip, const std::string& port, bool isHealthy);

int32_t InitMock();

void SetHostAndPortMock(const std::string &host, const std::string &port);

int32_t SendRequestMock01();

int32_t SendRequestMock02();

vector<unique_ptr<Coordinator>> GetCoordinatorsMock01();

vector<unique_ptr<Coordinator>> GetCoordinatorsMock02();

vector<unique_ptr<Coordinator>> GetCoordinatorsMock03();

#endif // COORDINATORBACKUPSTUB_H