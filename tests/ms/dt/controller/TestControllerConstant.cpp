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
#include "gtest/gtest.h"
#include "ControllerConstant.h"

using namespace MINDIE::MS;

class TestControllerConstant : public testing::Test {
    static constexpr int testNumZero = 0;
    static constexpr int invalidEnumValue = 999;

protected:
    ControllerConstant* constant = ControllerConstant::GetInstance();
};

TEST_F(TestControllerConstant, FilePathMapping)
{
    EXPECT_EQ(constant->GetFilePath(FilePath::CONFIG_PATH), "./conf/ms_controller.json");
}

TEST_F(TestControllerConstant, RoleStateMapping)
{
    EXPECT_EQ(constant->GetRoleState(RoleState::UNKNOWN), "RoleUnknown");
    EXPECT_EQ(constant->GetRoleState(RoleState::SWITCHING), "RoleSwitching");
    EXPECT_EQ(constant->GetRoleState(RoleState::READY), "RoleReady");
}

TEST_F(TestControllerConstant, CoordinatorURIMapping)
{
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::POST_REFRESH), "/v1/instances/refresh");
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::GET_INFO), "/v1/coordinator_info");
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::POST_OFFLINE), "/v1/instances/offline");
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::POST_ONLINE), "/v1/instances/online");
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::GET_TASK), "/v1/instances/tasks");
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::POST_QUERY_TASK), "/v1/instances/query_tasks");
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::GET_METRICS), "/metrics");
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::POST_BACKUP_INFO), "/backup_info");
    EXPECT_EQ(constant->GetCoordinatorURI(CoordinatorURI::GET_RECVS_INFO), "/recvs_info");
}

TEST_F(TestControllerConstant, ServerURIMapping)
{
    EXPECT_EQ(constant->GetServerURI(ServerURI::GET_CONFIG), "/v1/config");
    EXPECT_EQ(constant->GetServerURI(ServerURI::GET_STATUS_V1), "/v1/status");
    EXPECT_EQ(constant->GetServerURI(ServerURI::GET_STATUS_V2), "/v2/status");
    EXPECT_EQ(constant->GetServerURI(ServerURI::POST_DECODE_ROLE_V1), "/v1/role/decode");
    EXPECT_EQ(constant->GetServerURI(ServerURI::POST_PREFILL_ROLE_V1), "/v1/role/prefill");
    EXPECT_EQ(constant->GetServerURI(ServerURI::POST_DECODE_ROLE_V2), "/v2/role/decode");
    EXPECT_EQ(constant->GetServerURI(ServerURI::POST_PREFILL_ROLE_V2), "/v2/role/prefill");
    EXPECT_EQ(constant->GetServerURI(ServerURI::STOP_SERVICE), "/stopService");
}

TEST_F(TestControllerConstant, CCAEURIMapping)
{
    EXPECT_EQ(constant->GetCCAEURI(CCAEURI::REGISTER), "/rest/ccaeommgmt/v1/managers/mindie/register");
    EXPECT_EQ(constant->GetCCAEURI(CCAEURI::INVENTORY), "/rest/ccaeommgmt/v1/managers/mindie/inventory");
}

TEST_F(TestControllerConstant, EnvParamMapping)
{
    EXPECT_EQ(constant->GetEnvParam(EnvParam::CONTROLLER_CONFIG_FILE_PATH), "MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH");
    EXPECT_EQ(constant->GetEnvParam(EnvParam::GLOBAL_RANK_TABLE), "GLOBAL_RANK_TABLE_FILE_PATH");
    EXPECT_EQ(constant->GetEnvParam(EnvParam::P_RATE), "MINDIE_MS_P_RATE");
    EXPECT_EQ(constant->GetEnvParam(EnvParam::D_RATE), "MINDIE_MS_D_RATE");
    EXPECT_EQ(constant->GetEnvParam(EnvParam::POD_IP), "POD_IP");
    EXPECT_EQ(constant->GetEnvParam(EnvParam::CHECK_FILES), "MINDIE_CHECK_INPUTFILES_PERMISSION");
}

