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

import json

from .base_client import MindIEBaseClient, StreamClientMindIE

OPENAI_CHAT_URI = "/v1/chat/completions"
CHOICES_DIR = "choices"
INDEX = "index"
DELTA = "delta"
REASONING_CONTENT = "reasoning_content"
CONTENT = "content"


def construct_openai_chat_request_body(model_name: str, prompt: str, is_stream: bool, parameters: dict) -> dict:
    request_body = {
        "model": model_name,
        "messages": [{
            "role": "user",
            "content": prompt
        }],
        "stream": is_stream
    }
    request_body.update(parameters)
    return request_body


class OpenAIChatTextClient(MindIEBaseClient):
    def request_uri(self, model_name):
        return OPENAI_CHAT_URI

    def construct_request_body(
        self,
        model_name,
        inputs,
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ):
        return construct_openai_chat_request_body(model_name, inputs, False, parameters)

    def process_response(self, response, last_time_point):
        return response.data.decode()


class OpenAIChatStreamClient(StreamClientMindIE):
    @staticmethod
    def preprocess_cur_line(cur_line: str) -> str:
        if "\ndata" in cur_line:
            end_ix = cur_line.find("data: [DONE]")
            cur_line = cur_line if end_ix < 0 else cur_line[:end_ix]
            data_blocks = cur_line.strip().split('\n\n')
            merged_data = None
            for block in data_blocks:
                # 去掉 "data: " 前缀并解析 JSON
                json_str = block.replace('data: ', '')
                data = json.loads(json_str)

                # 如果是第一条数据，初始化 merged_data
                if merged_data is None:
                    merged_data = data
                else:
                    # 合并 choices
                    merged_data['choices'].extend(data['choices'])
            return json.dumps(merged_data)
        else:
            end_ix = cur_line.find("data: [DONE]")
            return cur_line if end_ix < 0 else cur_line[:end_ix]

    def request_uri(self, model_name):
        return OPENAI_CHAT_URI

    def construct_request_body(self, model_name, inputs, request_id="", parameters: dict = None, outputs=None):
        return construct_openai_chat_request_body(model_name, inputs, True, parameters)

    def process_stream_line(self, json_content: dict) -> dict:
        output = {}
        content_output = {}
        reasoning_output = {}
        logprobs = {}
        batch_size = None
        queue_wait_time = None
        logprobs_str = "logprobs"
        usage_str = "usage"
        result = dict()
        for i in range(len(json_content[CHOICES_DIR])):
            full_text = ""
            if CONTENT in json_content[CHOICES_DIR][i][DELTA]:
                content_output[str(json_content[CHOICES_DIR][i][INDEX])] = \
                    json_content[CHOICES_DIR][i][DELTA][CONTENT]
                full_text += json_content[CHOICES_DIR][i][DELTA][CONTENT]
            if REASONING_CONTENT in json_content[CHOICES_DIR][i][DELTA]:
                reasoning_output[str(json_content[CHOICES_DIR][i][INDEX])] = \
                    json_content[CHOICES_DIR][i][DELTA][REASONING_CONTENT]
                full_text += json_content[CHOICES_DIR][i][DELTA][REASONING_CONTENT]
            output[str(json_content[CHOICES_DIR][i][INDEX])] = full_text

        result.update({"text_output": output})
        if len(content_output) != 0:
            result.update({CONTENT: content_output})
        if len(reasoning_output) != 0:
            result.update({REASONING_CONTENT: reasoning_output})

        if usage_str in json_content:
            batch_size = json_content[usage_str]["batch_size"]
            queue_wait_time = json_content[usage_str]["queue_wait_time"]
            result.update({"batch_size": batch_size, "queue_wait_time": queue_wait_time})

        if len(json_content[CHOICES_DIR]) > 0 and logprobs_str in json_content[CHOICES_DIR][0].keys():
            for i in range(len(json_content[CHOICES_DIR])):
                logprobs[str(json_content[CHOICES_DIR][i][INDEX])] = json_content[CHOICES_DIR][i][logprobs_str]
            result.update({"logprobs": logprobs})
        return result


class OpenAITextClient(MindIEBaseClient):
    def request_uri(self, model_name: str) -> str:
        return "/v1/completions"

    def construct_request_body(
        self,
        model_name,
        inputs,
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ) -> dict:
        if parameters is None:
            parameters = {}

        parameters.update({
            "prompt": inputs,
            "model": model_name,
            "stream": False,
        })
        return parameters

    def process_response(self, response, last_time_point):
        return response.data.decode()


class OpenAIStreamClient(StreamClientMindIE):
    @staticmethod
    def preprocess_cur_line(cur_line: str) -> str:
        end_ix = cur_line.find("data: [DONE]")
        return cur_line if end_ix < 0 else cur_line[:end_ix]

    def request_uri(self, model_name: str) -> str:
        return "/v1/completions"

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
            "stream": True,
        })
        return parameters

    def process_stream_line(self, json_content: dict) -> dict:
        return json_content
