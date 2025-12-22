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

import base64
from io import BytesIO
from .base_loader import JsonlLoader


MINDIE_GET_COGVLM2_QUERIES = "mindie_get_cogvlm2_queries"
MINDIE_GET_INTERNVL_QUERIES = "mindie_get_internvl_queries"
MINDIE_GET_MINICPM_QUERIES = "mindie_get_minicpm_queries"
MINDIE_GET_PERFORMANCE_QUERIES = "mindie_get_performance_queries"
MINDIE_GET_QWEN_VL_QUERIES = "mindie_get_qwen_vl_queries"
MINDIE_GET_QWEN2_VL_QUERIES = "mindie_get_qwen2_vl_queries"
MINDIE_GET_VITA_QUERIES = "mindie_get_vita_queries"
MINDIE_GET_COMMON_QUERIES = "mindie_get_common_queries"

OPENAI_GET_INTERNVL_QUERIES = "openai_get_internvl_queries"
OPENAI_GET_QWEN2_VL_QUERIES = "openai_get_qwen2_vl_queries"
OPENAI_GET_COMMON_QUERIES = "openai_get_common_queries"

TYPE = "type"
TEXT = "text"
IMAGE_URL = "image_url"
URL = "url"


class TextVQALoader(JsonlLoader):
    def __init__(self, command_line_args: dict, *args):
        super().__init__(command_line_args, *args)
        self.model_name = command_line_args.get("ModelName")
        self.test_type = command_line_args.get("TestType")

        self.client_map = {
            "client": "mindie_func_map",
            "openai": "openai_func_map",
        }
        # Note! The name of the key may not match the model name in the value."
        self.mindie_func_map = {
            "cogvlm2": MINDIE_GET_COGVLM2_QUERIES,
            "glm4v": MINDIE_GET_INTERNVL_QUERIES,
            "internvl": MINDIE_GET_INTERNVL_QUERIES,
            "minicpm_qwen2_v2": MINDIE_GET_MINICPM_QUERIES,
            "performance": MINDIE_GET_PERFORMANCE_QUERIES,
            "qwen_vl": MINDIE_GET_QWEN_VL_QUERIES,
            "qwen2_vl": MINDIE_GET_QWEN2_VL_QUERIES,
            "vita": MINDIE_GET_VITA_QUERIES,
            "qwen2_5_vl": MINDIE_GET_QWEN2_VL_QUERIES,
            "qvq": MINDIE_GET_QWEN2_VL_QUERIES,
            "common": MINDIE_GET_COMMON_QUERIES,
        }
        self.openai_func_map = {
            "internvl": OPENAI_GET_INTERNVL_QUERIES,
            "qwen2_vl": OPENAI_GET_QWEN2_VL_QUERIES,
            "qwen2_5_vl": OPENAI_GET_QWEN2_VL_QUERIES,
            "qvq": OPENAI_GET_QWEN2_VL_QUERIES,
            "common": OPENAI_GET_COMMON_QUERIES,
        }

    @staticmethod
    def encode_base64_content_from_local_path(path: str) -> str:
        """Encode a content retrieved from a remote url to base64 format."""

        with open(path, 'rb') as f:
            data = f.read()
            data_bytes_io = BytesIO(data)
        result = base64.b64encode(data_bytes_io.read()).decode('utf-8')

        return result
    
    @staticmethod
    def mindie_get_cogvlm2_queries(line):
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: TEXT, TEXT: line["question"] + " Just tell me the answer in less than three words."},
                    {TYPE: IMAGE_URL, IMAGE_URL: line["image"]}
                ]
        }
        return queries
    
    @staticmethod
    def mindie_get_internvl_queries(line):
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: TEXT, TEXT: line["question"] + " Answer the question using a single word or phrase."},
                    {TYPE: IMAGE_URL, IMAGE_URL: line["image"]}
                ]
        }
        return queries

    @staticmethod
    def mindie_get_minicpm_queries(line):
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: IMAGE_URL, IMAGE_URL: line["image"]},
                    {TYPE: TEXT, TEXT: line["question"] + " Answer the question using a single word or phrase."}
                ]
        }
        return queries

    @staticmethod
    def mindie_get_performance_queries(line):
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: IMAGE_URL, IMAGE_URL: line["image"]},
                    {TYPE: TEXT, TEXT: line["question"]}
                ]
        }
        return queries

    @staticmethod
    def mindie_get_qwen_vl_queries(line):
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: IMAGE_URL, IMAGE_URL: line["image"]},
                    {TYPE: TEXT, TEXT: line["question"] + " Answer:"}
                ]
        }
        return queries
    
    @staticmethod
    def mindie_get_qwen2_vl_queries(line):
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: IMAGE_URL, IMAGE_URL: line["image"]},
                    {TYPE: TEXT, TEXT: line["question"] + " Answer the question using a single word or phrase."}
                ]
        }
        return queries
    
    @staticmethod
    def mindie_get_vita_queries(line):
        queries = {
            "id": line["question_id"],
            "data":
                [   
                    {"type": "image_url", "image_url": line["image"]},
                    {"type": "text", "text": line["question"] + " Answer the question using a single word or phrase."}  
                ]
        }
        return queries

    @staticmethod
    def mindie_get_common_queries(line):
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: IMAGE_URL, IMAGE_URL: line["image"]},
                    {TYPE: TEXT, TEXT: line["question"] + " Answer the question using a single word or phrase."}
                ]
        }
        return queries

    def openai_get_internvl_queries(self, line):
        image_base64 = self.encode_base64_content_from_local_path(line["image"])
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: TEXT, TEXT: line["question"] + " Answer the question using a single word or phrase."},
                    {TYPE: IMAGE_URL, IMAGE_URL: {URL: f"data:image/jpeg;base64,{image_base64}"}}
                ]
        }
        return queries

    def openai_get_qwen2_vl_queries(self, line):
        image_base64 = self.encode_base64_content_from_local_path(line["image"])
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: IMAGE_URL, IMAGE_URL: {URL: f"data:image/jpeg;base64,{image_base64}"}},
                    {TYPE: TEXT, TEXT: line["question"] + "  Answer the question using a single word or phrase."}
                ]
        }
        return queries

    def openai_get_common_queries(self, line):
        image_base64 = self.encode_base64_content_from_local_path(line["image"])
        queries = {
            "id": line["question_id"],
            "data":
                [
                    {TYPE: IMAGE_URL, IMAGE_URL: {URL: f"data:image/jpeg;base64,{image_base64}"}},
                    {TYPE: TEXT, TEXT: line["question"] + " Answer the question using a single word or phrase."}
                ]
        }
        return queries

    def read_line(self, line):
        try:
            func_map_name = self.client_map[self.test_type]
            func_map = getattr(self, func_map_name)
        except NotImplementedError as e:
            raise NotImplementedError(f"TestType only support {func_map.keys()}.") from e
        if self.model_name in func_map:
            func_name = func_map[self.model_name]
        else:
            func_name = func_map["common"]
        func = getattr(self, func_name)
        return func(line)
