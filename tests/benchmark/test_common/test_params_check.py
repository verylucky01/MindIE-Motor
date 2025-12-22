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

from mindiebenchmark.common.params_check import check_type_and_range


class TestCheckTypeAndRange(unittest.TestCase):
    def test_valid_integer_in_inclusive_range(self):
        """Test valid integer within an inclusive range."""
        try:
            check_type_and_range("test_param", 5, (int,), "[0, 10]")
        except ValueError as e:
            self.fail(f"check_type_and_range raised ValueError unexpectedly: {str(e)}")

    def test_valid_float_in_exclusive_range(self):
        """Test valid float within an exclusive range."""
        try:
            check_type_and_range("test_param", 2.5, (float,), "(2.0, 3.0)")
        except ValueError as e:
            self.fail(f"check_type_and_range raised ValueError unexpectedly: {str(e)}")

    def test_invalid_type(self):
        """Test invalid type for parameter."""
        with self.assertRaises(ValueError) as context:
            check_type_and_range("test_param", "5", (int, float), "[0, 10]")
        self.assertIn("expect one of", str(context.exception))

    def test_out_of_range(self):
        """Test value out of range."""
        with self.assertRaises(ValueError) as context:
            check_type_and_range("test_param", -1, (int,), "[0, 10]")
        self.assertIn("not in range", str(context.exception))

    def test_invalid_range_format(self):
        """Test invalid range string format."""
        with self.assertRaises(ValueError) as context:
            check_type_and_range("test_param", 5, (int,), "0, 10")  # Missing brackets
        self.assertIn("invalid range format", str(context.exception))


if __name__ == '__main__':
    unittest.main()
