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
#ifndef COMMMOCK_H
#define COMMMOCK_H

#include <iostream>
#include <string>
#include "HttpClient.h"


using namespace MINDIE::MS;
static void SetDefaultDeployerRespond();

static int32_t GetDeployerRespond(const Request &request, std::string& responseBody, int32_t &code);

static bool StartsWith(const std::string& str, const std::string& prefix);

static int32_t GetRandomNum();

#endif