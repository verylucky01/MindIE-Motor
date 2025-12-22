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


function show_help() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  --subdir <directory>    Specify a subdirectory for tests"
    echo "  --report-path <path>    Specify the path for the report"
    echo "  --help                  Show this help message"
}

function parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --subdir)
                USE_SUB_DIR=True
                TEST_DIR="$2"
                shift 2
                ;;
            --report-path)
                REPORT_PATH="$2"
                shift 2
                ;;
            --help)
                show_help
                exit 0
                ;;
            *)
                echo "Unknown option: $1"
                show_help
                exit 1
                ;;
        esac
    done
}

function setup_environment() {
    SCRIPT_DIR=$(dirname "$(realpath "$0")")
    TEST_DIR=$SCRIPT_DIR
    ROOT_TEST_DIR=$(dirname "${SCRIPT_DIR}")
    REPORT_PATH=$ROOT_TEST_DIR/reports
    USE_SUB_DIR=False

    # Get the root home directory
    DT_ROOT_HOME=$(dirname "${ROOT_TEST_DIR}")
    # Define paths
    DT_BENCHMARK_SRC_HOME="$DT_ROOT_HOME/mindie_service/tools/benchmark"
    DT_BENCHMARK_TEST_HOME="$DT_ROOT_HOME/tests"
    DT_BENCHMARK_MOCK_HOME="$DT_BENCHMARK_TEST_HOME/benchmark/mockers"

    # Set PYTHONPATH
    export PYTHONPATH="$DT_ROOT_HOME:$DT_BENCHMARK_SRC_HOME:$DT_BENCHMARK_TEST_HOME:$DT_BENCHMARK_MOCK_HOME"
    # Export Required Path for DT
    export DT_ROOT_HOME
    export DT_BENCHMARK_SRC_HOME
    export DT_BENCHMARK_TEST_HOME
    export DT_BENCHMARK_MOCK_HOME
    # Set Coverage File path
    export COVERAGE_FILE="${TEST_DIR}/.coverage"
}

function run_tests() {
    cd $ROOT_TEST_DIR
    python -m venv venv_dt
    source venv_dt/bin/activate
    cd - || exit

    #cov选项只包括mindiebenchmark,排除其他模块(包括dt)
    # pytest ${TEST_DIR} -o log_cli=True --log-cli-level=INFO
    pytest ${TEST_DIR} --cov=benchmark/mindiebenchmark  --cov-branch \
    --junit-xml=${REPORT_PATH}/benchmark_final.xml --html=${REPORT_PATH}/benchmark_final.html \
    --self-contained-html --cov-config=tests/benchmark/.coveragerc \
    --continue-on-collection-errors #避免collecting error中断test session，建议开发环境下取消该参数以提高效率


    coverage html -d ${REPORT_PATH}/report_benchmark/htmlcov
    coverage xml -o ${REPORT_PATH}/report_benchmark/coverage.xml
    deactivate
}

function main() {
    setup_environment
    parse_args "$@"
    run_tests
}

main "$@"
