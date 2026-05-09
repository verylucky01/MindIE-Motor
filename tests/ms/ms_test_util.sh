#!/bin/bash
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -e

TEST_MODE=${3:-"gcov"}

_download_kmc()
{
    artget pull "MindIE-KMC 1.0.RC3.B110" -ru software -rp "x86_64-linux/3.10" -ap "${MINDIE_MS_SRC_PATH}/open_source/kmc"
}

_build_kmc()
{
    cd ${PROJECT_PATH}/utils/cert
    chmod +x -R *
    sh build.sh release true
    cp -r ${PROJECT_PATH}/utils/cert/output ${MINDIE_MS_SRC_PATH}/open_source/kmc
}

_build_proto() {
    # 确保进入目录成功
    cd "${PROJECT_PATH}/mindie_motor/src" || { echo "Error: Failed to enter build directory"; exit 1; }

    # 仅对必要文件赋权
    if [ -f "build.sh" ]; then
        chmod +x "build.sh"
    else
        echo "Error: build.sh not found"
        exit 1
    fi

    # 定义要处理的 .proto 文件列表
    protos=(
        "rpc.proto:rpc.pb.h"
        "etcdserver.proto:etcdserver.pb.h"
        "kv.proto:kv.pb.h"
    )

    # 加载函数（假设 build.sh 中定义了 gen_etcd_grpc_proto）
    source ./build.sh || { echo "Error: Failed to load build.sh"; exit 1; }

    # 遍历生成代码
    for proto_pair in "${protos[@]}"; do
        proto_file="${proto_pair%%:*}"  # 提取 .proto 文件名
        target_header="${proto_pair##*:}"  # 提取目标头文件名
        gen_etcd_grpc_proto "${proto_file}" "${target_header}" || exit 1
    done
}

_build_src()
{
    local test_mode=${1?}
    cd $MINDIE_MS_SRC_PATH
    rm -rf build
    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILDING_STAGE=publish -DCMAKE_INSTALL_PREFIX:PATH=`pwd`/install -DCMAKE_CXX_FLAGS="-DUT_FLAG=ON"
    make -j8
    make install
}

_set_env()
{
    export LD_LIBRARY_PATH=${BUILD_MIES_3RDPARTY_INSTALL_DIR}/openssl/lib:${LD_LIBRARY_PATH}
    export LD_LIBRARY_PATH=${BUILD_MINDIE_SERVICE_INSTALL_DIR}/lib:${LD_LIBRARY_PATH}
    export LD_LIBRARY_PATH=${BUILD_MIES_3RDPARTY_INSTALL_DIR}/libboundscheck/lib:${LD_LIBRARY_PATH}
}

_build_dt()
{
    local test_mode=${1?}

    cd ${MINDIE_MS_TEST_PATH}/dt
    rm -rf build && mkdir build
    cd build

    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILDING_STAGE=${test_mode} -DCMAKE_CXX_FLAGS="-DUT_FLAG=ON" ..
    make -j8
}

# 构建 ST（System Test）用例：包含所有拉起 HttpServer 服务 + 构造请求的用例
_build_st()
{
    local test_mode=${1?}

    cd ${MINDIE_MS_TEST_PATH}/st
    rm -rf build && mkdir build
    cd build

    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILDING_STAGE=${test_mode} -DCMAKE_CXX_FLAGS="-DUT_FLAG=ON" ..
    make -j8
}

