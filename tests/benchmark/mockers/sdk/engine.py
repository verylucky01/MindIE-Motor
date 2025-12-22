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

from .status import Code, Status


class Engine:
    def __init__(self) -> None:
        pass
    
    @staticmethod
    def init() -> Status:
        return Status(Code.OK, "success")
    
    @staticmethod
    def get_request_block_quotas():
        return 1, 1, 1
    
    @staticmethod
    def get_processing_request():
        return 1
    
    @staticmethod
    def finalize():
        pass