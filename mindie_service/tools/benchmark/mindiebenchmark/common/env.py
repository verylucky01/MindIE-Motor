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
from mindiebenchmark.common.logging import Logger, LogParams
from mindiebenchmark.common.config import Config


def init_logger():
    def load_log_env(env_type: str):
        env_value = f'MINDIE_{env_type}'
        log_env = None
        if env_value in os.environ:
            log_env = os.getenv(env_value)
        env_values = dict(env_value=log_env, default_value=Config.config.get(env_type))
        return env_values
    log_params = LogParams()
    # set log path
    log_path_env = load_log_env(env_type="LOG_PATH")
    log_params.path = Logger.get_benchmark_log_string_from_env(log_path_env, "LOG_PATH")
    # set to file
    log_to_file_env = load_log_env(env_type="LOG_TO_FILE")
    log_params.to_file = Logger.get_benchmark_log_bool_from_env(log_to_file_env, "LOG_TO_FILE")
    # set to stdout
    log_to_stdout_env = load_log_env(env_type="LOG_TO_STDOUT")
    log_params.to_console = Logger.get_benchmark_log_bool_from_env(log_to_stdout_env, "LOG_TO_STDOUT")
    # set log level
    log_level_env = load_log_env(env_type="LOG_LEVEL")
    log_params.level = Logger.get_benchmark_log_string_from_env(log_level_env, "LOG_LEVEL")
    # set log verbose option
    log_verbose_env = load_log_env(env_type="LOG_VERBOSE")
    log_params.verbose = Logger.get_benchmark_log_bool_from_env(log_verbose_env, "LOG_VERBOSE")
    # set log rotate options
    log_rotate_env = load_log_env(env_type="LOG_ROTATE")
    log_params.rotate_options = Logger.get_benchmark_log_rotate_from_env(log_rotate_env, "LOG_ROTATE")
    Logger(log_params)