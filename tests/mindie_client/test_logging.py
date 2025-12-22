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
import pytest
from mindieclient.python.common.logging import (
    Log, _complete_relative_path, CustomRotatingFileHandler, _filter_files
)


class TestLogger(unittest.TestCase):
    def setUp(self):
        patch("mindieclient.python.common.config.Config._check_file_permission",
                    return_value=(True, None)).start()
        self.logger = Log()

    def test_get_client_log_rotate_from_env_s_digit(self):
        input_env = {'env_value': 'client: -fc 64 -r 5 -fs 134217728', 'default_value': '-fc 64 -r 5 -fs 100'}
        expected = {
            'fc': 64,
            'r': 5,
            'fs': 134217728
        }
        with pytest.raises(ValueError, match="LOG_ROTATE"):
            res = self.logger.get_client_log_rotate_from_env(input_env, parse_type="LOG_ROTATE")

    def test_get_client_log_rotate_from_env_s_str(self):
        input_env = {'env_value': 'client: -r 10 -fc 100 -fs 20971520', 'default_value': '-fc 64 -r 5 -fs 100'}
        expected = {
            'r': 10,
            'fc': 100,
            'fs': 20971520
        }
        with pytest.raises(ValueError, match="LOG_ROTATE"):
            res = self.logger.get_client_log_rotate_from_env(input_env, parse_type="LOG_ROTATE")

    def test_get_client_log_rotate_from_env_larger_key(self):
        input_env = {'env_value': '   client: -fc 10 -r 5 -fs 524288000', 'default_value': '-fc 64 -r 5 -fs 100'}
        expected = {
            'fc': 10,
            'r': 5,
            'fs': 524288000
        }
        with pytest.raises(ValueError, match="LOG_ROTATE"):
            res = self.logger.get_client_log_rotate_from_env(input_env, parse_type="LOG_ROTATE")

    def test_get_client_log_rotate_from_env_not_exactly(self):
        input_env = {'env_value': '-fc 64 -r 5 -fs 1125899906842624', 'default_value': '-fc 64 -r 5 -fs 100'}
        expected = {
            'fc': 64,
            'r': 5,
            'fs': 1125899906842624
        }
        res = self.logger.get_client_log_rotate_from_env(input_env, parse_type="LOG_ROTATE")
        self.assertNotEqual(res, expected)

    def test_get_client_log_rotate_from_env_too_large_file_size(self):
        input_env = {'env_value': '-s daily -r 5 -fs 1024; benchmark: -s daily -r 4 -fs 20 -parse_type LOG_LEVEL',
            'default_value': '-s daily -r 5 -fs 100'}
        self.assertRaises(ValueError, self.logger.get_client_log_rotate_from_env, input_env, parse_type="LOG_ROTATE")

    def test_get_client_log_rotate_from_env_raise_env(self):
        input_env = {'env_value': 'client; -s daily -r 5 -fs 1024 -parse_type LOG_LEVEL',
            'default_value': '-s daily -r 5 -fs 100'}
        self.logger.get_client_log_rotate_from_env(input_env, parse_type="LOG_ROTATE")

    def test_get_client_log_rotate_from_env_raise_s(self):
        input_env = {'env_value': 'client: -s year -r 5 -fs 1024 -parse_type LOG_LEVEL',
            'default_value': '-s daily -r 5 -fs 100'}
        self.assertRaises(ValueError, self.logger.get_client_log_rotate_from_env, input_env, parse_type="LOG_ROTATE")

    def test_get_client_log_bool_from_env_normal(self):
        input_env = {'env_value': ' client: true ; llm: false   ', 'default_value': 'true'}
        res = self.logger.get_client_log_bool_from_env(input_env, 'LOG_VERBOSE')
        self.assertTrue(res)

    def test_get_client_log_bool_from_env_one_normal(self):
        input_env = {'env_value': ' client: 1 ; llm: false   ', 'default_value': 'true'}
        res = self.logger.get_client_log_bool_from_env(input_env, 'LOG_VERBOSE')
        self.assertTrue(res)

    def test_get_client_log_bool_from_env_all(self):
        input_env = {'env_value': 'true', 'default_value': 'true'}
        res = self.logger.get_client_log_bool_from_env(input_env, 'LOG_VERBOSE')
        self.assertTrue(res)

    def test_get_client_log_bool_from_env_one_all(self):
        input_env = {'env_value': '1', 'default_value': 'true'}
        res = self.logger.get_client_log_bool_from_env(input_env, 'LOG_VERBOSE')
        self.assertTrue(res)

    def test_get_client_log_bool_from_env_all_other(self):
        input_env = {'env_value': 'true; llm: false', 'default_value': 'true'}
        res = self.logger.get_client_log_bool_from_env(input_env, 'LOG_VERBOSE')
        self.assertTrue(res)

    def test_get_client_log_bool_from_env_no_client(self):
        input_env = {'env_value': 'llm: true', 'default_value': 'true'}
        res = self.logger.get_client_log_bool_from_env(input_env, 'LOG_VERBOSE')
        self.assertTrue(res)

    def test_get_client_log_level_from_env_normal(self):
        input_env = {'env_value': 'client: error; client: debug', 'default_value': 'info'}
        expected = 'debug'
        res = self.logger.get_client_log_string_from_env(input_env, 'LOG_LEVEL')
        self.assertEqual(res, expected)

    def test_get_client_log_level_from_env_no_client(self):
        input_env = {'env_value': 'error; benchmark: info', 'default_value': 'info'}
        expected = 'error'
        res = self.logger.get_client_log_string_from_env(input_env, 'LOG_LEVEL')
        self.assertEqual(res, expected)

    def test_get_client_log_level_from_env_all(self):
        input_env = {'env_value': 'critical', 'default_value': 'info'}
        expected = 'critical'
        res = self.logger.get_client_log_string_from_env(input_env, 'LOG_LEVEL')
        self.assertEqual(res, expected)

    def test_get_client_log_path_from_env_no_client(self):
        input_env = {'env_value': 'llm: /home/working', 'default_value': '~/mindie/log'}
        expected = ('~/mindie/log')
        res = self.logger.get_client_log_string_from_env(input_env, 'LOG_PATH')
        self.assertEqual(res, expected)

    def test_get_client_log_path_from_env_default(self):
        input_env = {'env_value': '~/mindie/log; llm: /home/local', 'default_value': 'info'}
        expected = ('~/mindie/log')
        res = self.logger.get_client_log_string_from_env(input_env, 'LOG_PATH')
        self.assertEqual(res, expected)

    def test_complete_relative_path_point(self):
        input_path = './local'
        base_dir = '/root/mindie/log'
        expected = '/root/mindie/log/local'
        self.assertEqual(_complete_relative_path(input_path, base_dir), expected)

    def test_complete_relative_path_abs(self):
        my_path = '/root/mindie/log'
        input_path = my_path
        base_dir = my_path
        expected = my_path
        self.assertEqual(_complete_relative_path(input_path, base_dir), expected)

    def test_complete_relative_path_point_point(self):
        input_path = '../local'
        base_dir = '/root/mindie/log'
        expected = '/root/mindie/local'
        self.assertEqual(_complete_relative_path(input_path, base_dir), expected)

    def test_split_env_value(self):
        input_env = 'true; llm: false'
        expected_dict = 'client'
        expected_str = 'llm'
        my_dict, common_setting = Log._split_env_value(input_env)
        self.assertEqual((my_dict, common_setting), (expected_dict, expected_str))

    def test_rotating_file_handler_rotate(self):
        with patch("os.stat") as mock_stat, \
            patch("os.chmod") as mock_chmod, \
            patch("os.listdir") as mock_listdir, \
            patch("logging.handlers.BaseRotatingHandler.rotate") as mock_file_rotate:
            file_handler = CustomRotatingFileHandler(filename="logger_test",
                                        file_per_process=10,
                                        mode='a',
                                        maxBytes=1024,
                                        backupCount=2)            
            mock_stat.return_value.st_mode = 440
            mock_chmod.return_value = None
            mock_listdir.return_value = []
            mock_file_rotate.return_value = None
            self.assertIsNone(file_handler.rotate("source", "dest"))

    def test_rotating_file_handler_close(self):
        with patch("os.stat") as mock_stat, \
            patch("os.path") as mock_path, \
            patch("os.chmod") as mock_chmod, \
            patch("os.listdir") as mock_listdir, \
            patch("os.rename") as mock_rename, \
            patch("logging.handlers.BaseRotatingHandler.rotate") as mock_file_rotate:
            file_handler = CustomRotatingFileHandler(filename="logger_test",
                                        file_per_process=10,
                                        mode='a',
                                        maxBytes=1024,
                                        backupCount=1)
            mock_stat.return_value.st_mode = 440
            mock_chmod.return_value = None
            mock_path.dirname.return_value = "cur_dir"
            mock_listdir.return_value = []
            os.rename.return_value = None
            mock_file_rotate.return_value = None
            self.assertIsNone(file_handler.close())

    def tearDown(self):
        del self.logger


