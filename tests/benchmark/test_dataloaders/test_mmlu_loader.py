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
from queue import Queue
from unittest.mock import patch

import pandas as pd
from mindiebenchmark.common.datasetloaders import DatasetLoaderFactory
from mindiebenchmark.common.results import ResultData

TEST_MOD_PATH = "mindiebenchmark.common.datasetloaders.mmlu_loader."
TEST_CLASS_PATH = "mindiebenchmark.common.datasetloaders.mmlu_loader.MmluLoader."


class TestMmluLoader(unittest.TestCase):
    def setUp(self):
        self.command_line_args = {
            "DatasetType": "test_dataset",
            "TestType": "client",
            "TestAccuracy": True,
            "Tokenizer": True,
            "TaskKind": "stream",
            "ModelName": "llama3-8b",
            "SaveTokensPath": "path/to/tokens",
            "SaveHumanEvalOutputFile": "path/to/output",
        }
        self.queue = Queue()
        self.result = ResultData(self.command_line_args)

        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'mmlu'
        self.loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)

    @patch(TEST_MOD_PATH + 'extract_dataset_type_info')
    def test_validate_dataset_args_with_valid_args(self, mock_extract_dataset_type_info):
        self.loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        # Simulate the behavior of extract_dataset_type_info
        mock_extract_dataset_type_info.return_value = ("dataset_name", ["5"])

        # Call the validate_dataset_args method
        self.loader.validate_dataset_args("dataset_name", {"arg1": "value"})

        # Assert that the SHOT_NUM is set correctly
        self.assertEqual(self.loader.SHOT_NUM, 5)

    def test_validate_dataset_args_with_invalid_args(self):
        # Define reusable string variables
        dataset_name = "dataset_name"
        arg1 = "arg1"
        arg2 = "arg2"
        value = "value"
        non_integer_value = "non-integer"
        max_shot_value = "100"

        # Test for ValueError when more than one argument is passed
        with self.assertRaises(ValueError):
            self.loader.validate_dataset_args(dataset_name, {arg1: value, arg2: value})

        # Test for ValueError when the argument is not a valid integer
        with self.assertRaises(ValueError):
            self.loader.validate_dataset_args(dataset_name, {arg1: value, arg2: non_integer_value})

        # Test for ValueError when shot number exceeds the maximum
        with self.assertRaises(ValueError):
            self.loader.validate_dataset_args(dataset_name, {arg1: value, arg2: max_shot_value})


    @patch(TEST_CLASS_PATH + 'splice_ques_options')
    def test_format_example_mmlu(self, mock_splice_ques_options):
        # Mock splice_ques_options to avoid dependency on it
        mock_splice_ques_options.return_value = "Mocked options"

        val_data = {"question": "What is 2+2?", "A": "3", "B": "4", "C": "5", "D": "6", "answer": "B"}
        sub_name = "math"
        dev_data = pd.DataFrame([{"question": "What is 1+1?", "A": "1", "B": "2", "C": "3", "D": "4", "answer": "B"}])

        result = self.loader.format_example_mmlu(val_data, sub_name, dev_data)

        # Check if the prompt formatting logic is correct
        expected_result = ("The following are multiple choice questions (with answers) about  math.\n\n"
                           "Mocked options\n"
                           "Mocked optionsAnswer:")
        self.assertEqual(result, expected_result)

    @patch(TEST_MOD_PATH + 'pd.read_csv')
    @patch(TEST_MOD_PATH + 'check_file')
    @patch(TEST_CLASS_PATH + 'put_data_dict')
    def test_read_file(self, mock_put_data_dict, mock_check_file, mock_read_csv):
        # Setup mock behavior
        mock_check_file.return_value = None  # Mock check_file does nothing
        mock_read_csv.return_value = pd.DataFrame({
            'question': ['What is 2+2?'],
            'A': ['3'], 'B': ['4'], 'C': ['5'], 'D': ['6'], 'answer': ['B']
        })

        self.loader.dataset_path = '/path/to/dataset'
        self.loader.stop = False
        self.loader.gt_answer = {}

        # Run the method under test
        self.assertIsNone(self.loader.read_file('/path/to/file.csv'))

    def test_get_file_posix(self):
        self.assertEqual(self.loader.get_file_posix(), self.loader.CSV_POSIX)

    def test_get_sub_dir_name(self):
        self.assertEqual(self.loader.get_sub_dir_name(), "test")


if __name__ == '__main__':
    unittest.main()