# coordinator 纯 UT（不拉起 HttpServer）并发执行。
# 通过 MINDIE_MS_TEST_CONFIG_BASEDIR 使用 UT 专属配置目录，
# 避免与同时运行的 ST 进程互相覆盖配置文件。
# 并发度由 MS_UT_PARALLEL_JOBS 控制，默认 4。
_run_coordinator_ut_in_parallel()
{
    local max_jobs=${MS_UT_PARALLEL_JOBS:-4}
    local filters=(
        "TestClusterMonitor.*"
        "TestCoordinatorConfig*"
        "TestCoordinatorScheduler.*"
        "TestDigsScheduler.*"
        "TestDIGSSchedulerImpl.*"
        "TestDigsMetaResource.*"
        "TestGlobalScheduler.*"
        "TestSchedulerFramework.*"
        "TestDigsResourceInfo.*"
        "TestDigsRequest.*"
        "TestDigsRequestManager.*"
        "TestDigsRequestProfiler.*"
        "TestStaticAllocPolicy.*"
        "TestCoordinatorMain.*"
        "TestMetrics.*"
        "TestConfigure.*"
        "TestFlexProcess.*"
        "TestParseMetric.*"
        "TestFillInstancesInfoSplitedByFlex.*"
        "TestMemoryUtil.*"
    )
    echo "Run coordinator pure UT in parallel, jobs=${max_jobs}, config=${MINDIE_MS_TEST_CONFIG_BASEDIR}"
    printf '%s\n' "${filters[@]}" \
        | xargs -I{} -P "${max_jobs}" bash -c \
          'export MINDIE_MS_TEST_CONFIG_BASEDIR="$1"; ./mindie_ms_coordinator_ut --gtest_filter="$2"' \
          _ "${MINDIE_MS_TEST_CONFIG_BASEDIR}" "{}"
}

_prepare_test_config_dir()
{
    local src_dir=${1?}
    local target_dir=${2?}

    rm -rf "${target_dir}"
    cp -r "${src_dir}" "${target_dir}"
}

# ST 冒烟用例（拉起 HttpServer + 构造请求）
_run_smoke_st_cases()
{
    cd ${MINDIE_MS_TEST_PATH}/st
    ./mindie_ms_st --gtest_filter=TestHttp.*
    ./mindie_ms_st --gtest_filter=TestControllerListener.*

    ./mindie_ms_st --gtest_filter=TestPDInferReq.*
    # ./mindie_ms_st --gtest_filter=TestInferReq.*
    ./mindie_ms_st --gtest_filter=TestDResError.*
    ./mindie_ms_st --gtest_filter=TestMaxReq.*
    ./mindie_ms_st --gtest_filter=TestOpenAI.*
    ./mindie_ms_st --gtest_filter=TestRequestListener.*
    ./mindie_ms_st --gtest_filter=TestPResError.*
    ./mindie_ms_st --gtest_filter=TestPResInvalid.*
    ./mindie_ms_st --gtest_filter=TestRetry.*
    ./mindie_ms_st --gtest_filter=TestSelfDevelop.*
    ./mindie_ms_st --gtest_filter=*.TestDefaultPrefixCacheTC01
    ./mindie_ms_st --gtest_filter=*.TestDefaultPrefixCacheTC02
    ./mindie_ms_st --gtest_filter=*.TestDefaultPrefixCacheTC03
    ./mindie_ms_st --gtest_filter=*.TestDefaultRoundRobinTC01
    ./mindie_ms_st --gtest_filter=*.TestDefaultRoundRobinTC02
    ./mindie_ms_st --gtest_filter=TestTGI.*
    ./mindie_ms_st --gtest_filter=TestTokenizerAndProbe.*
    ./mindie_ms_st --gtest_filter=TestTriton.*
    ./mindie_ms_st --gtest_filter=TestVLLM.*
    ./mindie_ms_st --gtest_filter=TestSingleInferReqPrompt.*
    ./mindie_ms_st --gtest_filter=TestMaxConnection.*

    ./mindie_ms_st --gtest_filter=TestSingleDFX.TestSingleErrorTC01
    ./mindie_ms_st --gtest_filter=TestSingleDFX.TestSingleTimeoutTC01
    ./mindie_ms_st --gtest_filter=TestSingleDFX.SingleInstancesAdd
    ./mindie_ms_st --gtest_filter=TestSingleDFX.SingleInstancesUpdate
    ./mindie_ms_st --gtest_filter=TestSingleDFX.SingleInstancesRemove

    ./mindie_ms_st --gtest_filter=*.TestReqKeepAliveTimeoutTC01
    ./mindie_ms_st --gtest_filter=*.TestReqKeepAliveTimeoutTC02
    ./mindie_ms_st --gtest_filter=*.TestReqKeepAliveTimeoutTC03

    ./mindie_ms_st --gtest_filter=*.TestSingleNodeMetricsTC01
    ./mindie_ms_st --gtest_filter=*.TestMultiSingleNodeMetricsTC01
    ./mindie_ms_st --gtest_filter=*.TestSingleNodeMetricsSortTimeReUseTC01
    ./mindie_ms_st --gtest_filter=*.TestPDSeparateMetricsTC01
    ./mindie_ms_st --gtest_filter=TestSchedulerInfo.*
}

