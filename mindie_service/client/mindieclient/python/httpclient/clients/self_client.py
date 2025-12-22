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

import numpy as np

from python.httpclient.clients.base_client import StreamClientMindIE, MindIEBaseClient
from python.httpclient.utils import raise_error, check_response


class SelfTokenStreamClient(StreamClientMindIE):
    @staticmethod
    def validate_input(inputs):
        if not isinstance(inputs, np.ndarray):
            raise_error("[MIE02E000102] After data preprocessing, input token should be a ndarray!")
        if inputs.shape[-1] < 1 or inputs.shape[-1] > 512000:
            raise_error("[MIE02E000102] The length of prompt should be in range [1, 512000], "
                        "but got {}!".format(inputs.shape[-1]))

    @staticmethod
    def preprocess_cur_line(cur_line: str) -> str:
        if cur_line == "engine callback timeout.":
            raise_error(f"[MIE02E000407] Engine time out. Token generation might be incomplete.")
        return cur_line

    def request_uri(self, model_name: str) -> str:
        return "/infer_token"

    def construct_request_body(
        self,
        model_name,
        inputs,
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ) -> dict:
        return {
            "input_id": inputs[0].tolist(),
            "stream": True,
            "parameters": parameters,
        }

    def process_stream_line(self, json_content: dict) -> dict:
        token_str = "token"
        return {
            token_str: json_content[token_str]
        }
