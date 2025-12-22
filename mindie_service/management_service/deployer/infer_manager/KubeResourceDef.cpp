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
#include "Logger.h"
#include "KubeResourceDef.h"

namespace MINDIE::MS {


int32_t SetMountPath(LoadServiceParams &serverParams)
{
    std::vector<std::tuple<std::string, std::string, bool>> mountPathLists = {
        std::make_tuple("/mnt/mindie-service/ms/model", "Directory", true),
        std::make_tuple("/mnt/mindie-service/ms/writable-data", "Directory", false),
        std::make_tuple("/mnt/mindie-service/ms/run/run.sh", "File", true),
        std::make_tuple("/mnt/mindie-service/ms/config/config.json", "File", true) };
    std::ostringstream tmpOss;
    for (const auto& tuple : mountPathLists) {
        tmpOss << std::get<0>(tuple) << " ";
    }
    std::string hostPathListStr = tmpOss.str();
    LOG_I("The following host paths and their corresponding container paths are being mounted: %s.",
        hostPathListStr.c_str());
    for (uint32_t i = 0; i < mountPathLists.size(); i++) {
        serverParams.mountMap[std::get<0>(mountPathLists[i])] = mountPathLists[i];
    }
    return 0;
}


void SetHostPathVolumns(nlohmann::json &podSpec, nlohmann::json &containers,
    const LoadServiceParams &serverParams)
{
    uint32_t index = 0;
    for (auto i = serverParams.mountMap.begin(); i != serverParams.mountMap.end(); ++i) {
        nlohmann::json mount = nlohmann::json::object();
        mount["name"] = "path-" + std::to_string(index);
        mount["mountPath"] = i->first;
        mount["readOnly"] = std::get<2>(i->second); // 2 表示tuple的第3个元素，控制readyonly选项
        containers["volumeMounts"].emplace_back(mount);
        nlohmann::json volumes = nlohmann::json::object();
        volumes["name"] = "path-" + std::to_string(index);
        volumes["hostPath"] = nlohmann::json::object();
        volumes["hostPath"]["path"] = std::get<0>(i->second);
        volumes["hostPath"]["type"] = std::get<1>(i->second);
        podSpec["volumes"].emplace_back(volumes);
        index++;
    }
    return;
}

int32_t ParseProbeSetting(const nlohmann::json &mainObj, LoadServiceParams &serverParams)
{
    if (!IsJsonIntValid(mainObj, "liveness_timeout", 1, 600)) { // 1 600 最大超时时间
        return -1;
    }
    serverParams.livenessTimeout = mainObj["liveness_timeout"];
    if (!IsJsonIntValid(mainObj, "liveness_failure_threshold", 1, 10)) { // 1 10 最大失败次数
        return -1;
    }
    serverParams.livenessFailureThreshold = mainObj["liveness_failure_threshold"];
    if (!mainObj.contains("init_delay") || !mainObj["init_delay"].is_number_integer()) {
        LOG_E("[%s] [Deployer] Lack of key 'init_delay' or the value is invalid.",
            GetErrorCode(ErrorType::INVALID_INPUT, DeployerFeature::KUBERESOURCE_DEF).c_str());
        return -1;
    }
    if (!IsJsonBoolValid(mainObj, "detect_server_inner_error")) {
        return -1;
    }
    serverParams.detectInnerError = mainObj["detect_server_inner_error"];
    if (!IsJsonIntValid(mainObj, "readiness_timeout", 1, 600)) { // 1 600 最大超时时间
        return -1;
    }
    serverParams.readinessTimeout = mainObj["readiness_timeout"];
    if (!IsJsonIntValid(mainObj, "readiness_failure_threshold", 1, 10)) { // 1 10 最大失败次数
        return -1;
    }
    serverParams.readinessFailureThreshold = mainObj["readiness_failure_threshold"];
    return 0;
}
}