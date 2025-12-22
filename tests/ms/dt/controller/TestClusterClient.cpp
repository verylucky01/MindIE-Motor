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
#include <grpcpp/grpcpp.h>
#include <atomic>
#include <thread>
#include "gtest/gtest.h"
#include "grpc_proto/recover_mindie.grpc.pb.h"
#include "grpc_proto/cluster_fault.grpc.pb.h"
#include "Helper.h"
#include "ControllerConfig.h"
#include "FaultManager.h"
#include "ClusterClient.h"

using namespace MINDIE::MS;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using config::RankTableStream;
using MINDIE::MS::SaveRankTableCallback;
using MINDIE::MS::DealwithFaultMsgCallback;

class FakeConfigService final : public ::config::Config::Service {
public:
    std::atomic<bool> failRegister{false};
    std::atomic<bool> failSubscribe{false};
    // 注册 RPC： 保证返回 OK
    Status Register(ServerContext* ctx, const config::ClientInfo* req, config::Status* resp) override
    {
        if (failRegister) {
            return Status(grpc::StatusCode::INTERNAL, "fail to register");
        }
        resp->set_code(0);
        resp->set_info("ok");
        return Status::OK;
    }
    // 流式 RPC： 连续发两条 rank table 后结束
    Status SubscribeRankTable(ServerContext* ctx, const config::ClientInfo* req,
                              grpc::ServerWriter<RankTableStream>* writer) override
    {
        if (failSubscribe) {
            return Status(grpc::StatusCode::INTERNAL, "fail to subscribe");
        }
        RankTableStream msg;
        msg.set_ranktable("first_chunk");
        if (!writer->Write(msg)) {
            return Status(grpc::StatusCode::INTERNAL, "fail to write");
        }
        msg.set_ranktable("second_chunk");
        writer->Write(msg);
        return Status::OK;
    }
};

class FakeFaultService final : public ::fault::Fault::Service {
public:
    std::atomic<bool> failRegister{false};
    std::atomic<bool> failSubscribe{false};
    std::atomic<bool> sendUnhealthy{false};

    Status Register(ServerContext* ctx, const fault::ClientInfo* req, fault::Status* resp) override
    {
        if (failRegister) {
            return Status(grpc::StatusCode::INTERNAL, "fail to register");
        }
        resp->set_code(0);
        resp->set_info("ok");
        return Status::OK;
    }

    Status SubscribeFaultMsgSignal(ServerContext* ctx, const fault::ClientInfo* req,
                                   grpc::ServerWriter<fault::FaultMsgSignal>* writer) override
    {
        if (failSubscribe) {
            return Status(grpc::StatusCode::INTERNAL, "fail to subscribe");
        }
        fault::FaultMsgSignal msg;
        msg.set_signaltype("normal");
        msg.set_uuid("uid");
        msg.set_jobid(req->jobid());
        fault::NodeFaultInfo* node = msg.add_nodefaultinfo();
        node->set_nodename("node1");
        node->set_nodeip("127.0.0.2");
        node->set_nodesn("");
        node->set_faultlevel(sendUnhealthy ? "UnHealthy" : "healthy");
        fault::DeviceFaultInfo* device = node->add_faultdevice();
        device->set_deviceid("1");
        device->set_devicetype("npu");
        device->add_faultcodes("testcode");
        device->add_faulttype("npu");
        device->add_faultreason("link error");
        device->set_faultlevel(sendUnhealthy ? "UnHealthy" : "healthy");
        if (!writer->Write(msg)) {
            return Status(grpc::StatusCode::INTERNAL, "fail to write");
        }
        return Status::OK;
    }
};

class TestClusterClient : public ::testing::Test {
protected:
    std::unique_ptr<Server> server_;
    ClusterClient* client_;
    FakeConfigService rankService;
    FakeFaultService faultService;
    int testPort = 12345;
    std::shared_ptr<NodeStatus> mockNodeStatus;