# 蓝区门禁需要执行的冒烟用例，在该处补充
_run_smoke_dt_test()
{
    cd ${MINDIE_MS_TEST_PATH}/dt

    local _common_dir="${MINDIE_MS_TEST_PATH}/common/.mindie_ms"
    local _controller_config_dir="${MINDIE_MS_TEST_PATH}/common/.mindie_ms_controller_ut"
    local _ut_config_dir="${MINDIE_MS_TEST_PATH}/common/.mindie_ms_ut"
    local _st_config_dir="${MINDIE_MS_TEST_PATH}/common/.mindie_ms_st"
    _prepare_test_config_dir "${_common_dir}" "${_controller_config_dir}"
    _prepare_test_config_dir "${_common_dir}" "${_ut_config_dir}"
    _prepare_test_config_dir "${_common_dir}" "${_st_config_dir}"

    (
        cd ${MINDIE_MS_TEST_PATH}/dt
        export MINDIE_MS_TEST_CONFIG_BASEDIR="${_controller_config_dir}"
        ./mindie_ms_controller_ut
    ) &
    local _CONTROLLER_PID=$!

    (
        cd ${MINDIE_MS_TEST_PATH}/dt
        export MINDIE_MS_TEST_CONFIG_BASEDIR="${_ut_config_dir}"
        _run_coordinator_ut_in_parallel
    ) &
    local _UT_PID=$!

    (
        export MINDIE_MS_TEST_CONFIG_BASEDIR="${_st_config_dir}"
        _run_smoke_st_cases
    ) &
    local _ST_PID=$!

    set +e
    ./mindie_ms_ipc_ut
    local _IPC_EXIT=$?
    ./mindie_ms_securityutils_ut
    local _SECURITY_EXIT=$?
    ./mindie_service_utils_ut
    local _SERVICE_EXIT=$?
    wait ${_CONTROLLER_PID}
    local _CONTROLLER_EXIT=$?
    wait ${_UT_PID}
    local _UT_EXIT=$?
    wait ${_ST_PID}
    local _ST_EXIT=$?
    set -e

    rm -rf "${_controller_config_dir}"
    rm -rf "${_ut_config_dir}"
    rm -rf "${_st_config_dir}"

    if [ ${_CONTROLLER_EXIT} -ne 0 ] || [ ${_IPC_EXIT} -ne 0 ] \
        || [ ${_SECURITY_EXIT} -ne 0 ] || [ ${_SERVICE_EXIT} -ne 0 ] \
        || [ ${_UT_EXIT} -ne 0 ] || [ ${_ST_EXIT} -ne 0 ]; then
        echo "ERROR: controller UT exit=${_CONTROLLER_EXIT}, ipc UT exit=${_IPC_EXIT}, security UT exit=${_SECURITY_EXIT}, service UT exit=${_SERVICE_EXIT}, coordinator UT exit=${_UT_EXIT}, ST exit=${_ST_EXIT}"
        exit 1
    fi
    cd ${MINDIE_MS_TEST_PATH}/dt
}


