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
#ifndef MINDIE_MS_STATUS_HANDLER_H
#define MINDIE_MS_STATUS_HANDLER_H
#include <mutex>
#include <nlohmann/json.hpp>
#include <sys/types.h>
#include "Logger.h"
namespace MINDIE::MS {
class StatusHandler {
public:
    static StatusHandler *GetInstance()
    {
        static StatusHandler singleton;
        return &singleton;
    }

    int32_t RemoveServerStatus(std::string serverName);

    int32_t SaveStatusToFile(ServerSaveStatus status);

    void SetStatusFile(const std::string &statusFile)
    {
        this->mStatusFile = statusFile;
    }
private:
    int32_t GetStatusFromPath(nlohmann::json &statusJson);
    std::string mStatusFile;
    std::mutex mMutex;
};

}
#endif
