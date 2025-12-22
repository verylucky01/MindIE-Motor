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

py_install_path="/root/.local/lib/python3.10/site-packages"
if [ "$UID" = "0" ]; then
    log_file=${LOG_PATH}${LOG_NAME}
else
    cur_owner=$(whoami)
    LOG_PATH="/home/${cur_owner}${LOG_PATH}"
    log_file=${LOG_PATH}${LOG_NAME}
    py_install_path="/home/${cur_owner}/.local/lib/python3.10/site-packages"
fi

# 将日志记录到日志文件
function log() {
    if [ "x$log_file" = "x" ] || [ ! -f "$log_file" ]; then
        echo -e "[mindie-service] [$(date +%Y%m%d-%H:%M:%S)] [$1] $2"
    else
        echo -e "[mindie-service] [$(date +%Y%m%d-%H:%M:%S)] [$1] $2" >>$log_file
    fi
}

# 创建文件
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

function print() {
    if [ "x$log_file" = "x" ] || [ ! -f "$log_file" ]; then
        make_file "$log_file"
        # 安装日志权限
        chmod_recursion ${LOG_PATH} "750" "dir"
        chmod 640 ${log_file}
        echo -e "[mindie-service] [$(date +%Y%m%d-%H:%M:%S)] [$1] $2" | tee -a $log_file
    else
        local filesize=$(ls -l $log_file | awk '{ print $5}')
        local maxsize=$((1024*1024*50))
        if [ $filesize -gt $maxsize ]; then
            local log_file_move_name="ascend_mindie_service_install_bak.log"
            mv -f ${log_file} ${LOG_PATH}${log_file_move_name}
            chmod 440 ${LOG_PATH}${log_file_move_name}
            make_file "$log_file"
            chmod 640 ${log_file}
            log "INFO" "log file > 50M, move ${log_file} to ${LOG_PATH}${log_file_move_name}."
        fi
        echo -e "[mindie-service] [$(date +%Y%m%d-%H:%M:%S)] [$1] $2" | tee -a $log_file
    fi
}

function overwrite_one_file() {
    # 对密钥证书文件，需要写入0、1、随机数3种情况至少3次，并且覆盖每种情形
    # 读取需要操作的文件
    local file_path="$1"

    # 获取文件大小（以字节为单位）
    local file_size
    file_size=$(stat --format=%s "$file_path")

    # 使用0进行覆盖
    for ((i=0; i<file_size; i++)); do
        printf '\x00' | dd of="$file_path" bs=1 seek=$i conv=notrunc 2> /dev/null
    done
    # 使用1进行覆盖
    for ((i=0; i<file_size; i++)); do
        printf '\x01' | dd of="$file_path" bs=1 seek=$i conv=notrunc 2> /dev/null
    done
    # 使用随机数进行覆盖
    for ((i=0; i<file_size; i++)); do
        dd if=/dev/urandom of="$file_path" bs=1 count="$file_size" conv=notrunc 2>/dev/null
    done
}

function overwrite_security_files() {
    print "INFO" "Overwrite security files start ..."
    # 对密钥证书文件，需要写入0、1、随机数3种情况至少3次，并且覆盖每种情形
    local base_path=$1
    if [ ! -d "$base_path" ]; then
        print "WARN" "Directory $base_path does not exist, ignore."
        return 1
    fi
    local directories=("ca" "certs" "keys" "pass")
    local maxsize=$((1024 * 1024 * 10))
    for dir in "${directories[@]}"; do
        local dir_path="$base_path/$dir"
        if [ -d "$dir_path" ]; then
            for file in "$dir_path"/*; do
                if [ -f "$file" ] && [[ "$file" == *key* ]] && [ $(stat -c%s "$file") -le $maxsize ]; then
                    chmod u+w $file
                    overwrite_one_file $file
                fi
            done
        else
            print "WARN" "Directory $dir_path does not exist."
            return 1
        fi
    done
    print "INFO" "Overwrite security files successfully."
}

function main() {
    # 卸载C++接口
    CUR_DIR=$(dirname $(readlink -f $0))

    security_file_path="$CUR_DIR/../../latest/mindie-service/security"
    overwrite_security_files $security_file_path

    cd $CUR_DIR/../../
    if [ -d "${VERSION}" ]; then
        chmod 750 ${VERSION}/scripts
        [ -n "${VERSION}" ] && rm -rf $VERSION
        print "INFO" "Successfully uninstall C++ lib."
    else
        print "ERROR" "Can't find C++ lib directory! Failed to uninstall!"
        exit 1
    fi

    if [[ -h "latest" ]]; then
        rm -f latest
        print "INFO" "Successfully uninstall latest."
    else
        print "ERROR" "Can't find latest! Failed to uninstall!"
        exit 1
    fi

    # 卸载python接口
    python3 -m pip uninstall mindiebenchmark --log-file ${log_file}
    python3 -m pip uninstall mindieclient --log-file ${log_file}
    python3 -m pip uninstall infer-engine --log-file ${log_file}
    python3 -m pip uninstall model_wrapper --log-file ${log_file}
    python3 -m pip uninstall mies_tokenizer --log-file ${log_file}

    print "INFO" "finished uninstall Ascend-cann-mindie-service_${VERSION}"
}

main
