#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
from setuptools import setup, find_packages

os.environ['SOURCE_DATE_EPOCH'] = '0'
setup(
    name='node_manager',
    version='0.0.1',
    description='node health manager',
    packages=find_packages(),
    python_requires=">=3.10",
    entry_points={
        'console_scripts': [
            'node_manager=node_manager.node_manager_run:main',
        ],
    }
)