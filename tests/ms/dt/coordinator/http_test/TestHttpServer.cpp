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
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <memory>
#include <pthread.h>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <ctime>
#include <cstdlib>
#include "gtest/gtest.h"
#include "MyThreadPool.h"
#include "HttpServer.h"
#include "Configure.h"

using namespace MINDIE::MS;
using namespace std;

class TestHttp : public testing::Test {
protected:
    static std::size_t run_stub()
    {
        std::cout << "run_stub" << std::endl;
    }
};

static int getRand(size_t min, size_t max) {
    srand(time(nullptr));
    return ( rand() % (max - min + 1) ) + min ;
}

static void Send(std::shared_ptr<ServerConnection> connection, size_t resNum)
{
    std::thread::id this_id = std::this_thread::get_id();
    cout << "Thread ID:"<<this_id<<endl;
    for (size_t i = 0; i < resNum; ++i) {
            ServerRes res;
            if (i < (resNum - 1)) {
                res.isFinish = false;
            }
            res.body = "HttpServerTest";
            res.contentType = "text/event-stream";
            connection->SendRes(res);
        }
}

static MyThreadPool pools(4);

static void HttpServerConcurrencyTC01(std::shared_ptr<ServerConnection> connection)
{
    std::cout << "HttpServerConcurrencyTC01" << std::endl;
    auto task = pools.submit(Send, connection, 50);
}

static void HttpServerConcurrencyTC02(std::shared_ptr<ServerConnection> connection)
{
    std::cout << "HttpServerConcurrencyTC02" << std::endl;
    size_t r = getRand(1, 50);
    auto task = pools.submit(Send, connection, r);
}

/*
测试描述: 测试HttpServer高并发场景 UTHttpServerConcurrencyTC01
测试步骤:
    1. 构建http server,设置线程数为1
    2. 使用工具向http server并发发送请求
    3. http server收到请求后，回复50个响应

预期结果:
    1. 无cord dumped
*/
TEST_F(TestHttp, UTHttpServerConcurrencyTC01)
{
    
    // 构造一个http server
    HttpServer httpServer;
    httpServer.Init(1);
    // 设置回调函数
    ServerHandler handler;
    // 注册请求到达时的回调函数
    handler.RegisterFun(boost::beast::http::verb::get, "/",
        std::bind(HttpServerConcurrencyTC01, std::placeholders::_1));
    HttpServerParm parm;
    parm.address = Configure::Singleton()->httpConfig.predIp;
    parm.port = Configure::Singleton()->httpConfig.predPort; // 端口号
    parm.serverHandler = handler;
    parm.tlsItems.tlsEnable = false;
    parm.maxKeepAliveReqs = 1000;
    httpServer.Run({parm});
}

/*
测试描述: 测试HttpServer高并发场景 UTHttpServerConcurrencyTC01
测试步骤:
    1. 构建http server,设置线程数为1,监听2个端口
    2. 使用工具向http server并发发送请求
    3. http server收到请求后，回复随机个响应

预期结果:
    1. 无cord dumped
*/
TEST_F(TestHttp, UTHttpServerConcurrencyTC02)
{
    // 构造一个http server
    HttpServer httpServer;
    httpServer.Init(1);
    // 设置回调函数
    ServerHandler handler;
    // 注册请求到达时的回调函数
    handler.RegisterFun(boost::beast::http::verb::get, "/",
        std::bind(HttpServerConcurrencyTC02, std::placeholders::_1));
    HttpServerParm parm1;
    parm1.address = Configure::Singleton()->httpConfig.predIp; // 数据面ip
    parm1.port = Configure::Singleton()->httpConfig.predPort; // 数据面端口
    parm1.serverHandler = handler;
    parm1.tlsItems.tlsEnable = false;
    parm1.maxKeepAliveReqs = 1000;
    HttpServerParm parm2;
    parm2.address = Configure::Singleton()->httpConfig.managementIp; // 管理面ip
    parm2.port = Configure::Singleton()->httpConfig.managementPort; // 管理面端口
    parm2.serverHandler = handler;
    parm2.tlsItems.tlsEnable = false;
    parm2.maxKeepAliveReqs = 1000;
    httpServer.Run({parm1, parm2});
}