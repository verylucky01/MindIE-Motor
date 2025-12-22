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


options=$(getopt -o r:d:k:p --long data_file:,result_file:,k:,print -- "$@")
eval set -- "$options"

#extract parameters
while true; do
  case $1 in
    -r | --result_file) shift ; result_file=$1 ; shift ;;
    -d | --data_file) shift ; data_file=$1 ; shift ;;
    -k | --k) shift ; k=$1 ; shift ;;
    -p | --print) shift ; print=true; shift ;;
    --) shift ; break ;;
    *) echo "Invalid option: $1" exit 1;;
  esac
done

#check parameters
if [ -z "$result_file" ]; then
  echo "Error: result_file is required!"
  exit 1
fi
if [ -z "$data_file" ]; then
  echo "Error: data_file is required!"
  exit 1
fi
if [ -z "$k" ]; then
  echo "default k:[1,10,100]!"
  k=[1,10,100]
fi

#print parameters
if [ "$print" = true ]; then
  echo "data_file:$data_file;  result_file:$result_file;  k:$k"
fi

#human eval test code path at atb model, set_env first
human_eval_dir=$ATB_SPEED_HOME_PATH/tests/modeltest/dataset/full/HumanEval
modeltest_dir=$ATB_SPEED_HOME_PATH/tests/modeltest
nohup python3 -c "import sys; sys.setrecursionlimit(10000); \
sys.path.append('$human_eval_dir'); sys.path.append('$modeltest_dir'); \
import human_eval; human_eval.evaluate_functional_correctness(sample_file='$result_file', \
problem_file='$data_file', k=$k)" >human_eval_acc.log 2>&1 &