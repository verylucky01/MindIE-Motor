#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import sys
import rapidjson as json
import numpy as np

# append mindie service path
python_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
mindieservice_path = os.path.dirname(os.path.dirname(python_path))
sys.path.append(mindieservice_path)
from mindieclient.python.common.logging import Log
from .utils import raise_error


class Result:
    def __init__(self, response, verbose):
        self.logger = Log(__name__).getlog()
        json_content = response.decode()
        if len(json_content) > (10 * 1024 * 1024):
            raise_error("Response size is larger than 10MB.")
        if verbose:
            self.logger.info("The json response is {}".format(json_content))
        self._result_dict = json.loads(json_content)

    def retrieve_output_index_to_numpy(self, index):
        output_str = "outputs"
        if self._result_dict.get(output_str) is None:
            raise_error("Result dict does not contain outputs key!")
        if index < 0 or len(self._result_dict[output_str]) <= index:
            raise_error("The index should be in range [0, output_num], \
                but got {}".format(index))
        output = self._result_dict[output_str][index]
        arr = np.array(output["data"], dtype=np.uint32)
        return arr.reshape(output["shape"])

    def retrieve_output_name_to_numpy(self, name):
        if self._result_dict.get("outputs") is None:
            raise_error("Result dict does not contain outputs key!")
        for output in self._result_dict["outputs"]:
            if output["name"] == name:
                arr = np.array(output["data"], dtype=np.uint32)
                return arr.reshape(output["shape"])
            else:
                raise_error(
                    "The name {} is not equal to the response output name {}!".format(
                        name, output["name"]
                    )
                )
        return None

    def get_response(self):
        return self._result_dict