class TestFilterFiles(unittest.TestCase):
    
    @patch('os.listdir')
    @patch('os.path.getmtime')
    @patch('os.remove')
    def test_no_files_to_delete(self, mock_remove, mock_getmtime, mock_listdir):
        """Test when there are fewer files than the max_num, no files should be deleted."""
        # Simulating a directory with 2 files
        mock_listdir.return_value = ['file1.txt', 'file2.txt']
        mock_getmtime.return_value = 100  # Same mtime for all files
        
        _filter_files('/mock/dir', 'file', 5)  # max_num is greater than number of files
        
        # Ensure no files are removed
        mock_remove.assert_not_called()
        
    @patch('os.listdir')
    @patch('os.path.getmtime')
    @patch('os.remove')
    def test_delete_oldest_files(self, mock_remove, mock_getmtime, mock_listdir):
        """Test when there are more files than max_num, the oldest files should be deleted."""
        # Simulating a directory with 5 files
        mock_listdir.return_value = ['file1.txt', 'file2.txt', 'file3.txt', 'file4.txt', 'file5.txt']
        
        # Simulate different mtime for files (older files have smaller mtime)
        mock_getmtime.side_effect = [100, 200, 300, 400, 500]  # Increasing mtime
        
        _filter_files('/mock/dir', 'file', 3)  # Keep only 3 files
        
        # Ensure the oldest 2 files are removed
        mock_remove.assert_any_call('/mock/dir/file1.txt')
        mock_remove.assert_any_call('/mock/dir/file2.txt')
        self.assertEqual(mock_remove.call_count, 2)
        
    @patch('os.listdir')
    @patch('os.path.getmtime')
    @patch('os.remove')
    def test_no_files_matching_prefix(self, mock_remove, mock_getmtime, mock_listdir):
        """Test when no files match the prefix, no files should be deleted."""
        # Simulating a directory with 3 files, but none match the prefix 'other'
        mock_listdir.return_value = ['file1.txt', 'file2.txt', 'file3.txt']
        
        _filter_files('/mock/dir', 'other', 3)  # No files should be deleted
        
        # Ensure no files are removed
        mock_remove.assert_not_called()

    @patch('os.listdir')
    @patch('os.path.getmtime')
    @patch('os.remove')
    def test_files_with_same_mtime(self, mock_remove, mock_getmtime, mock_listdir):
        """Test when multiple files have the same mtime, we still delete the correct number."""
        # Simulating a directory with 5 files, all with the same mtime
        mock_listdir.return_value = ['file1.txt', 'file2.txt', 'file3.txt', 'file4.txt', 'file5.txt']
        mock_getmtime.return_value = 100
        
        _filter_files('/mock/dir', 'file', 3)  # Keep only 3 files
        
        # Ensure the oldest files are deleted (in this case, it's arbitrary since mtime is the same)
        mock_remove.assert_any_call('/mock/dir/file1.txt')
        mock_remove.assert_any_call('/mock/dir/file2.txt')
        self.assertEqual(mock_remove.call_count, 2)


if __name__ == "__main__":
    unittest.main()