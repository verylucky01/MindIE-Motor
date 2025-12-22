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
import queue
from io import BytesIO
from mindiebenchmark.common.results import ResultData
from .base_loader import MultiJsonLoader


MINDIE_GET_QUERIES = "mindie_get_queries"

OPENAI_GET_QUERIES = "openai_get_queries"

TYPE = "type"
TEXT = "text"
VIDEO_URL = "video_url"
URL = "url"


class VideoBenchLoader(MultiJsonLoader):
    def __init__(self, command_line_args: dict, data_queue: queue.Queue, result: ResultData):
        super().__init__(command_line_args, data_queue, result)
        self.model_name = command_line_args.get("ModelName")
        self.test_type = command_line_args.get("TestType")
        self.result = result
        self.client_map = {
            "client": "mindie_func_map",
            "openai": "openai_func_map",
        }
        self.mindie_func_map = {
            "internvl": MINDIE_GET_QUERIES,
            "qwen2_vl": MINDIE_GET_QUERIES,
            "minicpm_qwen2_v2": MINDIE_GET_QUERIES,
            "vita": MINDIE_GET_QUERIES,
            "common": MINDIE_GET_QUERIES,
        }
        self.openai_func_map = {
            "internvl": OPENAI_GET_QUERIES,
            "qwen2_vl": OPENAI_GET_QUERIES,
            "common": OPENAI_GET_QUERIES,
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
    def get_question(item):
        length = len(item["choices"])
        text_map = {
            2: 'two', 
            3: 'three', 
            4: 'four', 
            5: 'five', 
            6: 'six', 
        }

        if length not in text_map.keys():
            raise KeyError(f"The length of item[\"choices\"] must in {text_map.keys()}")
        
        question = item['question']
        if length == 6:
            question += "Choices: "
        else:
            question += " "
        choices_list = ['A', 'B', 'C', 'D', 'E', 'F']
        for i in range(length):
            choice = choices_list[i]
            question += (choice + '.' + item["choices"][choice] + ' ')
        op_list = ', '.join(choices_list[:length])
        question += (
        f'\n Among the {text_map.get(length)} options {op_list} above,'
        f' the one closest to the correct answer is:'
        )

        if length in [2, 3, 5]:
            question += " "

        question += (
            " Please respond with only the corresponding options and do not provide any explanations" 
            + " or additional information. ASSISTANT:"
        )
        return question
    
    @staticmethod
    def mindie_get_queries(vid_path, question):
        queries = [
            {TYPE: VIDEO_URL, VIDEO_URL: vid_path},
            {TYPE: TEXT, TEXT: question}
        ]
        return queries
    
    def openai_get_queries(self, vid_path, question):
        video_base64 = self.encode_base64_content_from_local_path(vid_path)
        queries = [
            {TYPE: VIDEO_URL, VIDEO_URL: {URL: f"data:video/mp4;base64,{video_base64}"}},
            {TYPE: TEXT, TEXT: question}
        ]
        return queries

    def read_line(self, line):
        qid, item = line
        vid_path = item['vid_path']
        sub_dataset_name = vid_path.split('/')[-2]
        self.gt_answer[self.id_count] = self.result.vb_gt_answer[sub_dataset_name][qid]["answer"]
        question = self.get_question(item)
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
        return func(vid_path, question)