TEST_F(TestControllerConstant, ConfigKeyMapping)
{
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_GLOBAL_RANK_TABLE_PATH), "global_rank_table_file_path");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_SERVER_PORT), "mindie_server_port");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_SERVER_CONTROL_PORT), "mindie_server_control_port");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_SERVER_METRIC_PORT), "mindie_server_metric_port");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_COORDINATOR_PORT), "mindie_ms_coordinator_port");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_MODEL_CONFIG_FILE_PATH), "digs_model_config_path");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_MACHINE_CONFIG_FILE_PATH), "digs_machine_config_path");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_DIGS_PREFILL_SLO), "digs_prefill_slo");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_DIGS_DECODE_SLO), "digs_decode_slo");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_MODEL_TYPE), "model_type");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_TRANSFER_TYPE), "transfer_type");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_DIGS_PP), "digs_pp");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_ROLE_DECISION_METHODS), "role_decision_methods");
    EXPECT_EQ(constant->GetConfigKey(ConfigKey::CONFIG_KEY_IP), "ip");
}

TEST_F(TestControllerConstant, RoleTypeConversion)
{
    EXPECT_EQ(constant->GetRoleTypeByStr("prefill"), RoleType::PREFILL);
    EXPECT_EQ(constant->GetRoleTypeByStr("decode"), RoleType::DECODE);
    EXPECT_EQ(constant->GetRoleTypeByStr("none"), RoleType::NONE);
}

TEST_F(TestControllerConstant, DIGSRoleConversion)
{
    EXPECT_EQ(constant->GetDIGSInstanceRoleByStr("prefill"), MINDIE::MS::DIGSInstanceRole::PREFILL_INSTANCE);
    EXPECT_EQ(constant->GetDIGSInstanceRoleByStr("decode"), MINDIE::MS::DIGSInstanceRole::DECODE_INSTANCE);
    EXPECT_EQ(constant->GetDIGSInstanceRoleByStr("none"), MINDIE::MS::DIGSInstanceRole::UN_DEF_INSTANCE);
}

TEST_F(TestControllerConstant, DeployModeConversion)
{
    EXPECT_EQ(constant->GetDeployModeByStr("pd_separate"), DeployMode::PD_SEPARATE);
    EXPECT_EQ(constant->GetDeployModeByStr("pd_disaggregation"), DeployMode::PD_SEPARATE);
    EXPECT_EQ(constant->GetDeployModeByStr("pd_disaggregation_single_container"), DeployMode::PD_SEPARATE);
    EXPECT_EQ(constant->GetDeployModeByStr("single_node"), DeployMode::SINGLE_NODE);
}

TEST_F(TestControllerConstant, SingleContainerCheck)
{
    EXPECT_TRUE(constant->IsSingleContainer("pd_disaggregation_single_container"));
    EXPECT_FALSE(constant->IsSingleContainer("pd_separate"));
    EXPECT_FALSE(constant->IsSingleContainer("single_node"));
    EXPECT_FALSE(constant->IsSingleContainer("invalid_mode"));
}

TEST_F(TestControllerConstant, ConstantStrings)
{
    EXPECT_EQ(constant->GetDigsRoleDecisionMethod(), "digs");
    EXPECT_EQ(constant->GetGroupGeneratorTypeDefault(), "Default");
    EXPECT_EQ(constant->GetMindIECoordinatorGroup(), "0");
    EXPECT_EQ(constant->GetMindIEServerGroup(), "2");
    EXPECT_EQ(constant->GetMindIEVersion(), "");
    EXPECT_EQ(constant->GetComponentType(), testNumZero);
}

TEST_F(TestControllerConstant, InvalidInputHandling)
{
    EXPECT_TRUE(constant->GetFilePath(static_cast<FilePath>(invalidEnumValue)).empty());
    EXPECT_TRUE(constant->GetRoleState(static_cast<RoleState>(invalidEnumValue)).empty());

    EXPECT_EQ(constant->GetRoleTypeByStr("invalid"), RoleType{});
    EXPECT_EQ(constant->GetDIGSInstanceRoleByStr("invalid"), MINDIE::MS::DIGSInstanceRole{});
    EXPECT_EQ(constant->GetDeployModeByStr("invalid"), DeployMode{});
}