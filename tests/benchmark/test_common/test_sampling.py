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

import unittest
from unittest import mock
import time

import numpy as np

from mindiebenchmark.common.utils import sample_one_value, get_time_stamp_ms


class TestSamplingFunctions(unittest.TestCase):

    @mock.patch('numpy.random.uniform')
    def test_sample_one_value_uniform(self, mock_uniform):
        # Mock np.random.uniform to return a fixed value
        mock_uniform.return_value = 10.5
        
        params = {
            "MinValue": 1,
            "MaxValue": 20
        }
        result = sample_one_value("uniform", params)
        
        # Assertions
        mock_uniform.assert_called_with(1, 20)
        self.assertEqual(result, 10)  # Result should be converted to an integer

    @mock.patch('numpy.random.normal')
    def test_sample_one_value_gaussian(self, mock_normal):
        # Mock np.random.normal to return a fixed value
        mock_normal.return_value = 5.2
        
        params = {
            "MinValue": 1,
            "MaxValue": 10,
            "Mean": 5,
            "Var": 4
        }
        result = sample_one_value("gaussian", params)
        
        # Assertions
        mock_normal.assert_called_with(5, 2)  # stddev is sqrt(Var)
        self.assertEqual(result, 5)  # Result should be clipped to the min/max range and converted to int

    @mock.patch('numpy.random.zipf')
    def test_sample_one_value_zipf(self, mock_zipf):
        # Mock np.random.zipf to return a fixed value
        mock_zipf.return_value = 3
        
        params = {
            "MinValue": 1,
            "MaxValue": 10,
            "Alpha": 2.0
        }
        result = sample_one_value("zipf", params)
        
        # Assertions
        mock_zipf.assert_called_with(2.0)
        self.assertEqual(result, 3)

    def test_sample_one_value_invalid_method(self):
        params = {
            "MinValue": 1,
            "MaxValue": 10
        }
        
        with self.assertRaises(ValueError):
            sample_one_value("invalid_method", params)

    @mock.patch('time.time')
    @mock.patch('time.localtime')
    @mock.patch('time.strftime')
    def test_get_time_stamp_ms(self, mock_strftime, mock_localtime, mock_time):
        # Mock time-related functions
        mock_time.return_value = 1617123456.789  # Fixed timestamp
        mock_localtime.return_value = time.localtime(1617123456.789)
        mock_strftime.return_value = "20210401123045"  # Mock the output of time.strftime
        
        result = get_time_stamp_ms()

        # Assertions
        mock_time.assert_called_once()
        
        # Ensure localtime is called with the correct timestamp
        mock_localtime.assert_called_with(1617123456.789)
        
        # Ensure strftime is called with the correct format
        mock_strftime.assert_called_with("%Y%m%d%H%M%S", time.localtime(1617123456.789))

        # Adjust expected result to match the rounded milliseconds
        self.assertEqual(result, "20210401123045789")  # Adjusted expected value

    def tearDown(self) -> None:
        pass
    
if __name__ == '__main__':
    unittest.main()
