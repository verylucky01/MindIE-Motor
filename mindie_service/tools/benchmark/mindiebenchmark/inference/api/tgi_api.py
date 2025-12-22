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

from mindiebenchmark.common.params_check import check_type_and_range
from .base_api import BaseAPI


class TgiAPI(BaseAPI):
    """A class for tgi inference.

    This class is used for handle tgi (text-generation-inference) request.
    """
    def validate_sampling_parameters(self):
        int32_range = '(-2147483647, 2147483647]'
        int64_range = '(-18446744073709551615, 18446744073709551615]'
        support_sample_params = {
            "repetition_penalty": ((float, int), ''),
            "return_full_text": ((bool, ), ''),
            "seed": ((int, ), int64_range),
            "temperature": ((float, int), ''),
            "top_k": ((int, ), int32_range),
            "top_p": ((float, int), ''),
            "truncate": ((bool, ), ''),
            "typical_p": ((float, ), ''),
            "watermark": ((bool, ), ''),
            "details": ((bool, ), ''),
            "do_sample": ((bool, ), ''),
            "max_new_tokens": ((int, ), int32_range),
            "stop": ((str, list), ''),
            "adapter_id": ((str, ), '')
        }

        for key, value in self.sample_params.items():
            if key not in support_sample_params:
                self.logger.warning(f"Parameter '{key}' is not support in --SamplingParams.")
            else:
                types, value_range = support_sample_params.get(key)
                check_type_and_range(key, value, types, value_range)

            if key == "do_sample":
                self.logger.warning("Parameter 'do_sample' in --SamplingParams will overwrite "
                                    "--DoSampling.")
            if key == "max_new_tokens":
                self.logger.warning("Parameter 'max_new_tokens' in --SamplingParams will overwrite "
                                    "--MaxOutputLen.")

    def request(self, req_id: str, data):
        outputs = []
        for res_ in self.client_instance.request(
            model_name=self.result_caching[req_id].model_id,
            inputs=data,
            request_id=req_id,
            parameters=self.get_inference_parameters()
        ):
            if res_["token"]["special"]:
                break
            token = res_["token"]["text"]
            outputs.append(token)
            if not self.result_caching[req_id].num_generated_tokens:
                self.result_caching[req_id].prefill_latency = res_["prefill_time"]
            else:
                self.result_caching[req_id].decode_cost.append(res_["decode_time"])
            self.result_caching[req_id].num_generated_tokens += 1
            self.result_caching[req_id].num_generated_chars += len(token)
        self.result_caching[req_id].output = "".join(outputs)
