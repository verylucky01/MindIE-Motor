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
import numpy as np

from mindiesimulator.request_sender import RequestStrategy, UniformSender, NonUniformSender


class TestRequestStrategy(unittest.TestCase):
    def test_abstract_method(self):
        # test NotImplementedError
        class TestStrategy(RequestStrategy):
            pass
        with self.assertRaises(TypeError):
            RequestStrategy()  


class TestUniformSender(unittest.TestCase):
    def test_initialization(self):
        sender = UniformSender(request_rate=2, random_seed=42)
        self.assertEqual(sender.request_rate, 2)
        self.assertEqual(sender.random_seed, 42)

    def test_set_random_seed(self):
        sender = UniformSender(request_rate=1, random_seed=42)
        first_time = sender.set_next_request_in_time(0)
        
        sender.set_random_seed(42)
        second_time = sender.set_next_request_in_time(0)
        
        self.assertEqual(first_time, second_time, "相同的种子应生成相同的时间间隔")

        sender = UniformSender(request_rate=10, random_seed=33)
        first_time = sender.set_next_request_in_time(0)
        
        sender.set_random_seed(33)
        second_time = sender.set_next_request_in_time(0)
        
        self.assertEqual(first_time, second_time, "相同的种子应生成相同的时间间隔")

    def test_uniform_range(self):
        sender = UniformSender(request_rate=1, random_seed=42)
        for _ in range(100):
            previous_time = np.random.randint(0, 100)
            next_time = sender.set_next_request_in_time(previous_time)
            self.assertGreaterEqual(next_time, previous_time)
            self.assertLessEqual(next_time, previous_time + 2)


class TestNonUniformSender(unittest.TestCase):
    def test_initialization(self):
        sender = NonUniformSender(request_rate=3)
        self.assertEqual(sender.request_rate, 3)

    def test_fixed_interval(self):
        sender = NonUniformSender(request_rate=2)
        self.assertEqual(sender.set_next_request_in_time(0), 0.5)
        self.assertEqual(sender.set_next_request_in_time(0.5), 1.0)
        self.assertEqual(sender.set_next_request_in_time(1.0), 1.5)
        self.assertEqual(sender.set_next_request_in_time(100.0), 100.5)
        self.assertEqual(sender.set_next_request_in_time(135.5), 136.0)
        self.assertEqual(sender.set_next_request_in_time(201.0), 201.5)

    def test_multiple_calls(self):
        sender = NonUniformSender(request_rate=1)
        times = [0]
        for _ in range(5):
            times.append(sender.set_next_request_in_time(times[-1]))
        
        expected = [0, 1, 2, 3, 4, 5]
        self.assertEqual(times, expected)

if __name__ == '__main__':
    unittest.main()