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
#ifndef MINDIE_MS_INFER_MANAGER_H
#define MINDIE_MS_INFER_MANAGER_H
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>
#include "Logger.h"
#include "ConfigParams.h"
#include "ServerFactory.h"
#include "HttpClient.h"
#include "HttpServer.h"
namespace Http = Beast::http;
namespace MINDIE::MS {

/// This class handles HTTP requests for server deployment.
///
/// It supports server deployment, query, uninstallation, and update.
class ServerManager {
public:
    explicit ServerManager(const HttpClientParams &params) : mClientParams(params) {}
    std::pair<ErrorCode, Response> PostServersHandler(const Http::request<Http::string_body> &req);

    /// Deploy the server.
    ///
    /// Deploy the server. This is an asynchronous method.
    /// \param req The HTTP request content.
    /// \return The results of deployment.
    std::pair<ErrorCode, Response> LoadServer(const std::string &loadConfigJson);

    /// Uninstall the server.
    ///
    /// Uninstall the server and delete related resources.
    /// \param req The HTTP request content.
    /// \return The results of uninstallation.
    std::pair<ErrorCode, Response> UnloadServer(const Http::request<Http::string_body> &req);

    /// Query the server's deployment status.
    ///
    /// Query the deployment stage, readiness status, model information, etc.
    /// \param req The HTTP request content.
    /// \return The query results.
    std::pair<ErrorCode, Response> GetDeployStatus(const Http::request<Http::string_body> &req);
    int32_t FromStatusFile(const std::string &statusPath);

    /// Update the server.
    ///
    /// Forced rolling update.
    /// \param req The HTTP request content.
    /// \return The results of updating.
    std::pair<ErrorCode, Response> UpdateServer(const std::string &updateConfigJson);
private:
    int32_t RestoreOneInferServer(const nlohmann::json &status);
    std::pair<ErrorCode, Response> InferServerDeploy(const std::string &loadConfigJson,
        const nlohmann::json &jsonObj, std::string &logCode);
    /// The parameters of the httpClient.
    HttpClientParams mClientParams;

    /// The httpClient to request k8s.
    HttpClient mKubeHttpClient;

    /// The inference service states of the servers.
    std::map<std::string, std::unique_ptr<InferServerBase>> mServerHandlers;

    /// The file path to save service status.
    std::string mStatusFile;

    /// The read-write lock.
    std::mutex mMutex;
};

}
#endif
