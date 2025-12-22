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

import json

from mindiebenchmark.common.params_check import check_type_and_range
from .base_api import BaseAPI

TEXT_OUTPUT_STR = "text_output"


class OpenAIChatTextAPI(BaseAPI):
    """A class for openai chat text inference.

    This class follows the format of openai. It has the same parameters as openai, but just for chat
    and full text inference.
    """

    def get_inference_parameters(self):
        params = {
            "max_tokens": self.max_new_token
        }
        params.update(self.sample_params)
        return params

    def request(self, req_id: str, data):
        params = self.get_inference_parameters()
        if self.dataloader.dataset_type == "synthetic":
            params.update({
                "max_tokens": self.result_caching[req_id].num_expect_generated_tokens,
                # For synthetic, ignore_eos is True in default. Unless the user set it as False.
                "ignore_eos": self.sample_params.get("ignore_eos", True)
            })

        result = self.client_instance.request(
            model_name=self.result_caching[req_id].model_id,
            inputs=data,
            parameters=params,
            request_id=req_id,
            is_stream=False,
        )
        content = json.loads(result)
        self.result_caching[req_id].output = content["choices"]
        self.result_caching[req_id].num_generated_chars = len(self.result_caching[req_id].output)
        self.result_caching[req_id].num_generated_tokens = content["usage"]["completion_tokens"]
        self.result_caching[req_id].prefill_latency = content["prefill_time"]
        self.result_caching[req_id].decode_cost = content["decode_time_arr"]

    def validate_sampling_parameters(self):
        int32_range = '(-2147483647, 2147483647]'
        int64_range = '(-18446744073709551615, 18446744073709551615]'
        support_sample_params = {
            "model": ((str, ), ''),
            "stream": ((bool, ), ''),
            "presence_penalty": ((float, int), ''),
            "frequency_penalty": ((float, int), ''),
            "repetition_penalty": ((float, int), ''),
            "temperature": ((float, int), ''),
            "top_p": ((float, int), ''),
            "top_k": ((int, ), int32_range),
            "seed": ((int, ), int64_range),
            "stop": ((str, list), ''),
            "stop_token_ids": ((list,), ''),
            "include_stop_str_in_output": ((bool, ), ''),
            "skip_special_tokens": ((bool, ), ''),
            "ignore_eos": ((bool, ), ''),
            "max_tokens": ((int, ), int32_range),
            "tools": ((list, ), ''),
            "tool_choice": ((str, dict), ''),
            "best_of": ((int, ), int32_range),
            "n": ((int, ), int32_range),
            "use_beam_search": ((bool, ), ''),
            "top_logprobs": ((int, ), int32_range),
            "logprobs": ((bool, ), ''),
            "beam_search": ((bool, ), ''),
        }

        for key, value in self.sample_params.items():
            if key not in support_sample_params:
                self.logger.warning(f"Parameter '{key}' is not support in --SamplingParams.")
            else:
                types, value_range = support_sample_params.get(key)
                check_type_and_range(key, value, types, value_range)
            if key == "max_tokens":
                self.logger.warning("Parameter 'max_tokens' in --SamplingParams will overwrite --MaxOutputLen.")
            if key == "stream":
                self.logger.warning("Parameter 'stream' in --SamplingParams will overwrite --TaskKind.")


