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

__all__ = [
    "MindIEHTTPClient",
    "Input",
    "RequestedOutput",
    "Result",
    "AsyncRequest",
    "MindIEClientException",
]

try:
    from .client import AsyncRequest, MindIEHTTPClient
    from .input_requested_output import Input, RequestedOutput
    from .result import Result
    from .utils import MindIEClientException
except ModuleNotFoundError as error:
    raise RuntimeError("Module not Found") from error