    void SetUp() override
    {
        mockNodeStatus = std::make_shared<NodeStatus>();
        CopyDefaultConfig();
        auto controllerJson = GetMSControllerConfigJsonPath();
        auto testJson = GetMSControllerTestJsonPath();
        CopyFile(controllerJson, testJson);
        ModifyJsonItem(testJson, "tls_config", "cluster_tls_enable", false);
        ModifyJsonItem(testJson, "cluster_port", "", testPort);
        ChangeFileMode(testJson);
        ChangeFileMode(GetModelConfigTestJsonPath());
        ChangeFileMode(GetMachineConfigTestJsonPath());
        setenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH", testJson.c_str(), 1);
        ControllerConfig::GetInstance()->Init();

        ServerBuilder builder;
        builder.AddListeningPort("127.0.0.1:12345",
                                 grpc::InsecureServerCredentials());
        builder.RegisterService(&rankService);
        builder.RegisterService(&faultService);
        server_ = builder.BuildAndStart();

        setenv("MINDX_TASK_ID", "job_42", 1);
        setenv("MINDX_SERVER_IP", "127.0.0.1", 1);

        client_ = ClusterClient::GetInstance();
        client_->SetNodeStatus(mockNodeStatus);

        std::atomic<bool> waitClusterDGRTSave{false};
        client_->SetWaitClusterDGRTSave(&waitClusterDGRTSave);
    }

    void TearDown() override
    {
        rankService.failRegister = false;
        rankService.failSubscribe = false;
        faultService.failRegister = false;
        faultService.failSubscribe = false;
        faultService.sendUnhealthy = false;

        unsetenv("MINDX_TASK_ID");
        unsetenv("MINDX_SERVER_IP");
        unsetenv("MINDIE_MS_CONTROLLER_CONFIG_FILE_PATH");
        client_->Reset();
        server_->Shutdown();
    }
};

TEST_F(TestClusterClient, Register_Failed)
{
    unsetenv("MINDX_TASK_ID");
    EXPECT_EQ(client_->Init(), -1);
    ASSERT_TRUE(server_) << "Server not started";
    EXPECT_EQ(client_->Register(), -1);
    setenv("MINDX_TASK_ID", "job", 1);
    EXPECT_EQ(client_->Init(), 0);
    unsetenv("MINDX_SERVER_IP");
    EXPECT_EQ(client_->Register(), -1);
}

TEST_F(TestClusterClient, RegisterTls_Failed)
{
    // Init 之后，Register 应成功，返回 0
    auto testJson = GetMSControllerTestJsonPath();
    ModifyJsonItem(testJson, "tls_config", "cluster_tls_enable", true);
    ControllerConfig::GetInstance()->Init();
    EXPECT_EQ(client_->Init(), 0);
    // 确保服务端已启动
    ASSERT_TRUE(server_) << "Server not started";
    EXPECT_EQ(client_->Register(), -1);
}

TEST_F(TestClusterClient, SubscribeRankTable_Success)
{
    // 先完成注册
    ASSERT_EQ(client_->Init(), 0);
    ASSERT_EQ(client_->Register(), 0);

    // 收集回调到的结果
    // 执行订阅
    EXPECT_EQ(client_->SubscribeRankTable(SaveRankTableCallback), 0);
}

TEST_F(TestClusterClient, SubscribeRankTable_Failure)
{
    ASSERT_EQ(client_->Init(), 0);
    ASSERT_EQ(client_->Register(), 0);
    rankService.failSubscribe = true;
    EXPECT_EQ(client_->SubscribeRankTable(SaveRankTableCallback), -1);
}

TEST_F(TestClusterClient, SubscribeFaultMsgSignal_Success)
{
    ASSERT_EQ(client_->Init(), 0);
    ASSERT_EQ(client_->Register(), 0);
    EXPECT_EQ(client_->SubscribeFaultMsgSignal(DealwithFaultMsgCallback), 0);
}

TEST_F(TestClusterClient, SubscribeFaultMsgSignal_Failure)
{
    ASSERT_EQ(client_->Init(), 0);
    ASSERT_EQ(client_->Register(), 0);
    faultService.failSubscribe = true;
    EXPECT_EQ(client_->SubscribeFaultMsgSignal(DealwithFaultMsgCallback), -1);
}

