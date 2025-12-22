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
#ifndef MINDIE_MS_FACTORY_H
#define MINDIE_MS_FACTORY_H
#include <map>
#include <memory>
#include "ConfigParams.h"
#include "Logger.h"
#include "sys/types.h"
namespace MINDIE::MS {
class InferServerBase {
public:
    InferServerBase() = default;
    virtual ~InferServerBase() = default;
    InferServerBase(const InferServerBase&) = delete;
    InferServerBase& operator=(const InferServerBase&) = delete;
    virtual std::pair<int32_t, std::string> Init(HttpClientParams httpClientConfig, const std::string &statusPath) = 0;
    virtual std::pair<int32_t, std::string> Deploy(const std::string &config) = 0;
    virtual std::pair<int32_t, std::string> Update(const std::string &config) = 0;
    virtual std::pair<int32_t, std::string> GetDeployStatus() = 0;
    virtual std::pair<int32_t, std::string> Unload() = 0;
    virtual int32_t FromJson(const nlohmann::json &serverStatus) = 0;
};

using InferServerCreator = std::unique_ptr<InferServerBase> (*)();

class InferServerFactory {
public:
    InferServerFactory(const InferServerFactory&) = delete;
    InferServerFactory& operator=(const InferServerFactory&) = delete;

    static InferServerFactory *GetInstance()
    {
        static InferServerFactory singleton;
        return &singleton;
    }

    int32_t Insertcreator(InferServerCreator creator, const std::string &type)
    {
        auto iter = mCreatorList.find(type);
        if (iter != mCreatorList.end()) {
            LOG_E("[%s] [Deployer] Infer server creator is already inserted!",
                GetErrorCode(ErrorType::OPERATION_REPEAT, DeployerFeature::SERVER_FACTORY).c_str());
            return -1;
        }
        mCreatorList.insert(std::make_pair(type, creator));
        return 0;
    }

    std::unique_ptr<InferServerBase> CreateInferServer(const std::string &type)
    {
        auto iter = mCreatorList.find(type);
        if (iter == mCreatorList.end()) {
            LOG_E("[%s] [Deployer] Infer server not found.",
                GetErrorCode(ErrorType::NOT_FOUND, DeployerFeature::SERVER_FACTORY).c_str());
            return nullptr;
        }
        InferServerCreator creator = iter->second;
        std::unique_ptr<InferServerBase> server = creator();
        return server;
    }

private:
    InferServerFactory() = default;
    ~InferServerFactory()
    {
        mCreatorList.clear();
    }
    std::map<std::string, InferServerCreator> mCreatorList {};
};

template <class T> class InferServerCreatorRegister {
public:
    explicit InferServerCreatorRegister(const std::string &type) noexcept
    {
        auto creator = []() -> std::unique_ptr<InferServerBase> {
            auto inferServer = std::unique_ptr<InferServerBase>(new (std::nothrow) T());
            return inferServer;
        };
        InferServerFactory::GetInstance()->Insertcreator(creator, type);
    }
    ~InferServerCreatorRegister() = default;
};

#define REGISTER_INFER_SERVER(className, serverType) \
    static InferServerCreatorRegister<className> Infer##serverType(#serverType)

}
#endif
