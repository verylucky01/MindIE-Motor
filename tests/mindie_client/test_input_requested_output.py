# Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.


import unittest
from unittest import mock
from unittest.mock import MagicMock
import numpy as np
from mindieclient.python.httpclient.input_requested_output import Input, RequestedOutput
from mindieclient.python.common.logging import Log

test_module_path = 'mindieclient.python.httpclient.input_requested_output'

class TestInputAndRequestedOutput(unittest.TestCase):

    def setUp(self):
        mock.patch("mindieclient.python.common.config.Config._check_file_permission",
                    return_value=(True, None)).start()

    def tearDown(self):
        mock.patch.stopall()

    @mock.patch.object(Log, 'getlog')
    def test_input_name_empty(self, mock_getlog):
        # 测试 Input name 为空时的默认值和日志警告
        mock_logger = MagicMock()
        mock_getlog.return_value = mock_logger
        input_obj = Input("", [1, 100], "UINT32")
        self.assertEqual(input_obj.get_input_name(), "input0")
        mock_logger.warning.assert_called_once_with("[MIE02W000402] Input name is an empty string. "
                                                    "Set it with default value input0!")

    @mock.patch(test_module_path +'.raise_error')
    def test_input_name_too_long(self, mock_raise_error):
        # 测试 Input name 超过长度限制
        Input("a" * 1001, [1, 100], "UINT32")
        mock_raise_error.assert_called_once_with("[MIE02E000402] The length of name should be in range[1, 1000], "
                                                 "but got 1001")

    @mock.patch(test_module_path +'.raise_error')
    def test_input_shape_invalid(self, mock_raise_error):
        # 测试 shape 不符合指定格式
        Input("input1", [2, 16001], "UINT32")
        mock_raise_error.assert_called_once_with("[MIE02E000402] The format of shape should be like [1,n] "
                                                 "and n <= 16000, but got [2, 16001]")

    @mock.patch(test_module_path +'.raise_error')
    def test_initialize_data_invalid_tensor_type(self, mock_raise_error):
        # 设置 mock_raise_error 在调用时抛出异常
        mock_raise_error.side_effect = ValueError("[MIE02E000402] Input_tensor must be a numpy array!")
        # 创建 Input 对象
        input_obj = Input("input1", [1, 100], "UINT32")
        # 尝试传入不是 np.ndarray 类型的数据，应该触发异常
        with self.assertRaises(ValueError) as context:
            input_obj.initialize_data([1, 2, 3])

        mock_raise_error.assert_called_once_with("Input_tensor must be a numpy array!")

    @mock.patch(test_module_path +'.raise_error')
    def test_initialize_data_shape_mismatch(self, mock_raise_error):
        # 测试 tensor 的形状和 input 的 shape 不匹配
        input_obj = Input("input1", [1, 100], "UINT32")
        tensor = np.ones((1, 50), dtype=np.uint32)  # Shape mismatch
        input_obj.initialize_data(tensor)
        mock_raise_error.assert_called_once_with("[MIE02E000402] Expected dim 1 value is 100, but got 50")

    @mock.patch(test_module_path +'.raise_error')
    def test_initialize_data_invalid_dtype(self, mock_raise_error):
        # 测试 tensor 数据类型不是 uint32
        input_obj = Input("input1", [1, 100], "UINT32")
        tensor = np.ones((1, 100), dtype=np.float32)  # Wrong dtype
        input_obj.initialize_data(tensor)
        mock_raise_error.assert_called_once_with("[MIE02E000402] Expected input tensor data_type uint32, "
                                                 "but got float32")

    @mock.patch(test_module_path +'.raise_error')
    def test_initialize_data_invalid_input_data_type(self, mock_raise_error):
        # 测试 input 的数据类型不匹配
        input_obj = Input("input1", [1, 100], "FLOAT32")
        tensor = np.ones((1, 100), dtype=np.uint32)
        input_obj.initialize_data(tensor)
        mock_raise_error.assert_called_once_with("[MIE02E000402] Expected input data_type UINT32, but got FLOAT32")

    def test_get_input_tensor(self):
        # 测试生成 input tensor 并验证内容
        input_obj = Input("input1", [1, 100], "UINT32")
        tensor = np.ones((1, 100), dtype=np.uint32)
        input_obj.initialize_data(tensor)
        input_tensor = input_obj.get_input_tensor()
        self.assertEqual(input_tensor["name"], "input1")
        self.assertEqual(input_tensor["datatype"], "UINT32")
        self.assertEqual(input_tensor["shape"], [1, 100])
        self.assertEqual(input_tensor["data"], [1] * 100)

    @mock.patch(test_module_path +'.raise_error')
    def test_get_input_tensor_no_data(self, mock_raise_error):
        # 测试获取数据时 input_data 为 None 的情况
        input_obj = Input("input1", [1, 100], "UINT32")
        input_obj.get_input_tensor()
        mock_raise_error.assert_called_once_with("[MIE02E000402] The input data is None!")

    @mock.patch.object(Log, 'getlog')
    def test_requested_output_name_empty(self, mock_getlog):
        # 测试 Output name 为空时的默认值和日志警告
        mock_logger = MagicMock()
        mock_getlog.return_value = mock_logger
        output_obj = RequestedOutput("")
        self.assertEqual(output_obj.get_output_name(), "output0")
        mock_logger.warning.assert_called_once_with("[MIE02W000402] Output name is an empty string. "
                                             "Set it with default value output0!")

    @mock.patch(test_module_path +'.raise_error')
    def test_requested_output_name_too_long(self, mock_raise_error):
        # 测试 Output name 超过长度限制
        RequestedOutput("a" * 1001)
        mock_raise_error.assert_called_once_with("[MIE02W000402] The length of name should be in range[1, 1000], "
                                                 "but got 1001")

    def test_get_output_tensor(self):
        # 测试获取 output tensor
        output_obj = RequestedOutput("output1")
        output_tensor = output_obj.get_output_tensor()
        self.assertEqual(output_tensor["name"], "output1")


if __name__ == "__main__":
    unittest.main()