TEST_F(TestClusterClient, AddFaultNodeByNodeIp_Unhealthy)
{
    faultService.sendUnhealthy = true;
    auto node = std::make_unique<NodeInfo>();
    node->hostId = "127.0.0.2";
    node->ip = "127.0.0.2";

    ServerInfo serverInfo;
    serverInfo.hostId = "127.0.0.2";
    serverInfo.ip = "127.0.0.2";
    node->serverInfoList.push_back(serverInfo);

    mockNodeStatus->AddNode(std::move(node));
    fault::NodeFaultInfo nodeInfo;
    nodeInfo.set_nodeip("127.0.0.2");
    nodeInfo.set_faultlevel("UnHealthy");
    client_->AddFaultNodeByNodeIp(nodeInfo);
}
/*
测试描述： 测试 ClusterClient 的 Start 方法和线程重启功能。
测试步骤：
    1. 调用 Start 方法，传入 mockNodeStatus。
    2. 等待一段时间，确保线程启动。
    3. 恢复环境，停止线程。
预期结果：
    1. Start 方法返回 0，表示成功启动。
    2. mFaultStop 和 mRankTableStop 为 true。
 */
TEST_F(TestClusterClient, StartAndThreadRestart)
{
    ASSERT_EQ(client_->Start(mockNodeStatus), 0);
    // 等待线程就绪
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(client_->mFaultStop);
    EXPECT_TRUE(client_->mRankTableStop);
    unsetenv("MINDX_TASK_ID");
}

/*
测试描述：测试 ReportAlarm 函数设置告警标志位
测试步骤：
    1. 初始化 ClusterClient
    2. 分别调用不同类型的 ReportAlarm
    3. 验证对应的标志位被设置为 true
预期结果：对应的原子标志位被正确设置
*/
TEST_F(TestClusterClient, ReportAlarm_SetFlags)
{
    ASSERT_EQ(client_->Init(), 0);
    
    // 测试 RANKTABLE_SUBSCRIBE_FAILED
    client_->ReportAlarm(ClusterConnectionReason::RANKTABLE_SUBSCRIBE_FAILED);
    EXPECT_TRUE(client_->mRankTableAlarmReported.load());
    
    // 测试 FAULT_SUBSCRIBE_FAILED
    client_->ReportAlarm(ClusterConnectionReason::FAULT_SUBSCRIBE_FAILED);
    EXPECT_TRUE(client_->mFaultMsgAlarmReported.load());
    
    // 测试 CONNECTION_INTERRUPTED
    client_->ReportAlarm(ClusterConnectionReason::CONNECTION_INTERRUPTED);
    EXPECT_TRUE(client_->mConnectionAlarmReported.load());
    
    // 测试 REGISTER_FAILED（不设置标志位，只验证不抛异常）
    EXPECT_NO_THROW(client_->ReportAlarm(ClusterConnectionReason::REGISTER_FAILED));
}

/*
测试描述：测试 ClearAlarm 函数清除告警标志位
测试步骤：
    1. 先设置告警标志位
    2. 调用 ClearAlarm 清除告警
    3. 验证标志位被设置为 false
预期结果：对应的原子标志位被正确清除
*/
TEST_F(TestClusterClient, ClearAlarm_ClearFlags)
{
    ASSERT_EQ(client_->Init(), 0);
    
    // 先设置告警
    client_->ReportAlarm(ClusterConnectionReason::RANKTABLE_SUBSCRIBE_FAILED);
    client_->ReportAlarm(ClusterConnectionReason::FAULT_SUBSCRIBE_FAILED);
    client_->ReportAlarm(ClusterConnectionReason::CONNECTION_INTERRUPTED);
    
    // 清除告警并验证
    client_->ClearAlarm(ClusterConnectionReason::RANKTABLE_SUBSCRIBE_FAILED);
    EXPECT_FALSE(client_->mRankTableAlarmReported.load());
    
    client_->ClearAlarm(ClusterConnectionReason::FAULT_SUBSCRIBE_FAILED);
    EXPECT_FALSE(client_->mFaultMsgAlarmReported.load());
    
    client_->ClearAlarm(ClusterConnectionReason::CONNECTION_INTERRUPTED);
    EXPECT_FALSE(client_->mConnectionAlarmReported.load());
}