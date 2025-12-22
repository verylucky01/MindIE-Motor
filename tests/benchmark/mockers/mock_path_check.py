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

import unittest
from unittest.mock import patch, MagicMock
from typing import Tuple
from mindiebenchmark.common.file_utils import PathCheck


class PathCheckMocker:
    def __init__(self):
        self.mock_check_path_valid = MagicMock(return_value=(True, ""))
        self.mock_check_path_link = MagicMock(return_value=(True, ""))
        self.mock_check_path_exists = MagicMock(return_value=(True, ""))
        self.mock_check_path_owner_group_valid = MagicMock(return_value=(True, ""))
        self.mock_check_file_size = MagicMock(return_value=(True, ""))

    @staticmethod
    def stop_patching(self):
        patch.stopall()

    def mock_methods(self):
        patch.object(PathCheck, 'check_path_valid', self.mock_check_path_valid).start()
        patch.object(PathCheck, 'check_path_link', self.mock_check_path_link).start()
        patch.object(PathCheck, 'check_path_exists', self.mock_check_path_exists).start()
        patch.object(PathCheck, 'check_path_owner_group_valid', self.mock_check_path_owner_group_valid).start()
        patch.object(PathCheck, 'check_file_size', self.mock_check_file_size).start()
