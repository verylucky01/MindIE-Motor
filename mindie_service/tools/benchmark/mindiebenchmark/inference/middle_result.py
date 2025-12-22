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

from dataclasses import dataclass, field


@dataclass
class MiddleResult:
    data_id: str = ''
    input_data: str = ''
    num_input_tokens: int = 0
    num_input_chars: int = 0
    num_generated_tokens: int = 0
    num_generated_chars: int = 0
    prefill_latency: float = .0
    decode_cost: list[float] = field(default_factory=list)
    req_latency: float = .0
    decode_batch_size: list[int] = field(default_factory=list)
    prefill_batch_size: int = 0
    post_process_time: float = .0
    tokenized_time: float = .0
    detokenized_time: float = .0
    queue_wait_time: list[float] = field(default_factory=list)
    data_option: list = field(default_factory=list)
    output: str = ""
    output_token_id: list[int] = field(default_factory=list)
    start_time: float = .0
    end_time: float = .0
    request_id: str = ''
    model_id: str = ''
    success: bool = False

    def is_valid(self):
        """To ensure valid results"""
        return all([self.num_input_tokens, self.num_input_chars,
                    self.num_generated_tokens, self.num_generated_chars, self.decode_cost])

    def convert_to_standard(self) -> dict:
        return {
            "id": self.data_id,
            "input_data": self.input_data,
            'prefill_latencies': self.prefill_latency,
            'decode_token_latencies': self.decode_cost[:],
            'decode_max_token_latencies': max(self.decode_cost) if self.decode_cost else .0,
            'seq_latencies': self.req_latency,
            'input_tokens_len': self.num_input_tokens,
            'generate_tokens_len': self.num_generated_tokens,
            'generate_characters_len': self.num_generated_chars,
            'prefill_batch_size': self.prefill_batch_size,
            'decode_batch_size': self.decode_batch_size[:],
            'queue_wait_time': self.queue_wait_time[:],
            'post_processing_time': self.post_process_time,
            'tokenizer_time': self.tokenized_time,
            'detokenizer_time': self.detokenized_time,
            'output': self.output,
            'output_token_id': self.output_token_id,
            'characters_per_token': self.num_generated_chars / self.num_generated_tokens
            if self.num_generated_tokens else 0.0,
            'request_id': self.request_id,
        }
