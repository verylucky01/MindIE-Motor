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

import os
import unittest
from unittest.mock import patch
from mindiebenchmark.common.logging import _filter_files
from benchmark.module import Logger, _complete_relative_path, _split_env_value

DEFAULT_VALUES = dict(
    LOG_ROTATE="-fs 20 -fc 64 -r 10",
    LOG_VERBOSE="true",
    LOG_PATH="~/mindie/log",
    LOG_LEVEL="INFO",
)


class TestLogger(unittest.TestCase):
    def setUp(self):
        self.logger = Logger()

    def test_get_benchmark_log_rotate_from_env_s_digit(self):
        input_env = dict(
            env_value="benchmark: -fc 4 -r 5 -fs 128",
            default_value=DEFAULT_VALUES["LOG_ROTATE"],
        )
        expected = {"fc": 4, "r": 5, "fs": 128 * 1024 * 1024}
        res = self.logger.get_benchmark_log_rotate_from_env(input_env, "LOG_ROTATE")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_rotate_from_env_s_str(self):
        input_env = dict(
            env_value="benchmark: -fc 100",
            default_value=DEFAULT_VALUES["LOG_ROTATE"],
        )
        expected = {"r": 10, "fc": 100, "fs": 20 * 1024 * 1024}
        res = self.logger.get_benchmark_log_rotate_from_env(input_env, "LOG_ROTATE")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_rotate_from_env_larger_key(self):
        input_env = dict(
            env_value="   Benchmark: -r 5 -fs 450 -fc 10; llm: -fs 450",
            default_value=DEFAULT_VALUES["LOG_ROTATE"],
        )

        expected = {"fc": 10, "r": 5, "fs": 450 * 1024 * 1024}
        res = self.logger.get_benchmark_log_rotate_from_env(input_env, "LOG_ROTATE")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_rotate_from_env_not_exactly(self):
        input_env = dict(
            env_value="-r 5 -fs 256",
            default_value=DEFAULT_VALUES["LOG_ROTATE"],
        )
        expected = {"fc": 64, "r": 5, "fs": 256 * 1024 * 1024}
        res = self.logger.get_benchmark_log_rotate_from_env(input_env, "LOG_ROTATE")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_rotate_from_env_str(self):
        input_env = dict(
            env_value="-r hello -fs eve",
            default_value=DEFAULT_VALUES["LOG_ROTATE"],
        )
        expected = {"fc": 64, "r": 10, "fs": 20 * 1024 * 1024}
        res = self.logger.get_benchmark_log_rotate_from_env(input_env, "LOG_ROTATE")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_rotate_from_env_too_large_file_size(self):
        input_env = dict(
            env_value="-r 5 -fs 1024; client: -r 4 -fs 20",
            default_value=DEFAULT_VALUES["LOG_ROTATE"],
        )
        with self.assertRaises(ValueError) as context:
            self.logger.get_benchmark_log_rotate_from_env(input_env, "LOG_ROTATE")
        self.assertEqual(
            str(context.exception),
            "Invalid MINDIE_LOG_ROTATE, file size should be in range [1, 500]MB. "
            "Please check environment value or benchmark config.json",
        )

    def test_get_benchmark_log_rotate_from_use_default(self):
        input_env = dict(
            env_value="benchmark; -r 5 -fs 1024",
            default_value=DEFAULT_VALUES["LOG_ROTATE"],
        )
        expected = {"fc": 64, "r": 10, "fs": 20 * 1024 * 1024}
        res = self.logger.get_benchmark_log_rotate_from_env(input_env, "LOG_ROTATE")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_rotate_from_env_raise_s(self):
        input_env = dict(
            env_value="benchmark: -r 5 -fs 1024",
            default_value=DEFAULT_VALUES["LOG_ROTATE"],
        )
        with self.assertRaises(ValueError) as context:
            self.logger.get_benchmark_log_rotate_from_env(input_env, "LOG_ROTATE")
        self.assertEqual(
            str(context.exception),
            "Invalid MINDIE_LOG_ROTATE, file size should be in range [1, 500]MB. "
            "Please check environment value or benchmark config.json",
        )

    def test_get_benchmark_log_bool_from_env_normal(self):
        input_env = dict(
            env_value=" benchmark: true ; llm: false   ",
            default_value=DEFAULT_VALUES["LOG_VERBOSE"],
        )
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_VERBOSE")
        self.assertTrue(res)

    def test_get_benchmark_log_bool_from_env_one_normal(self):
        input_env = dict(
            env_value=" benchmark: 1 ; llm: false   ",
            default_value=DEFAULT_VALUES["LOG_VERBOSE"],
        )
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_VERBOSE")
        self.assertTrue(res)

    def test_get_benchmark_log_bool_from_env_all(self):
        input_env = dict(env_value="true", default_value=DEFAULT_VALUES["LOG_VERBOSE"])
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_VERBOSE")
        self.assertTrue(res)

    def test_get_benchmark_log_bool_from_env_one_all(self):
        input_env = dict(env_value="1", default_value=DEFAULT_VALUES["LOG_VERBOSE"])
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_VERBOSE")
        self.assertTrue(res)

    def test_get_benchmark_log_bool_from_env_all_other(self):
        input_env = dict(
            env_value="true; llm: false", default_value=DEFAULT_VALUES["LOG_VERBOSE"]
        )
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_VERBOSE")
        self.assertTrue(res)

    def test_get_benchmark_log_bool_from_env_no_benchmark(self):
        input_env = dict(
            env_value="llm: true", default_value=DEFAULT_VALUES["LOG_VERBOSE"]
        )
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_VERBOSE")
        self.assertTrue(res)

    def test_get_benchmark_log_level_from_env_normal(self):
        input_env = dict(
            env_value="client: error; benchmark: debug",
            default_value=DEFAULT_VALUES["LOG_LEVEL"],
        )
        expected = "debug"
        res = self.logger.get_benchmark_log_string_from_env(input_env, "LOG_LEVEL")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_level_from_env_no_benchmark(self):
        input_env = dict(
            env_value="error; client: info", default_value=DEFAULT_VALUES["LOG_LEVEL"]
        )
        expected = "error"
        res = self.logger.get_benchmark_log_string_from_env(input_env, "LOG_LEVEL")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_level_from_env_all(self):
        input_env = dict(
            env_value="critical", default_value=DEFAULT_VALUES["LOG_LEVEL"]
        )
        expected = "critical"
        res = self.logger.get_benchmark_log_string_from_env(input_env, "LOG_LEVEL")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_level_from_unsupport(self):
        input_env = dict(env_value="true", default_value=DEFAULT_VALUES["LOG_LEVEL"])
        expected = "info"
        res = self.logger.get_benchmark_log_string_from_env(input_env, "LOG_LEVEL")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_to_stdout_true(self):
        input_env = dict(env_value="true", default_value=DEFAULT_VALUES["LOG_TO_STDOUT"])
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_TO_STDOUT")
        self.assertTrue(res)

    def test_get_benchmark_log_to_stdout_zero(self):
        input_env = dict(env_value="0", default_value=DEFAULT_VALUES["LOG_TO_STDOUT"])
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_TO_STDOUT")
        self.assertFalse(res)

    def test_get_benchmark_log_to_stdout_unsupport(self):
        input_env = dict(env_value="llm:f;server:3", default_value=DEFAULT_VALUES["LOG_TO_STDOUT"])
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_TO_STDOUT")
        self.assertFalse(res)

    def test_get_benchmark_log_to_stdout_from_benchmark(self):
        input_env = dict(env_value="benchmark:True;llm:false", default_value=DEFAULT_VALUES["LOG_TO_STDOUT"])
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_TO_STDOUT")
        self.assertTrue(res)

    def test_get_benchmark_log_to_stdout_from_all(self):
        input_env = dict(env_value="benchmark:True;false", default_value=DEFAULT_VALUES["LOG_TO_STDOUT"])
        res = self.logger.get_benchmark_log_bool_from_env(input_env, "LOG_TO_STDOUT")
        self.assertFalse(res)

    def test_get_benchmark_log_path_from_env_no_benchmark(self):
        input_env = dict(
            env_value="llm: /home/working", default_value=DEFAULT_VALUES["LOG_PATH"]
        )
        expected = "~/mindie/log"
        res = self.logger.get_benchmark_log_string_from_env(input_env, "LOG_PATH")
        self.assertEqual(res, expected)

    def test_get_benchmark_log_path_from_env_default(self):
        input_env = dict(
            env_value="~/mindie/log; llm: /home/local",
            default_value=DEFAULT_VALUES["LOG_PATH"],
        )
        expected = "~/mindie/log"
        res = self.logger.get_benchmark_log_string_from_env(input_env, "LOG_PATH")
        self.assertEqual(res, expected)

    def test_complete_relative_path_point(self):
        input_path = "./local"
        base_dir = "/root/mindie/log"
        expected = "/root/mindie/log/local"
        self.assertEqual(_complete_relative_path(input_path, base_dir), expected)

    def test_complete_relative_path_abs(self):
        my_path = "/root/mindie/log"
        input_path = my_path
        base_dir = my_path
        expected = my_path
        self.assertEqual(_complete_relative_path(input_path, base_dir), expected)

    def test_complete_relative_path_point_point(self):
        input_path = "../local"
        base_dir = "/root/mindie/log"
        expected = "/root/mindie/local"
        self.assertEqual(_complete_relative_path(input_path, base_dir), expected)

    def test_split_env_value(self):
        input_env = "true; llm: false"
        expected_dict = {"llm": "false"}
        expected_str = "true"
        my_dict, common_setting = _split_env_value(input_env)
        self.assertEqual((my_dict, common_setting), (expected_dict, expected_str))

    def test_logger_init_defalut(self):
        self.assertIsNotNone(Logger())

    def test_logger_init(self):
        from mindiebenchmark.common.logging import LogParams

        logparams = LogParams(to_file=True, to_console=False, verbose=False)
        self.assertIsNotNone(Logger(logparams))

    def tearDown(self):
        del self.logger


