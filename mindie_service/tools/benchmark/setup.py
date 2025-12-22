#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import sys
import argparse
from setuptools import setup, find_packages

os.environ["SOURCE_DATE_EPOCH"] = "0"
# 创建一个参数解析实例
parser = argparse.ArgumentParser(description="Setup Parameters")
parser.add_argument("--setup_cmd", type=str, default="bdist_wheel")
parser.add_argument("--version", type=str, default="1.0.RC1")

# 开始解析
args = parser.parse_args()
# 把路径参数从系统命令中移除，只保留setup需要的参数
sys.argv = [sys.argv[0], args.setup_cmd]
print(sys.argv)

mindie_benchmark_version = args.version

setup(
    name="mindiebenchmark",
    version=mindie_benchmark_version,
    author="ascend",
    description="build wheel for mindie benchmark",
    setup_requires=[],
    zip_safe=False,
    python_requires=">=3.9",
    install_requires=[],
    include_package_data=True,
    packages=find_packages(),
    entry_points={
        'console_scripts': [
            'benchmark=mindiebenchmark.benchmark_run:main'
        ],
    },
    package_data={
        'mindiebenchmark': ['config/config.json', 'config/synthetic_config.json', 'requirements.txt']
    }
)
