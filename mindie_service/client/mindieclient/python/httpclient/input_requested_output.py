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
import numpy as np

# append mindie service path
python_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
mindieservice_path = os.path.dirname(os.path.dirname(python_path))
sys.path.append(mindieservice_path)
from mindieclient.python.common.logging import Log
from .utils import raise_error


class Input:
    def __init__(self, name: str, shape: np.ndarray, datatype: str):
        """
        constructor of Input class
        Args:
            name: str, input name
            shape: np.ndarray, input shape
            datatype: str, input data type
        """
        self.logger = Log(__name__).getlog()
        if name == "":
            self._input_name = "input0"
            self.logger.warning(
                "[MIE02W000402] Input name is an empty string. Set it with default value input0!"
            )
        elif len(name) > 1000:
            raise_error("[MIE02E000402] The length of name should be in range[1, 1000], "
                        "but got {}".format(len(name)))
        else:
            self._input_name = name
        self._check_shape(shape)
        self._input_data_type = datatype
        self._input_shape = shape
        self._input_data = None

    @staticmethod
    def _check_shape(shape: np.ndarray):
        if (not isinstance(shape, np.ndarray) or not np.issubdtype(shape.dtype, np.integer) or not np.all(shape > 0)):
            raise_error(
                "[MIE02E000402] Shape must be a numpy array and elements of shape must be integer, but got {}".format(
                    shape
                )
            )
        if len(shape) != 2 or shape[0] != 1 or shape[1] > 16000:
            raise_error(
                "[MIE02E000402] The format of shape should be like [1,n] and n <= 16000, but got {}".format(shape)
            )

    def get_input_name(self):
        return self._input_name

    def get_input_shape(self):
        return self._input_shape

    def set_input_shape(self, shape):
        self._check_shape(shape)
        self._input_shape = shape

    def get_input_data_type(self):
        return self._input_data_type

    def initialize_data(self, input_tensor):
        if not isinstance(input_tensor, (np.ndarray,)):
            raise_error("Input_tensor must be a numpy array!")
        # check input shape
        if len(input_tensor.shape) != len(self._input_shape):
            raise_error(
                "[MIE02E000402] Expected length of shape {}, but got{}".format(
                    self._input_shape, input_tensor.shape
                )
            )
        for idx, cur_dim in enumerate(self._input_shape):
            if cur_dim != input_tensor.shape[idx]:
                raise_error(
                    "[MIE02E000402] Expected dim {} value is {}, but got {}".format(
                        idx, cur_dim, input_tensor.shape[idx]
                    )
                )
        if input_tensor.dtype != np.dtype('uint32'):
            raise_error(
                "[MIE02E000402] Expected input tensor data_type uint32, but got {}".format(
                    input_tensor.dtype.name
                )
            )

        # check input datatype
        supported_dtype = "UINT32"
        if self._input_data_type != supported_dtype:
            raise_error(
                "[MIE02E000402] Expected input data_type {}, but got {}".format(
                    supported_dtype, self._input_data_type
                )
            )
        flatten_arr = input_tensor.flatten()
        self._input_data = [val.item() for val in flatten_arr]

    def get_input_tensor(self):
        input_tensor = {
            "name": self._input_name,
            "datatype": self._input_data_type,
            "shape": self._input_shape.tolist(),
        }
        if self._input_data is None:
            raise_error("[MIE02E000402] The input data is None!")
        input_tensor["data"] = self._input_data
        return input_tensor


class RequestedOutput:
    def __init__(self, name):
        self.logger = Log(__name__).getlog()
        if name == "" or name is None:
            self._output_name = "output0"
            self.logger.warning(
                "[MIE02W000402] Output name is an empty string. Set it with default value output0!"
            )
        elif len(name) > 1000:
            raise_error("[MIE02W000402] The length of name should be in range[1, 1000], but got {}".format(len(name)))
        else:
            self._output_name = name

    def get_output_name(self):
        return self._output_name

    def get_output_tensor(self):
        output_tensor = {"name": self._output_name}
        return output_tensor