class TestFilterFiles(unittest.TestCase):

    @patch("os.listdir")
    @patch("os.path.getmtime")
    @patch("os.remove")
    def test_no_files_to_delete(self, mock_remove, mock_getmtime, mock_listdir):
        """Test when there are fewer files than the max_num, no files should be deleted."""
        # Simulating a directory with 2 files
        mock_listdir.return_value = ["file1.txt", "file2.txt"]
        mock_getmtime.return_value = 100  # Same mtime for all files

        _filter_files("/mock/dir", "file", 5)  # max_num is greater than number of files

        # Ensure no files are removed
        mock_remove.assert_not_called()

    @patch("os.listdir")
    @patch("os.path.getmtime")
    @patch("os.remove")
    def test_delete_oldest_files(self, mock_remove, mock_getmtime, mock_listdir):
        """Test when there are more files than max_num, the oldest files should be deleted."""
        # Simulating a directory with 5 files
        mock_listdir.return_value = [
            "file1.txt",
            "file2.txt",
            "file3.txt",
            "file4.txt",
            "file5.txt",
        ]

        # Simulate different mtime for files (older files have smaller mtime)
        mock_getmtime.side_effect = [100, 200, 300, 400, 500]  # Increasing mtime

        _filter_files("/mock/dir", "file", 3)  # Keep only 3 files

        # Ensure the oldest 2 files are removed
        mock_remove.assert_any_call("/mock/dir/file1.txt")
        mock_remove.assert_any_call("/mock/dir/file2.txt")
        self.assertEqual(mock_remove.call_count, 2)

    @patch("os.listdir")
    @patch("os.path.getmtime")
    @patch("os.remove")
    def test_no_files_matching_prefix(self, mock_remove, mock_getmtime, mock_listdir):
        """Test when no files match the prefix, no files should be deleted."""
        # Simulating a directory with 3 files, but none match the prefix 'other'
        mock_listdir.return_value = ["file1.txt", "file2.txt", "file3.txt"]

        _filter_files("/mock/dir", "other", 3)  # No files should be deleted

        # Ensure no files are removed
        mock_remove.assert_not_called()

    @patch("os.listdir")
    @patch("os.path.getmtime")
    @patch("os.remove")
    def test_files_with_same_mtime(self, mock_remove, mock_getmtime, mock_listdir):
        """Test when multiple files have the same mtime, we still delete the correct number."""
        # Simulating a directory with 5 files, all with the same mtime
        mock_listdir.return_value = [
            "file1.txt",
            "file2.txt",
            "file3.txt",
            "file4.txt",
            "file5.txt",
        ]
        mock_getmtime.return_value = 100

        _filter_files("/mock/dir", "file", 3)  # Keep only 3 files

        # Ensure the oldest files are deleted (in this case, it's arbitrary since mtime is the same)
        mock_remove.assert_any_call("/mock/dir/file1.txt")
        mock_remove.assert_any_call("/mock/dir/file2.txt")
        self.assertEqual(mock_remove.call_count, 2)


if __name__ == "__main__":
    unittest.main()
