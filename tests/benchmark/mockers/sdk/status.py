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

from enum import Enum


class Code(Enum):
    OK = 0
    ERROR = 1
    INVALID_ARG = 2
    NOT_FOUND = 3


class Status:
    def __init__(self, code: Code, msg: str) -> None:
        self.code = code
        self.msg = msg
    
    def is_ok(self):
        return Code.OK.value == self.code.value
    
    def get_code(self):
        return self.code
    
    def get_msg(self):
        return self.msg