# 除冒烟外的测试用例，在该处补充
_run_other_dt_test()
{
    cd ${MINDIE_MS_TEST_PATH}/st

    ./mindie_ms_st --gtest_filter=*.TestRetryFailedTC01
    ./mindie_ms_st --gtest_filter=TestTimeout.TestFirstTokenTimeoutTC01
    ./mindie_ms_st --gtest_filter=TestTimeout.TestInferTimeoutTC01
    ./mindie_ms_st --gtest_filter=TestRetryFailed.TestRetryFailedTC02
}

# 仅ms黄区执行的用例
_run_dt_y_test()
{
    cd ${MINDIE_MS_TEST_PATH}/dt

    sudo ulimit -n 99999
    sudo ifconfig eth0:1 172.16.0.1 netmask 255.255.254.0 up
    sudo ifconfig eth0:2 172.16.0.2 netmask 255.255.254.0 up
    sudo ifconfig eth0:3 172.16.0.3 netmask 255.255.254.0 up
    sudo ifconfig eth0:4 172.16.0.4 netmask 255.255.254.0 up
    sudo ifconfig eth0:5 172.16.0.5 netmask 255.255.254.0 up
    sudo ifconfig eth0:6 172.16.0.6 netmask 255.255.254.0 up
    sudo ifconfig eth0:7 172.16.0.7 netmask 255.255.254.0 up
    sudo ifconfig eth0:8 172.16.0.8 netmask 255.255.254.0 up

    cd ${MINDIE_MS_TEST_PATH}/st
    ./mindie_ms_st --gtest_filter=TestSSLRequest.*
    ./mindie_ms_st --gtest_filter=StressTestPD.TestPDOpenAIStressTC01
    ./mindie_ms_st --gtest_filter=StressTestPD.TestPDOpenAIStressTC02
    ./mindie_ms_st --gtest_filter=StressTestSingleNode.TestSingleNodeOpenAIStressTC01
    ./mindie_ms_st --gtest_filter=StressTestSingleNode.TestSingleNodeOpenAIStressTC02
    ./mindie_ms_st --gtest_filter=TestMetricsPDSeparate.TestPDSeparateMetricsTC02


}


_gen_coverage()
{
    rm -rf tests/ms/dt/build/coordinator/config_test/CMakeFiles/mindie_ms_coordinator_config_ut.dir/TestCoordinatorConfig.gc*

    cd ${PROJECT_PATH}
    rm -rf ${PROJECT_PATH}/result ${PROJECT_PATH}/test.info ${PROJECT_PATH}/clean.info
    lcov --directory . --capture --output-file test.info -rc lcov_branch_coverage=1
    lcov --remove test.info '*third_party*' '*open_source*' '*test*' '*/usr/*' '*mock*' '*grpc_proto*' '*etcd_proto*' -o clean.info --rc lcov_branch_coverage=1
    genhtml --branch-coverage clean.info -o result -p ${PROJECT_PATH}

    # 合并测试结果
    cd ${PROJECT_PATH}/result
    python3 ${MINDIE_MS_TEST_PATH}/scripts/merge_gtest_result.py test_detail ${MINDIE_MS_TEST_PATH}/dt

    HTML_FILE="${PROJECT_PATH}/result/index.html"
    line_coverage=$(awk -F'[<>]' '/<td class="headerItem">Lines:/{getline; getline; getline; print $3}' "$HTML_FILE" | sed 's/\s*//g')
    echo "Line coverage: $line_coverage"
    line_coverage_value=$(echo "$line_coverage" | sed 's/%//')

    COVERAGE_TARGET='68'
    if (( $(echo "$line_coverage_value < $COVERAGE_TARGET" | bc -l) )); then
    echo "execute dt pipeline failed, line coverage ($line_coverage) is less than target value ($COVERAGE_TARGET%)"
        exit 1
    fi
    echo "execute dt pipeline success"
}
