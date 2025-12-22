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
from unittest.mock import patch, MagicMock, Mock
from parameterized import parameterized  # Make sure to install this package
from mock_object import TokenizerModel
from mindiebenchmark.common.results import ResultData
from mindiebenchmark.common.tokenizer import BenchmarkTokenizer


class TestBenchmarkTokenizer(unittest.TestCase):

    def setUp(self):
        self.model_paths = {
            "transformers": 'mock_path',  # Path for Huggingface model
            "mindformers": 'mock_path'  # Path for Mindformers model
        }
        
        self.transformer_tokenize_mocker = patch(
            'transformers.AutoTokenizer.from_pretrained',
            return_value=TokenizerModel()
        )
        self.transformer_tokenize_mocker.start()
        
        self.mindformer_tokenize_mocker = patch(
            'mindformers.get_model',
            return_value=(TokenizerModel(), None)
        )
        self.mindformer_tokenize_mocker.start()

        self.result_mock = Mock()
        self.result_mock.tokenizer_time = []  # Array to record tokenizer timing
        self.result_mock.detokenizer_time = []  # Array to record detokenizer timing

        BenchmarkTokenizer.reset_instance()

    def clear_timing(self):
        """Clear the tokenizer and detokenizer timing lists."""
        self.result_mock.tokenizer_time.clear()
        self.result_mock.detokenizer_time.clear()


    @parameterized.expand([
        ("huggingface", "transformers"),
        ("mindformers", "mindformers"),
    ])
    def test_encode_with_record_false(self, _, tokenizer_type):
        self.clear_timing()
        BenchmarkTokenizer.init(self.model_paths.get(tokenizer_type), tokenizer_type=tokenizer_type)
        text = "Sample text"
        tokens = BenchmarkTokenizer.encode(text)

        # Verify that the encoding result is a non-empty list
        self.assertIsInstance(tokens, list)
        self.assertGreater(len(tokens), 0)

        # Check that the tokenizer_time array has not changed
        self.assertEqual(len(self.result_mock.tokenizer_time), 0)  # No records
        
    @parameterized.expand([
    ("huggingface", "transformers"),
    ("mindformers", "mindformers"),
    ])
    def test_decode_with_record_false(self, _, tokenizer_type):
        self.clear_timing()
        BenchmarkTokenizer.init(self.model_paths.get(tokenizer_type), tokenizer_type=tokenizer_type)
        tokens = [1, 2, 3]
        decoded_text = BenchmarkTokenizer.decode(tokens)

        # Verify that the decoding result is a non-empty string
        self.assertIsInstance(decoded_text, str)
        self.assertGreater(len(decoded_text), 0)

        # Check that the detokenizer_time array has not changed
        self.assertEqual(len(self.result_mock.detokenizer_time), 0)


    @parameterized.expand([
        ("huggingface", "transformers"),
        ("mindformers", "mindformers"),
    ])  # Mocking time for encoding
    def test_encode_with_record_true(self, _, tokenizer_type):
        self.clear_timing()
        BenchmarkTokenizer.init(self.model_paths.get(tokenizer_type), tokenizer_type=tokenizer_type)
        text = "Sample text"
        tokens = BenchmarkTokenizer.encode(text, tokenizer_time=self.result_mock.tokenizer_time)

        # Verify that the encoding result is a non-empty list
        self.assertIsInstance(tokens, list)
        self.assertGreater(len(tokens), 0)

        # Check the tokenizer_time array length
        self.assertEqual(len(self.result_mock.tokenizer_time), 1)

    @parameterized.expand([
        ("huggingface", "transformers"),
        ("mindformers", "mindformers"),
    ])
    # Mocking time for decoding
    def test_decode_with_record_true(self, _, tokenizer_type):
        self.clear_timing()
        BenchmarkTokenizer.init(self.model_paths.get(tokenizer_type), tokenizer_type=tokenizer_type)
        tokens = [1, 2, 3]
        decoded_text = BenchmarkTokenizer.decode(tokens, detokenizer_time=self.result_mock.detokenizer_time)

        # Verify that the decoding result is a non-empty string
        self.assertIsInstance(decoded_text, str)
        self.assertGreater(len(decoded_text), 0)

        # Check the detokenizer_time array length
        self.assertEqual(len(self.result_mock.detokenizer_time), 1)

    def tearDown(self):
        patch.stopall()

if __name__ == "__main__":
    unittest.main()