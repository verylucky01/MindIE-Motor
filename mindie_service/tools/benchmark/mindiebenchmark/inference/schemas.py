#!/usr/bin/env python3
# coding: utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

"""This file is to storage json schemas for validating responses"""
from pydantic import BaseModel, Field, ConfigDict


def to_camel(string: str) -> str:
    return ''.join(word.capitalize() for word in string.split('_'))


class PerfStat(BaseModel):
    token_id: str | int = Field(alias="tokenId")
    process_time: float
    process_batch_size: int | None = None
    queue_wait_time: float | None = None
    post_process_time: float | None = None
    tokenizer_time: float | None = None
    detokenizer_time: float | None = None

    @property
    def is_text(self):
        return self.tokenizer_time is not None


class ReturnParameters(BaseModel):
    perf_stat: list[PerfStat]
    process_text_length: int | None = None

    @property
    def first_token(self):
        if self.perf_stat:
            return self.perf_stat[0]
        else:
            return -1

    @property
    def first_token_time(self):
        first_token = self.first_token
        if first_token:
            return first_token.process_time
        else:
            return -1

    @property
    def decode_cost(self):
        return [item.process_time for item in self.perf_stat] if self.perf_stat else []

    @property
    def process_batch_size(self):
        return [item.process_batch_size for item in self.perf_stat] if self.perf_stat else []

    @property
    def post_process_time(self):
        return [item.post_process_time for item in self.perf_stat] if self.perf_stat else []

    @property
    def queue_wait_time(self):
        return [item.queue_wait_time for item in self.perf_stat] if self.perf_stat else []

    @property
    def tokenizer_time(self):
        if self.perf_stat and self.perf_stat[0].is_text:
            return [item.tokenizer_time for item in self.perf_stat]
        else:
            return []

    @property
    def detokenizer_time(self):
        if self.perf_stat and self.perf_stat[0].is_text:
            return [item.detokenizer_time for item in self.perf_stat]
        else:
            return []


class InferResponse(BaseModel):
    class InferOutput(BaseModel):
        name: str
        shape: list[int]
        datatype: str = "UINT32"
        data: list[int]

        def valid(self, output_key="OUTPUT0"):
            return self.name == output_key

    id: str
    outputs: list[InferOutput]

    @property
    def output(self):
        if self.outputs:
            return self.outputs[0]
        else:
            return {}


class TextResponse(BaseModel):
    class TextDetails(BaseModel):
        finish_reason: str
        generated_tokens: int
        first_token_cost: float | None = None
        decode_cost: list[float] | None = None
        perf_stat: list[list[int]] | None = None

    id: str
    model_name: str
    model_version: str | None
    text_output: str
    details: TextDetails | None
    model_config = ConfigDict(protected_namespaces=())
