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

import re
import json
import logging
from typing import Optional
from urllib.parse import urlparse

logger = logging.getLogger(__file__)
# 定义常量
INFER_RESPONSE_JSON = {
    "generated_text": "infer_token api mock generated_text",
    "details": {
        "finish_reason": "length",
        "generated_tokens": 20,
        "seed": 846930886
    }}

INFER_STREAM_RESPONSE = {"prefill_time": 45.54, "decode_time": None, "token": {"id": 626, "text": "am"}}

CORRECT_RESPONSE_JSON = {
    "id": "mock_id",
    "model_name": "llama2_7b",
    "model_version": None,
    "text_output": "MindIE Mock Output",
    "details": {"finish_reason": "eos_token", "generated_tokens": 1, "first_token_cost": 1, "decode_cost": [1],
                "batch_size": 1, "queue_wait_time": 1, "perf_stat": [[1, 1], [2, 2]]},
    "prefill_time": 1,
    "decode_time": 1
}

OPENAI_RESPONSE_JSON = {
    "id": "chatcmpl-123",
    "object": "chat.completion",
    "created": 1677652288,
    "model": "gpt-3.5-turbo-0613",
    "choices": [
        {
            "index": 0,
            "message": {
                "role": "assistant",
                "content": "\n\nHello there, how may I assist you today?"
            },
            "finish_reason": "eos_token"
        }
    ],
    "usage": {
        "prompt_tokens": 9,
        "completion_tokens": 12,
        "total_tokens": 21
    },
    "prefill_time": 1,
    "decode_time_arr": [1]
}

OPENAI_STREAM_RESPONSE = {"id": "1", "object": "chat.completion.chunk", "created": 1707275960, "model": "baichuan",
                          "choices": [
                              {"index": 0, "delta": {"role": "assistant", "content": "am"}, "finish_reason": None}]
}

ERROR_RESPONSE_JSON = {
    "error": "Not Found"
}

ENCODING = 'utf-8'


class MockResponse:
    def __init__(self, status_code: int, json_data: dict, req_id: Optional[str] = None,
                 stream_data: Optional[dict] = None):
        self.status: int = status_code
        self.status = status_code
        self.data = json.dumps(json_data).encode(ENCODING)

        if req_id:
            self.replace_id(req_id)

        if stream_data:
            self.stream_data = json.dumps(stream_data).encode(ENCODING)
        else:
            self.stream_data = None

    def replace_id(self, new_id):
        # replace id field in data and stream data
        if new_id is None:
            return self
        data_json = json.loads(self.data.decode(ENCODING))
        data_json['id'] = new_id
        self.data = json.dumps(data_json).encode(ENCODING)

        if self.stream_data:
            stream_data_json = json.loads(self.stream_data.decode(ENCODING))
            stream_data_json['id'] = new_id
            self.stream_data = json.dumps(stream_data_json).encode(ENCODING)

        return self

    def set_stream(self):
        # add 'data:' suffix to stream data line
        self.stream_data = 'data: ' + self.stream_data.decode(ENCODING) + '\n'
        self.stream_data = self.stream_data.encode(ENCODING)
        return self

    def stream(self):
        # response.stream returns a iterable type
        return [self.stream_data]


def live_response():
    return MockResponse(200, CORRECT_RESPONSE_JSON)


def ready_response():
    return MockResponse(200, CORRECT_RESPONSE_JSON)


def health_response():
    return MockResponse(200, CORRECT_RESPONSE_JSON)


def generate_response(req_id):
    return MockResponse(200, CORRECT_RESPONSE_JSON).replace_id(req_id)


def infer_token_response():
    return MockResponse(200, INFER_RESPONSE_JSON, stream_data=INFER_STREAM_RESPONSE)


def generate_stream_response(req_id):
    return MockResponse(200, CORRECT_RESPONSE_JSON, stream_data=CORRECT_RESPONSE_JSON).replace_id(req_id).set_stream()


def completions_response(req_id):
    return MockResponse(200, OPENAI_RESPONSE_JSON, stream_data=OPENAI_STREAM_RESPONSE).replace_id(req_id).set_stream()


def not_found_response():
    return MockResponse({"error": "Not Found"}, 404)


# base on different api, return mock response accordingly
def server_mocker(method: str, url: str, body=None, fields=None, headers=None, **urlopen_kw):
    parsed_url = urlparse(url)
    # Get the path part, e.g., /api/data -> data
    path_suffix = '/' + parsed_url.path.split('/')[-1]
    req_id = None

    # Try extracting 'id' from the body if present
    if body is not None:
        try:
            body_json_str = body.decode(ENCODING)
            request_body = json.loads(body_json_str)
            req_id = request_body.get("id")
        except Exception as e:
            logger.info(e)

    # Define a mapping of paths to response handlers
    response_map = {
        re.compile(r".*/live"): live_response,  # Match any path starting with /live
        re.compile(r".*/ready"): ready_response,  # Match any path starting with /ready
        re.compile(r".*/health"): health_response,  # Match any path starting with /health
        re.compile(r".*/infer_token"): infer_token_response,  # Match any path starting with /infer_token
        # Match /generate_stream and any suffix
        re.compile(r".*/generate_stream"): lambda: generate_stream_response(req_id),
        # Match '/generate' but make sure it does not match '/generate_stream'
        re.compile(r".*/generate(?!_stream)"): lambda: generate_response(req_id),
        re.compile(r".*/completions"): lambda: completions_response(req_id),  # Match /completions and any suffix
    }
    
    # Check for matching pattern in the path
    for pattern, response_func in response_map.items():
        if pattern.match(parsed_url.path):
            return response_func()

    # Default response if no match is found
    return not_found_response()
