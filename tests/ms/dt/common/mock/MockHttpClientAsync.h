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
#ifndef MOCKHTTPCLIENTASYNC_H
#define MOCKHTTPCLIENTASYNC_H
#pragma once
#include <gmock/gmock.h>
#include "HttpClientAsync.h"

class MockHttpClientAsync : public HttpClientAsync {
public:
    MOCK_METHOD2(FindId, std::vector<std::string>(const std::string& ip, const std::string& port));
    MOCK_METHOD1(GetConnection, std::shared_ptr<ClientConnection>(const std::string& connId));
    MOCK_METHOD2(AddConnection, void(const std::string& connId, std::shared_ptr<ClientConnection> conn));
};

#endif // MOCKHTTPCLIENTASYNC_H
