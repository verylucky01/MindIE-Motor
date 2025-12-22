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
#include <ctime>
#include <nlohmann/json.hpp>
#include "CommMock.h"

static nlohmann::json g_msDeployerRespondMap = nlohmann::json();
using namespace MINDIE::MS;

static int32_t GetRandomNum()
{
    std::srand(std::time(nullptr));
    int32_t randomNumber = std::rand() % 100 + 1;
    return randomNumber;
}

static void SetDefaultDeployerRespond()
{
    g_msDeployerRespondMap.clear();
    // k8s api server的响应
    std::vector<std::string> postEmptyRespond = {
        "/api/v1/namespaces/mindie/configmaps",
        "/api/v1/namespaces/mindie/configmaps/rings-config-mindie-server-deployment-0",
        "/apis/apps/v1/namespaces/mindie/deployments",
        "/api/v1/namespaces/mindie/deployments",
        "/api/v1/namespaces/mindie/services"

    };
    for (auto url:postEmptyRespond) {
        nlohmann::json respond = nlohmann::json();
        respond["url"] = url;
        respond["method"] = "POST";
        respond["respond"] = "";
        respond["status_code"] = GetRandomNum() % 2 == 0 ? 200 : 400;
        g_msDeployerRespondMap.push_back(respond);
    }

    std::vector<std::string> deleteEmptyRespond = {
        "/api/v1/namespaces/mindie/configmaps/rings-config-mindie-server-deployment-0",
        "/apis/apps/v1/namespaces/mindie/deployments/mindie-server-deployment-0",
        "/api/v1/namespaces/mindie/deployments/mindie-server-deployment-0",
        "/api/v1/namespaces/mindie/services/mindie-server-service",
        "/api/v1/namespaces/mindie/services/mindie-server",
        "/apis/apps/v1/namespaces/mindie/deployments/mindie-server"
    };
    for (auto url:deleteEmptyRespond) {
        nlohmann::json respond = nlohmann::json();
        respond["url"] = url;
        respond["method"] = "DELETE";
        respond["respond"] = "";
        respond["status_code"] = GetRandomNum() % 2 == 0 ? 200 : 400;
        g_msDeployerRespondMap.push_back(respond);
    }

    std::vector<std::string> patchEmptyRespond = {
        "/api/v1/namespaces/mindie/pods/mindie-server-pod",
        "/apis/apps/v1/namespaces/mindie/deployments/mindie-server"

    };
    for (auto url:deleteEmptyRespond) {
        nlohmann::json respond = nlohmann::json();
        respond["url"] = url;
        respond["method"] = "PATCH";
        respond["respond"] = "";
        respond["status_code"] = GetRandomNum() % 2 == 0 ? 200 : 400;
        g_msDeployerRespondMap.push_back(respond);
    }

    std::vector<std::string> getEmptyRespond = {
        "/api/v1/namespaces/mindie/services/mindie-server-service",
        "/apis/apps/v1/namespaces/mindie/deployments/mindie-server",
        "/api/v1/namespaces/mindie/services/mindie-server",
        "/apis/apps/v1/namespaces/mindie/deployments/mindie-server-deployment-0",

    };
    for (auto url:deleteEmptyRespond) {
        nlohmann::json respond = nlohmann::json();
        respond["url"] = url;
        respond["method"] = "GET";
        respond["respond"] = "";
        respond["status_code"] = GetRandomNum() % 2 == 0 ? 200 : 400;
        g_msDeployerRespondMap.push_back(respond);
    }

    {
        nlohmann::json respond = nlohmann::json();
        respond["url"] = "/api/v1/namespaces/mindie/configmaps/rings-config-mindie-server-deployment-0";
        respond["method"] = "GET";
        nlohmann::json res;
        res["data"] = nlohmann::json();
        res["data"]["hccl.json"] = "{\"status\":\"completed\", \"server_list\": [{\"container_ip\": \"127.0.0.1\"}]}";
        respond["respond"] = res.dump();
        respond["status_code"] = GetRandomNum() % 2 == 0 ? 200 : 400;
        g_msDeployerRespondMap.push_back(respond);
    }

    {
        nlohmann::json respond = nlohmann::json();
        respond["url"] = "/apis/apps/v1/namespaces/mindie/deployments/mindie-server-deployment-0";
        respond["method"] = "GET";
        nlohmann::json res;
        res["metadata"] = nlohmann::json();
        res["metadata"]["name"] = "rings-config-mindie-server-deployment-0";
        res["metadata"]["namespace"] = "mindie";
        res["metadata"]["annotations"] = nlohmann::json();
        res["metadata"]["annotations"]["mindie_port"] = "32001";
        res["metadata"]["annotations"]["init_delay"] = "180";

        respond["respond"] = res.dump();
        respond["status_code"] = GetRandomNum() % 2 == 0 ? 200 : 400;
        g_msDeployerRespondMap.push_back(respond);
    }

    {
        nlohmann::json respond = nlohmann::json();
        respond["url"] = "/api/v1/namespaces/mindie/pods";
        respond["method"] = "GET";
        nlohmann::json res;
        res["items"] = nlohmann::json();
        res["items"]["metadata"] = nlohmann::json();
        res["items"]["metadata"]["name"] = "mindie-server";
        res["items"]["status"] = nlohmann::json();
        res["items"]["status"]["phase"] = "Running";
        res["items"]["status"]["podIP"] = "127.0.0.1";
        res["items"]["status"]["hostIP"] = "127.0.0.1";

        respond["respond"] = res.dump();
        respond["status_code"] = GetRandomNum() % 2 == 0 ? 200 : 400;
        g_msDeployerRespondMap.push_back(respond);
    }

    {
        nlohmann::json respond = nlohmann::json();
        respond["url"] = "/api/v1/pods";
        respond["method"] = "GET";
        nlohmann::json res;
        res["items"] = nlohmann::json();
        res["items"]["metadata"] = nlohmann::json();
        res["items"]["metadata"]["name"] = "mindie-server";
        res["items"]["metadata"]["namespace"] = "mindie";
        res["items"]["status"] = nlohmann::json();
        res["items"]["status"]["podIP"] = "127.0.0.1";

        respond["respond"] = res.dump();
        respond["status_code"] = GetRandomNum() % 2 == 0 ? 200 : 400;
        g_msDeployerRespondMap.push_back(respond);
    }
}

static int32_t GetDeployerRespond(const Request &request, std::string& responseBody, int32_t &code)
{
    std::string target = request.target;
    std::string method = "";
    if (request.method == boost::beast::http::verb::get) {
        method = "GET";
    } else if (request.method == boost::beast::http::verb::post) {
        method = "POST";
    } else if (request.method == boost::beast::http::verb::delete_) {
        method = "DELETE";
    } else if (request.method == boost::beast::http::verb::patch) {
        method = "PATCH";
    }

    for (auto respond : g_msDeployerRespondMap) {
        if (respond["url"] == target && respond["method"] == method) {
            responseBody = respond["respond"];
            code = respond["status_code"];
            break;
        }
    }
    if (code != 200) {
        return -1;
    }
    return 0;
}

static bool StartsWith(const std::string& str, const std::string& prefix)
{
    auto tmp = str.substr(0, prefix.size());
    if (tmp == prefix) {
        return true;
    } else {
        return false;
    }
}