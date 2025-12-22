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


TEST_PATH=$(dirname "$(realpath "$0")")

COMPONENT="all"
REPORT_PATH="${TEST_PATH}/reports"
SUBDIR=""

install_env() {
    cd "$TEST_PATH"
    python -m venv venv_dt
    source venv_dt/bin/activate
    bash "${TEST_PATH}/install_env.sh"
    cd -
}

activate_venv() {
    source "$TEST_PATH/venv_dt/bin/activate"
}

deactivate_venv() {
    deactivate
}

run_benchmark_dt() {
    activate_venv
    local cmd="bash $TEST_PATH/benchmark/run_test.sh"
    [[ -n "$1" ]] && cmd+=" --report-path $1"
    [[ -n "$2" ]] && cmd+=" --subdir $2"
    eval "$cmd"
    deactivate_venv
}

run_mindieclient_dt() {
    activate_venv
    local cmd="bash $TEST_PATH/mindie_client/run_test.sh"
    [[ -n "$1" ]] && cmd+=" --report-path $1"
    [[ -n "$2" ]] && cmd+=" --subdir $2"
    eval "$cmd"
    deactivate_venv
}

run_component_tests() {
    local component=$1
    local report_path=$2
    local subdir=$3
    case "$component" in
        benchmark)
            run_benchmark_dt "$report_path" "$subdir"
            ;;
        mindieclient)
            run_mindieclient_dt "$report_path" "$subdir"
            ;;
        all)
            run_benchmark_dt "$report_path" "$subdir"
            run_mindieclient_dt "$report_path" "$subdir"
            ;;
        *)
            echo "Unknown Component: $component"
            echo "Available Components: benchmark, mindieclient, all"
            exit 1
            ;;
    esac
}

cleanup_reports() {
    if [ -d "${REPORT_PATH}" ]; then
        rm -rf "$REPORT_PATH"
    fi
    mkdir "$REPORT_PATH"
}

show_help() {
    echo "Usage: $0 --component [benchmark|mindieclient|all] [--subdir SUBDIR] [--report-path REPORT_PATH]"
    echo
    echo "Options:"
    echo "  --component      Specify the component to test (default: all)"
    echo "  --subdir        Specify a subdirectory for test files (optional)"
    echo "  --report-path   Specify the path to save reports (optional, default is current script directory)"
}

initialize_vars() {
    COMPONENT="all"
    REPORT_PATH="${TEST_PATH}/reports"
}

parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --component)
                COMPONENT="$2"
                shift 2
                ;;
            --report-path)
                REPORT_PATH="$2"
                shift 2
                ;;
            --subdir)
                SUBDIR="$2"
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

check_dt_executed() {
    # 定义文件路径
    FILE="tests/reports/report_mindieclient/coverage.xml"

    # 检查文件是否存在
    if [ ! -f "$FILE" ]; then
        # 如果第二个文件不存在，输出错误信息
        echo "====================== ERRORS ======================"
        echo "$FILE not found"
        exit 1
    fi

    # 如果文件存在，输出成功信息
    echo "File exists: $FILE"
}

main() {
    initialize_vars
    parse_args "$@"

    if [[ "$COMPONENT" != "benchmark" && "$COMPONENT" != "mindieclient" && "$COMPONENT" != "all" ]]; then
        echo "Error: Component '$COMPONENT' is invalid."
        show_help
        exit 1
    fi

    # 首次运行需要安装依赖
    install_env
    
    # 删除reports路径前需要路径校验 
    # cleanup_reports "$REPORT_PATH"

    # 构造参数数组
    local params=("$COMPONENT")
    [[ -n "$REPORT_PATH" ]] && params+=("$REPORT_PATH")
    [[ -n "$SUBDIR" ]] && params+=("$SUBDIR")

    run_component_tests "${params[@]}"

    check_dt_executed
}

main "$@"

