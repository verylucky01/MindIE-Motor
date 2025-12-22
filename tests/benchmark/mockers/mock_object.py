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

from typing import Union, List, Optional
import numpy


class MockBenchmarkTokenizer:
    _init_flag = False

    @staticmethod
    def init(model_path: str, tokenizer_type: str = "transformers", **kwargs):
        MockBenchmarkTokenizer._init_flag = True
        
    @staticmethod
    def is_initialized() -> bool:
        return MockBenchmarkTokenizer._init_flag

    @staticmethod
    def reset_instance():
        MockBenchmarkTokenizer._init_flag = False

    @staticmethod
    def encode(text: str, tokenizer_time=None, add_special_tokens=True, **kwargs):
        if not MockBenchmarkTokenizer._init_flag :
            raise RuntimeError("Tokenizer has not been initialized. Call init first.")
        return [0]

    @staticmethod
    def decode(tokens, detokenizer_time=None, skip_special_tokens=False, **kwargs):
        if not MockBenchmarkTokenizer._init_flag :
            raise RuntimeError("Tokenizer has not been initialized. Call init first.")
        return "test"

    def _check_tokenzier_type(self, model_path):
        pass


class AutoTokenizerInputIdType():
    def __init__(self, input_ids: list):
        self.input_id = input_ids

    def astype(self, input_type: numpy.integer):
        return numpy.array(self.input_id)


class TokenizerModel():
    @staticmethod
    def __call__(self, text: Union[str, List[str]] = None,
                 return_tensors: Optional[Union[str]] = None) -> dict:
        return {"input_ids": AutoTokenizerInputIdType([0])}

    @staticmethod
    def encode(ques_string: str,
               add_special_tokens: bool) -> list:
        return [0]

    @staticmethod
    def decode(token_ids: list,
               skip_special_tokens: bool = False,
               clean_up_tokenization_spaces: bool = None, ) -> str:
        return "test"


class TritonCallbackObject():
    @staticmethod
    def as_numpy(required_name: str):
        if required_name == 'OUTPUT_IDS':
            return [0]
        elif required_name == 'IBIS_EOS_ATTR':
            return [[1], [1]]
        else:
            return [-1]

    @staticmethod
    def get_response():
        class CallbackGetResponse():
            parameters = dict()
