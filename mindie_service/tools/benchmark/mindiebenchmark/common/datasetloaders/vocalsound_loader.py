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
from typing import Union
from .base_loader import BaseLoader


MINDIE_GET_QWEN2_AUDIO_QUERIES = "mindie_get_qwen2_audio_queries"
OPENAI_GET_QWEN2_AUDIO_QUERIES = "openai_get_qwen2_audio_queries"

TYPE = "type"
TEXT = "text"
AUDIO_URL = "audio_url"
URL = "url"


class VocalsoundLoader(BaseLoader):
    def __init__(self, command_line_args: dict, *args):
        super().__init__(command_line_args, *args)
        self.model_name = command_line_args.get("ModelName")
        self.test_type = command_line_args.get("TestType")
        self.client_map = {
            "client": "mindie_func_map",
            "openai": "openai_func_map",
        }
        self.mindie_func_map = {
            "qwen2_audio": MINDIE_GET_QWEN2_AUDIO_QUERIES,
        }
        self.openai_func_map = {
            "qwen2_audio": OPENAI_GET_QWEN2_AUDIO_QUERIES,
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
    def mindie_get_qwen2_audio_queries(file_path, prompt):
        data = [
                {TYPE: AUDIO_URL, AUDIO_URL: file_path},
                {TYPE: TEXT, TEXT: prompt}
        ]
        return data


    def openai_get_qwen2_audio_queries(self, file_path, prompt):
        audio_base64 = self.encode_base64_content_from_local_path(file_path)
        data = [
                {TYPE: AUDIO_URL, AUDIO_URL: {URL: f"data:audio/x-wav;base64,{audio_base64}"}},
                {TYPE: TEXT, TEXT: prompt}
        ]
        return data
    
    def read_file(self, file_path) -> Union[str, dict]:
        self.gt_answer[str(self.id_count)] = file_path.split('.')[0].split('_')[-1]
        
        prompt_head = "In this audio, what kind of sound can you hear? "
        prompt_body1 = "A: Laughter, B: Sigh, C: Cough, D: Throat clearing, E: Sneeze, F: Sniff, "
        prompt_body2 = "Please select the one closest to the correct answer. ASSISTANT:"
        prompt = prompt_head + prompt_body1 + prompt_body2
        
        try:
            func_map_name = self.client_map[self.test_type]
            func_map = getattr(self, func_map_name)
        except NotImplementedError as e:
            raise NotImplementedError(f"TestType only support {func_map.keys()}.") from e
        try:
            func_name = func_map[self.model_name]
            func = getattr(self, func_name)
        except KeyError as e:
            raise KeyError(f"Unsupported model for {func_map_name} client!"
                                        f" Please choose from [{func_map.keys()}].") from e

        data = func(file_path, prompt)
        self.put_data_dict(data)
        
    def get_file_posix(self) -> str:
        return self.WAV_POSIX