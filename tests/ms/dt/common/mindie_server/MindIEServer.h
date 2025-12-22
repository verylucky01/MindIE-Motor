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
#ifndef MINDIESERVER_H
#define MINDIESERVER_H

#include <iostream>
#include <string>
#include <memory>
#include <fstream>
#include <cstdlib> // #include <stdlib.h>
#include <pthread.h>
#include <queue>
#include <future>
#include <atomic>
#include <ctime>
#include <cstdlib>
#include "HttpServer.h"

using namespace MINDIE::MS;

class MindIEServerMock {
public:
    explicit MindIEServerMock(const std::string &keyInit);
    virtual ~MindIEServerMock() = default;
    virtual void InitMetrics();
    std::string GetKey() const;
    virtual void SetServerId(int32_t serverId);
    virtual void SetPDRespond(std::shared_ptr<ServerConnection> connection, bool isStream, std::string question,
        nlohmann::json &pResJson);
    virtual void TritonPReqHandler(std::shared_ptr<ServerConnection> connection);
    virtual void TGIPReqHandler(std::shared_ptr<ServerConnection> connection);
    virtual void MindIEPReqHandler(std::shared_ptr<ServerConnection> connection);
    virtual void OpenAIPReqHandler(std::shared_ptr<ServerConnection> connection);
    virtual void DReqHandler(std::shared_ptr<ServerConnection> connection);
    virtual void DMessageHandler(std::string reqId, std::string message, bool isStream);
    virtual void RolePHandler(std::shared_ptr<ServerConnection> connection);
    virtual void RoleDHandler(std::shared_ptr<ServerConnection> connection);
    virtual void ConfigHandler(std::shared_ptr<ServerConnection> connection);
    virtual void StatusHandler(std::shared_ptr<ServerConnection> connection);
    virtual void TokenizerHandler(std::shared_ptr<ServerConnection> connection);
    virtual void OpenAISingleReqHandler(std::shared_ptr<ServerConnection> connection);
    virtual void TritonSingleReqHandler(std::shared_ptr<ServerConnection> connection);
    virtual void MetricsHandler(std::shared_ptr<ServerConnection> connection);
    virtual void PDMetricsHandler(std::shared_ptr<ServerConnection> connection);

protected:
    std::string key;
    std::mutex mtx;
    std::string role = "none";
    std::vector<std::string> connIpVec;
    std::shared_ptr<ServerConnection> dConn;
    std::atomic<bool> init;
    int32_t id;
    std::vector<std::string> singleNodeMetrics;
    std::vector<std::string> PDNodeMetrics;
};

// 集群Mock
class ClusterMock {
public:
    void AddInstance(const std::string ip, const std::string port, std::shared_ptr<MindIEServerMock> instance)
    {
        std::string key = ip + ":" + port;
        std::unique_lock<std::mutex> lock(mtx);
        cluster[key] = instance;
    }
    std::shared_ptr<MindIEServerMock> GetInstance(const std::string ip, const std::string port)
    {
        std::string key = ip + ":" + port;
        std::unique_lock<std::mutex> lock(mtx);
        auto it = cluster.find(key);
        if (it == cluster.end()) {
            return nullptr;
        }
        return it->second;
    }
    static ClusterMock &GetCluster()
    {
        static ClusterMock gClusterMock;
        return gClusterMock;
    }

private:
    std::mutex mtx;
    std::map<std::string, std::shared_ptr<MindIEServerMock>> cluster;
};


void CreateSingleServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, uint32_t threadNum = 4, int32_t serverId = 0,
    const std::string &metricPort = "");
void CreateMindIEServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, uint32_t threadNum = 4, int32_t serverId = 0,
    const std::string &metricPort = "");
void CreateSSLServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, uint32_t threadNum = 4,
    const std::string &metricPort = "");
void CreateMetricsSSLServer(const std::string &ip1, const std::string &port1,
    const std::string &ip2, const std::string &port2, uint32_t threadNum = 4, int32_t serverId = 0);
#endif