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

import sys
import time
import logging
import numpy as np


logger_screen = logging.getLogger("benchmark_screen")
logger_screen.setLevel(logging.INFO)
ch = logging.StreamHandler(sys.stdout)
logger_screen.addHandler(ch)


def sample_one_value(method: str, params: dict) -> int:
    # Sample one value, the args have been checked before
    min_value = params["MinValue"]
    max_value = params["MaxValue"]
    if method == "uniform":
        value = np.random.uniform(min_value, max_value)
    elif method == "gaussian":
        mean = params["Mean"]
        stddev = np.sqrt(params["Var"])
        value = np.random.normal(mean, stddev)
        value = np.clip(value, min_value, max_value)
    elif method == "zipf":
        alpha = params["Alpha"]
        value = np.random.zipf(alpha)
        value = np.clip(value, min_value, max_value)
    else:
        raise ValueError(f"Unknown method: {method}, method should be one of {{uniform, gaussian, zipf}}.")
    return int(value)


def get_time_stamp_ms() -> str:
    ct = time.time()
    local_time = time.localtime(ct)
    date_s = time.strftime("%Y%m%d%H%M%S", local_time)
    date_ms = (ct - int(ct)) * 1000
    time_stmap = "%s%03d" % (date_s, date_ms)
    return time_stmap


def print_to_screen(msg: str):
    """Use logger to print on screen instead of print method
    """
    logger_screen.info(msg)


def print_value_warn(value_type, env_value, default_value, support_choices=None):
    msg = f"[WARNING] Invalid value of benchmark in MINDIE_{value_type}: {env_value}. "
    if support_choices:
        msg += f"Only {support_choices} support. "
    msg += f"benchmark in MINDIE_{value_type} will be set default to {default_value} "
    logger_screen.warning(msg)