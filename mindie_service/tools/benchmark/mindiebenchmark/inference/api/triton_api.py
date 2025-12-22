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

from pydantic import ValidationError

from mindiebenchmark.inference.schemas import TextResponse
from mindiebenchmark.common.params_check import check_type_and_range
from .base_api import BaseAPI

END_OF_SEQ = "</s>"
TEXT_OUTPUT_STR = "text_output"


class TritonTextAPI(BaseAPI):
    """A class for triton text inference.

    This class is used for handle triton request.
    """
    def request(self, req_id: str, data):
        params = self.get_inference_parameters()
        params.update({"perf_stat": True})
        response = self.client_instance.request(
            model_name=self.model_name,
            inputs=data,
            parameters=params,
            request_id=req_id,
            is_stream=False
        )
        try:
            content = TextResponse.model_validate(response.get_response())
        except ValidationError as e:
            self.logger.error("[MIE01E000108] Request %s failed with response format error:\n %s",
                              req_id, e.errors())
            return
        except Exception:
            self.logger.error("Get requests failed")
            return
        self.result_caching[req_id].num_generated_tokens = content.details.generated_tokens
        self.result_caching[req_id].num_generated_chars = len(content.text_output)
        self.result_caching[req_id].output = content.text_output
        self.result_caching[req_id].prefill_latency = content.details.perf_stat[0][1]
        self.result_caching[req_id].decode_cost = [content.details.perf_stat[i + 1][1]
                                                   for i in range(len(content.details.perf_stat) - 1)]

    def validate_sample_parameters(self):
        int32_range = '(-2147483647, 2147483647]'
        int64_range = '(-18446744073709551615, 18446744073709551615]'
        support_sample_params = {
            "details": ((bool, ), ''),
            "do_sample": ((bool, ), ''),
            "max_new_tokens": ((int, ), int32_range),
            "repetition_penalty": ((float, int), ''),
            "frequency_penalty": ((float, int), ''),
            "presence_penalty": ((float, int), ''),
            "seed": ((int, ), int64_range),
            "temperature": ((float, int), ''),
            "top_k": ((int, ), int32_range),
            "top_p": ((float, int), ''),
            "batch_size": ((int, ), int32_range),
            "typical_p": ((float, ), ''),
            "watermark": ((bool, ), ''),
            "perf_stat": ((bool, ), ''),
            "priority": ((int, ), int32_range),
            "timeout": ((int, ), int64_range),
            "firstTokenCost": ((int, ), int64_range),
            "decodeTime": ((list, ), '')
        }

        for key, value in self.sample_params.items():
            if key not in support_sample_params:
                self.logger.warning(f"Parameter '{key}' is not support in --SamplingParams. "
                                     "Please remove it from --SamplingParams.")
            else:
                types, value_range = support_sample_params.get(key)
                check_type_and_range(key, value, types, value_range)
            if key == "details":
                self.logger.warning("Set 'details' in --SamplingParams may result in error.")
            if key == "do_sample":
                self.logger.warning("Parameter 'do_sample' in --SamplingParams will overwrite "
                                    "--DoSampling.")
            if key == "max_new_tokens":
                self.logger.warning("Parameter 'max_new_tokens' in --SamplingParams will overwrite "
                                    "--MaxOutputLen.")


class TritonStreamAPI(BaseAPI):
    """A class for triton stream inference.

    This class is used for handle triton request.
    """
    def request(self, req_id: str, data):
        outputs = []
        pre_token_num = 0
        for res_ in self.client_instance.request(
            model_name=self.model_name,
            inputs=data,
            request_id=req_id,
            parameters=self.get_inference_parameters()
        ):
            cur_generate_token_num = res_.get('generated_tokens', 1)
            if res_[TEXT_OUTPUT_STR] == END_OF_SEQ:
                break
            outputs.append(res_[TEXT_OUTPUT_STR])
            if not self.result_caching[req_id].num_generated_tokens:
                generate_token_num = cur_generate_token_num
                self.result_caching[req_id].prefill_latency = res_["prefill_time"]
                self.result_caching[req_id].prefill_batch_size = res_["batch_size"]
            else:
                generate_token_num = max(1, cur_generate_token_num - pre_token_num)
                decode_times = [res_["decode_time"] / generate_token_num] * generate_token_num
                decode_batch_size = [res_["batch_size"]] * generate_token_num
                self.result_caching[req_id].decode_cost.extend(decode_times)
                self.result_caching[req_id].decode_batch_size.extend(decode_batch_size)
                
            pre_token_num = cur_generate_token_num
            self.result_caching[req_id].num_generated_tokens = cur_generate_token_num
            self.result_caching[req_id].num_generated_chars += len(res_[TEXT_OUTPUT_STR])
            self.result_caching[req_id].queue_wait_time.append(res_["queue_wait_time"])
        self.result_caching[req_id].output = "".join(outputs)

    def validate_sample_parameters(self):
        int32_range = '(-2147483647, 2147483647]'
        int64_range = '(-18446744073709551615, 18446744073709551615]'
        support_sample_params = {
            "details": ((bool, ), ''),
            "do_sample": ((bool, ), ''),
            "max_new_tokens": ((int, ), int32_range),
            "repetition_penalty": ((float, int), ''),
            "frequency_penalty": ((float, int), ''),
            "presence_penalty": ((float, int), ''),
            "seed": ((int, ), int64_range),
            "temperature": ((float, int), ''),
            "top_k": ((int, ), int32_range),
            "top_p": ((float, int), ''),
            "batch_size": ((int, ), int32_range),
            "typical_p": ((float, ), ''),
            "watermark": ((bool, ), ''),
            "perf_stat": ((bool, ), ''),
            "priority": ((int, ), int32_range),
            "timeout": ((int, ), int64_range),
            "firstTokenCost": ((int, ), int64_range),
            "decodeTime": ((list, ), '')
        }

        for key, value in self.sample_params.items():
            if key not in support_sample_params:
                self.logger.warning(f"Parameter '{key}' is not support in --SamplingParams. "
                                     "Please remove it from --SamplingParams.")
            else:
                types, value_range = support_sample_params.get(key)
                check_type_and_range(key, value, types, value_range)

            if key == "details":
                self.logger.warning("Set 'details' in --SamplingParams may result in error.")
            if key == "do_sample":
                self.logger.warning("Parameter 'do_sample' in --SamplingParams will overwrite "
                                    "--DoSampling.")
            if key == "max_new_tokens":
                self.logger.warning("Parameter 'max_new_tokens' in --SamplingParams will overwrite "
                                    "--MaxOutputLen.")
