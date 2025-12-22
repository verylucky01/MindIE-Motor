#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import argparse
import json
import os
import stat
import importlib.util

from mindieclient.python.common.file_utils import PathCheck, safe_open

CLIENT_PATH = importlib.util.find_spec("mindieclient").origin
BASE_DIRECTORY = os.path.dirname(CLIENT_PATH)


class Config:
    config = {}
    config_path = os.path.join(BASE_DIRECTORY, "python", "config", "config.json")

    @staticmethod
    def _check_file_permission(config_path, expected_permissions, path_name):
        ret, infos = PathCheck.check_path_full(config_path)
        if not ret:
            raise ValueError(infos)
        real_path = os.path.realpath(config_path)
        try:
            # 获取指定文件的属主
            file_stat = os.stat(real_path)
            file_permissions = file_stat.st_mode & 0xFFF
            # 将权限位转换为人类可读的格式
            permissions = stat.filemode(file_permissions)
            # 检查文件的权限位是否小于等于期望的权限位
            if file_permissions & (~expected_permissions & 0o777) != 0:
                raise ValueError(f"The file {real_path} does not meet the minimum permission "
                                 f"requirement {oct(expected_permissions)[2:]}, but got {oct(file_permissions)[2:]}")
        except os.error as e:
            raise ValueError(f"Failed to check permission: `{path_name}` of mindieclient") from e

    @classmethod
    def get_config(cls, key: str):
        return cls.config.get(key)

    @classmethod
    def load_config(cls, config_path):
        real_path = os.path.realpath(config_path)
        ret, infos = PathCheck.check_path_full(real_path)
        if not ret:
            raise ValueError(infos)
        expected_permissions = 0o640  # 定义配置文件权限位常量
        cls._check_file_permission(real_path, expected_permissions, 'ConfigPath')
        try:
            with safe_open(real_path, mode="r", encoding="utf-8") as file:
                loaded_config = json.load(file)
                cls.config.update(loaded_config["LogConfig"])
        except (FileNotFoundError, json.JSONDecodeError) as e:
            raise ValueError(f"Failed to load JSON config from {real_path}") from e

    @classmethod
    def validate_config(cls):
        # 校验配置项
        # 在这里根据自己的需求实现校验逻辑
        # 如果配置项不符合要求，可以抛出异常或者给出警告信息
        # JSON配置文件校验（这里只是示例，你可以根据需要添加更多校验逻辑）
        log_config_list = [
            "LOG_LEVEL",
            "LOG_PATH",
            "LOG_TO_FILE",
            "LOG_TO_STDOUT",
            "LOG_VERBOSE",
            "LOG_ROTATE",
        ]
        for log_config_type in log_config_list:
            if log_config_type not in cls.config:
                raise ValueError(f'JSON config is missing {log_config_type}, please check')
