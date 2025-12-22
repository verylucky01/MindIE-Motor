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

path="${BASH_SOURCE[0]}"

if [[ -f "$path" ]] && [[ "$path" =~ set_env\.sh ]];then
    mindie_path=$(cd $(dirname $path); pwd )

    if [[ -f "${mindie_path}/latest/version.info" ]];then
    	if [[ -f "${mindie_path}/latest/mindie-service/set_env.sh" ]];then
			source ${mindie_path}/latest/mindie-service/set_env.sh
		else
			echo "mindie-service package is incomplete please check it."
		fi

    	if [[ -f "${mindie_path}/latest/mindie-llm/set_env.sh" ]];then
			source ${mindie_path}/latest/mindie-llm/set_env.sh
		else
			echo "mindie-llm package is incomplete please check it."
		fi

    else
        echo "The package of mindie is incomplete, please check it."
    fi
else
    echo "There is no 'set_env.sh' to import"
fi
