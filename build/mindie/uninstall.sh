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

VERSION=replace_version
LOG_PATH=replace_log_path
LOG_NAME=replace_log_name
OPERATION_LOG_NAME=operation.log
OPERATION_LOG_PATH=${HOME}/log/mindie/
whl_uninstall_flag="y"

file_dir=$(dirname $(readlink -f $0))

if [ ! "$UID" = "0" ]; then
    cur_owner=$(whoami)
    LOG_PATH="/home/${cur_owner}${LOG_PATH}"
fi
log_file=${LOG_PATH}${LOG_NAME}
operation_logfile=${OPERATION_LOG_PATH}${OPERATION_LOG_NAME}

function log() {
    if [ "x$log_file" = "x" ] || [ ! -f "$log_file" ]; then
        echo -e "[mindie] [$(date +%Y%m%d-%H:%M:%S)] [$1] $2"
    else
        echo -e "[mindie] [$(date +%Y%m%d-%H:%M:%S)] [$1] $2" >>$log_file
    fi
}

function make_file() {
    log "INFO" "touch ${1}"
    touch ${1} 2>/dev/null
    if [ $? -ne 0 ]; then
        print "ERROR" "create $1 failed !"
        exit 1
    fi
}

function chmod_recursion() {
    local rights=$2
    if [ "$3" = "dir" ]; then
        find $1 -type d -exec chmod ${rights} {} \; 2>/dev/null
    elif [ "$3" = "file" ]; then
        find $1 -type f -name $4 -exec chmod ${rights} {} \; 2>/dev/null
    fi
}

function log_init() {
    if [ ! -d "$LOG_PATH" ]; then
        make_dir "$LOG_PATH"
    fi

    if [ ! -f "$log_file" ]; then
        make_file "$log_file"
        chmod_recursion ${LOG_PATH} "750" "dir"
        chmod 640 ${log_file}
    else
        local filesize=$(ls -l $log_file | awk '{ print $5}')
        local maxsize=$((1024*1024*50))
        if [ $filesize -gt $maxsize ]; then
            local log_file_move_name="mindie_install_bak.log"
            mv -f ${log_file} ${LOG_PATH}${log_file_move_name}
            chmod 440 ${LOG_PATH}${log_file_move_name}
            make_file "$log_file"
            chmod 640 ${log_file}
            log "INFO" "log file > 50M, move ${log_file} to ${LOG_PATH}${log_file_move_name}."
        fi
    fi

    if [ ! -d "$OPERATION_LOG_PATH" ]; then
        make_dir "$OPERATION_LOG_PATH"
    fi
    if [ ! -f "$operation_logfile" ]; then
        make_file "$operation_logfile"
        chmod_recursion ${OPERATION_LOG_PATH} "750" "dir"
        chmod 640 ${operation_logfile}
    else
        local filesize=$(ls -l $operation_logfile | awk '{ print $5}')
        local maxsize=$((1024*1024*50))
        if [ $filesize -gt $maxsize ]; then
            local operation_logfile_move_name="operation_bak.log"
            mv -f ${operation_logfile} ${OPERATION_LOG_PATH}${operation_logfile_move_name}
            chmod 440 ${OPERATION_LOG_PATH}${operation_logfile_move_name}
            make_file "$operation_logfile"
            chmod 640 ${operation_logfile}
            log "INFO" "log file > 50M, move ${operation_logfile} to \
                ${OPERATION_LOG_PATH}${operation_logfile_move_name}."
        fi
    fi
    print "INFO" "Uninstall log save in ${log_file}"
    print "INFO" "Operation log save in ${operation_logfile}"
}

function print() {
    if [ "x$log_file" = "x" ] || [ ! -f "$log_file" ]; then
        echo -e "[mindie] [$(date +%Y%m%d-%H:%M:%S)] [$1] $2"
    else
        echo -e "[mindie] [$(date +%Y%m%d-%H:%M:%S)] [$1] $2" | tee -a $log_file
    fi
}