class OpenAIChatStreamAPI(BaseAPI):
    def get_inference_parameters(self):
        params = {
            "max_tokens": self.max_new_token
        }
        params.update(self.sample_params)
        return params

    def request(self, req_id: str, data):
        outputs = []
        params = self.get_inference_parameters()
        if self.dataloader.dataset_type == "synthetic":
            params.update({
                "max_tokens": self.result_caching[req_id].num_expect_generated_tokens,
                # For synthetic, ignore_eos is True in default. Unless the user set it as False.
                "ignore_eos": self.sample_params.get("ignore_eos", True)
            })

        for res_ in self.client_instance.request(
            model_name=self.result_caching[req_id].model_id,
            inputs=data,
            request_id=req_id,
            parameters=params,
        ):
            if isinstance(res_[TEXT_OUTPUT_STR], dict):
                for value in res_[TEXT_OUTPUT_STR].values():
                    self.result_caching[req_id].num_generated_chars += len(value)
                    break
            else:
                self.result_caching[req_id].num_generated_chars += len(res_[TEXT_OUTPUT_STR])
            outputs.append(res_)
            if not self.result_caching[req_id].num_generated_tokens:
                self.result_caching[req_id].prefill_latency = res_["prefill_time"]
            else:
                self.result_caching[req_id].decode_cost.append(res_["decode_time"])
            self.result_caching[req_id].num_generated_tokens += 1

        if len(outputs) > 0 and isinstance(outputs[0], dict):
            self.result_caching[req_id].output = outputs
        else:
            self.result_caching[req_id].output = "".join(outputs)

    def validate_sampling_parameters(self):
        int32_range = '(-2147483647, 2147483647]'
        int64_range = '(-18446744073709551615, 18446744073709551615]'
        support_sample_params = {
            "model": ((str, ), ''),
            "stream": ((bool, ), ''),
            "presence_penalty": ((float, int), ''),
            "frequency_penalty": ((float, int), ''),
            "repetition_penalty": ((float, int), ''),
            "temperature": ((float, int), ''),
            "top_p": ((float, int), ''),
            "top_k": ((int, ), int32_range),
            "seed": ((int, ), int64_range),
            "stop": ((str, list), ''),
            "stop_token_ids": ((list,), ''),
            "include_stop_str_in_output": ((bool, ), ''),
            "skip_special_tokens": ((bool, ), ''),
            "ignore_eos": ((bool, ), ''),
            "max_tokens": ((int, ), int32_range),
            "tools": ((list, ), ''),
            "tool_choice": ((str, dict), ''),
            "best_of": ((int, ), int32_range),
            "n": ((int, ), int32_range),
            "use_beam_search": ((bool, ), ''),
            "top_logprobs": ((int, ), int32_range),
            "logprobs": ((bool, ), ''),
            "beam_search": ((bool, ), ''),
        }

        for key, value in self.sample_params.items():
            if key not in support_sample_params:
                self.logger.warning(f"Parameter '{key}' is not support in --SamplingParams.")
            else:
                types, value_range = support_sample_params.get(key)
                check_type_and_range(key, value, types, value_range)
            if key == "max_tokens":
                self.logger.warning("Parameter 'max_tokens' in --SamplingParams will overwrite --MaxOutputLen.")
            if key == "stream":
                self.logger.warning("Parameter 'stream' in --SamplingParams will overwrite --TaskKind.")


class OpenAIStreamAPI(BaseAPI):
    def get_inference_parameters(self):
        params = {
            "temperature": 0.0,
            "max_tokens": self.max_new_token,
            "stream": True,
        }
        params.update(self.sample_params)
        return params

    def request(self, req_id: str, data):
        outputs = []
        params = self.get_inference_parameters()
        if self.dataloader.dataset_type == "synthetic":
            params.update({
                "max_tokens": self.result_caching[req_id].num_expect_generated_tokens,
                # For synthetic, ignore_eos is True in default. Unless the user set it as False.
                "ignore_eos": self.sample_params.get("ignore_eos", True)
            })

        for res_ in self.client_instance.request(
            model_name=self.result_caching[req_id].model_id,
            inputs=data,
            request_id=req_id,
            parameters=params,
        ):
            token = res_["choices"][0]["text"]
            outputs.append(token)
            if not self.result_caching[req_id].num_generated_tokens:
                self.result_caching[req_id].prefill_latency = res_["prefill_time"]
            else:
                self.result_caching[req_id].decode_cost.append(res_["decode_time"])
            self.result_caching[req_id].num_generated_tokens += 1
            self.result_caching[req_id].num_generated_chars += len(token)
        self.result_caching[req_id].output = "".join(outputs)

    def validate_sampling_parameters(self):
        int32_range = '(-2147483647, 2147483647]'
        int64_range = '(-18446744073709551615, 18446744073709551615]'
        support_sample_params = {
            "model": ((str, ), ''),
            "stream": ((bool, ), ''),
            "presence_penalty": ((float, int), ''),
            "frequency_penalty": ((float, int), ''),
            "repetition_penalty": ((float, int), ''),
            "temperature": ((float, int), ''),
            "top_p": ((float, int), ''),
            "top_k": ((int, ), int32_range),
            "seed": ((int, ), int64_range),
            "stop": ((str, list), ''),
            "stop_token_ids": ((list,), ''),
            "include_stop_str_in_output": ((bool, ), ''),
            "skip_special_tokens": ((bool, ), ''),
            "ignore_eos": ((bool, ), ''),
            "max_tokens": ((int, ), int32_range),
            "tools": ((list, ), ''),
            "tool_choice": ((str, dict), ''),
            "best_of": ((int, ), int32_range),
            "n": ((int, ), int32_range),
            "top_logprobs": ((int, ), int32_range),
            "logprobs": ((bool, ), ''),
            "beam_search": ((bool, ), ''),
        }

        for key, value in self.sample_params.items():
            if key not in support_sample_params:
                self.logger.warning(f"Parameter '{key}' is not support in --SamplingParams.")
            else:
                types, value_range = support_sample_params.get(key)
                check_type_and_range(key, value, types, value_range)
            if key == "max_tokens":
                self.logger.warning("Parameter 'max_tokens' in --SamplingParams will overwrite --MaxOutputLen.")
            if key == "stream":
                self.logger.warning("Parameter 'stream' in --SamplingParams will overwrite --TaskKind.")
