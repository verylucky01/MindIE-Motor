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

import numpy

from mindiebenchmark.common.params_check import check_type_and_range
from .base_api import BaseAPI


class MindIETokenAPI(BaseAPI):
    """A class for mindie token inference.

    This class will send request following the rules of mindie. It just handle the data that is
    token-id, instead of string text.
    """
    def process_input_data(self, data, rrid: str):
        data_arr = numpy.array(data, dtype=numpy.uint32)
        if data_arr.ndim == 1:
            data_arr = numpy.expand_dims(data_arr, axis=0)
        self.result_caching[rrid].num_input_tokens = data_arr.shape[-1]
        self.result_caching[rrid].num_input_chars = data_arr.shape[-1]
        return data_arr

    def request(self, req_id: str, data: list):
        outputs = []
        for res_ in self.client_instance.request(
            model_name=self.model_name,
            inputs=data,
            request_id=req_id,
            parameters=self.get_inference_parameters()
        ):
            text_output = res_["token"]["text"]
            if text_output is None:
                break
            outputs.append(text_output)
            if not self.result_caching[req_id].num_generated_tokens:
                self.result_caching[req_id].prefill_latency = res_["prefill_time"]
            else:
                self.result_caching[req_id].decode_cost.append(res_["decode_time"])
            self.result_caching[req_id].num_generated_tokens += 1
            self.result_caching[req_id].num_generated_chars += len(text_output)

    def validate_sample_parameters(self):
        int32_range = '(-2147483647, 2147483647]'
        int64_range = '(-18446744073709551615, 18446744073709551615]'
        support_sample_params = {
            "stream": ((bool, ), ''),
            "max_new_tokens": ((int, ), int32_range),
            "do_sample": ((bool, ), ''),
            "details": ((bool, ), ''),
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
            if key == "max_tokens":
                self.logger.warning("Parameter 'max_tokens' in --SamplingParams will overwrite --MaxOutputLen.")
            if key == "stream":
                self.logger.warning("Parameter 'stream' in --SamplingParams will overwrite --TaskKind.")
            if key == "details":
                self.logger.warning("Set 'details' in --SamplingParams may result in error.")
            if key == "do_sample":
                self.logger.warning("Parameter 'do_sample' in --SamplingParams will overwrite "
                                    "--DoSampling.")
            if key == "max_new_tokens":
                self.logger.warning("Parameter 'max_new_tokens' in --SamplingParams will overwrite "
                                    "--MaxOutputLen.")
