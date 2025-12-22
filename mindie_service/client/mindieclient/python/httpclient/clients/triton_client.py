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

import urllib.parse
from typing import List

import gevent

from mindieclient.python.httpclient.utils import check_response, raise_error
from mindieclient.python.httpclient.result import Result
from mindieclient.python.httpclient.input_requested_output import Input, RequestedOutput

from .base_client import MindIEBaseClient, StreamClientMindIE


def generate_infer_request_body(request_id, inputs: List[Input], outputs: List[RequestedOutput], parameters) -> dict:
    return {
        "id": request_id,
        "inputs": [cur_input.get_input_tensor() for cur_input in inputs],
        "outputs": [cur_output.get_output_tensor() for cur_output in outputs],
        "parameters": parameters,
    }


class TritonTextClient(MindIEBaseClient):
    def request_uri(self, model_name):
        return f"/v2/models/{model_name}/generate"

    def construct_request_body(
        self,
        model_name,
        inputs,
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ) -> dict:
        return {
            "id": request_id,
            "text_input": inputs,
            "parameters": parameters,
        }

    def process_response(self, response, last_time_point):
        check_response(response)
        return Result(response.data, self._verbose)


class TritonStreamClient(StreamClientMindIE):
    @staticmethod
    def preprocess_cur_line(cur_line: str) -> str:
        if cur_line == "engine callback timeout.":
            raise_error(f"[MIE02E000407] Engine time out. Token generation might be incomplete.")
        return cur_line

    def request_uri(self, model_name: str) -> str:
        return f"/v2/models/{model_name}/generate_stream"

    def construct_request_body(
        self,
        model_name,
        inputs,
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ) -> dict:
        return {
            "id": request_id,
            "text_input": inputs,
            "parameters": parameters,
            'details': True,
        }

    def process_stream_line(self, json_content: dict) -> dict:
        text_output_str = "text_output"
        details_str = "details"
        response_dict = {text_output_str: json_content[text_output_str]}
        if details_str in json_content:
            response_dict["batch_size"] = json_content[details_str]["batch_size"]
            response_dict["queue_wait_time"] = json_content[details_str]["queue_wait_time"]
            response_dict["generated_tokens"] = json_content[details_str]["generated_tokens"]
        return response_dict


class TritonTokenInferTextClient(MindIEBaseClient):
    @staticmethod
    def validate_input(inputs):
        if len(inputs) != 1:
            raise_error("The size of inputs only supports 1!")

    def request_uri(self, model_name: str) -> str:
        return f"/v2/models/{model_name}/infer"

    def construct_request_body(
        self,
        model_name,
        inputs,
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ) -> dict:
        return generate_infer_request_body(request_id, inputs, outputs, parameters)

    def process_response(self, response, last_time_point):
        check_response(response)
        return Result(response.data, self._verbose)


class TritonTokenAsyncInferTextClient(MindIEBaseClient):
    @staticmethod
    def validate_input(inputs):
        if len(inputs) != 1:
            raise_error("The size of inputs only supports 1!")

    def request_uri(self, model_name: str) -> str:
        return f"/v2/models/{model_name}/infer"

    def construct_request_body(
        self,
        model_name,
        inputs,
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ) -> dict:
        return generate_infer_request_body(request_id, inputs, outputs, parameters)

    def do_request(
        self,
        request_uri: str,
        request_body: dict,
        request_method: str = "POST",
        is_stream: bool = True
    ):
        def post_func(uri, body):
            return self._post(uri, body)

        response = self._pool.apply_async(post_func,
                                          (urllib.parse.urljoin(str(self.valid_url), request_uri), request_body))
        response.start()
        try:
            result = response.get()
            return result
        except Exception as e:
            raise e

    def process_response(self, response, last_time_point):
        from mindieclient.python.httpclient import AsyncRequest
        return AsyncRequest(response, self._verbose)
