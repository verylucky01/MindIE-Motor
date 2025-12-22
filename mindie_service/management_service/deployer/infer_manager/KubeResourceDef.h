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
#ifndef MINDIE_MS_KUBE_RESOURCE_DEF
#define MINDIE_MS_KUBE_RESOURCE_DEF
#include "ConfigParams.h"
namespace MINDIE::MS {

int32_t SetMountPath(LoadServiceParams &serverParams);

void SetHostPathVolumns(nlohmann::json &podSpec, nlohmann::json &containers,
    const LoadServiceParams &serverParams);

int32_t ParseProbeSetting(const nlohmann::json &mainObj, LoadServiceParams &serverParams);
}
#endif