function log_uninstall_operation() {
    if [ ! -f "${operation_logfile}" ]; then
        touch "${operation_logfile}"
        chmod 640 "${operation_logfile}"
    fi
    echo "[mindie] [${file_dir}/uninstall.sh][$(date +%Y%m%d-%H:%M:%S)] [$(whoami)] [$1] $2" >> "$operation_logfile"
}

function get_python_version() {
    py_version=$(python3 -c 'import sys; print(sys.version_info[0], ".", sys.version_info[1])' | tr -d ' ')
    py_major_version=${py_version%%.*}
    py_minor_version=${py_version##*.}
}

function pip_uninstall() {
    get_python_version
    if [[ "$py_major_version" == "3" ]] && { [[ "$py_minor_version" == "10" ]] || [[ "$py_minor_version" == "11" ]]; }; then
        python_interpreter="python3.$py_minor_version"
        print "INFO" "Current Python Interpreter: ${python_interpreter}"
    else
        print "ERROR" "MindIE unstall failed, please install Python3.10 or Python3.11 first"
        install_failed_process
        exit 1
    fi

    $python_interpreter -m pip uninstall $1 -y --log-file ${log_file}
    if test $? -ne 0; then
        print "ERROR" "Uninstall $1 whl package failed, detail info can be checked in ${log_file}."
        whl_uninstall_flag="n"
    else
        print "INFO" "Uninstall $1 whl package success."
    fi
}

function uninstall_mindie_rt_whl() {
    pip_uninstall ascendie
}

function uninstall_mindie_torch_whl() {
    pip_uninstall mindietorch
}

function uninstall_mindie_service_whl() {
    pip_uninstall mindieclient
    pip_uninstall mindiebenchmark
    pip_uninstall ock-hntc-mix
    pip_uninstall infer-engine
    pip_uninstall model_wrapper
    pip_uninstall mies_tokenizer
}

function uninstall_mindie_llm_whl() {
    pip_uninstall mindie-llm
}

function uninstall_mindie_sd_whl() {
    pip_uninstall mindiesd
}

function main() {
    log_init
    # Uninstall C++ interface
    CUR_DIR=$(dirname $(readlink -f $0))
    cd $CUR_DIR/../../
    if [ -d "${VERSION}" ]; then
        chmod -R 750 ${VERSION}
        [ -n "${VERSION}" ] && rm -rf $VERSION
        print "INFO" "Successfully uninstall C++ lib."
    else
        print "ERROR" "Can't find C++ lib directory! Failed to uninstall!"
        log_uninstall_operation "ERROR" "uninstall failed"
        exit 1
    fi

    if [[ -h "latest" && -f "set_env.sh" ]]; then
        rm -f latest set_env.sh
        print "INFO" "Successfully uninstall latest and set_env.sh."
    else
        print "ERROR" "Can't find latest and set_env.sh! Failed to uninstall!"
        exit 1
    fi

    # Delete `mindie/` if it's empty
    cd ../
    if [ -n "mindie" ] && [ ! "$(ls -A mindie)" ]; then
        rm -rf mindie
    fi

    # Uninstall Python interface
    print "INFO" "mindie whl package uninstall process start."
    uninstall_mindie_rt_whl
    uninstall_mindie_torch_whl
    uninstall_mindie_service_whl
    uninstall_mindie_llm_whl
    uninstall_mindie_sd_whl
    if [ "${whl_uninstall_flag}" == "y" ]; then
        print "INFO" "mindie whl package uninstall success"
        print "INFO" "finished uninstall Ascend-mindie_${VERSION}"
        log_uninstall_operation "INFO" "uninstall success"
        echo "------"
        echo "To take effect for current user, you can exec command : unset TUNE_BANK_PATH"
        echo "------"
    else
        print "ERROR" "mindie whl package uninstall not completely success"
        log_uninstall_operation "ERROR" "uninstall failed"
        exit 1
    fi
}

main
