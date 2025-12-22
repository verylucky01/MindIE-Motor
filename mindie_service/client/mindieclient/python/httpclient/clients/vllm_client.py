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

from .base_client import StreamClientMindIE


class VLLMClient(StreamClientMindIE):
    def request_uri(self, model_name: str) -> str:
        return "/generate"

    def construct_request_body(
        self,
        model_name,
        inputs,
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ) -> dict:
        parameters.update({
            "prompt": inputs,
            "model": model_name,
        })
        return parameters

    def process_stream_line(self, json_content: dict) -> dict:
        return json_content
