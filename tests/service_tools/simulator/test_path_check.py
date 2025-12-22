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
from unittest.mock import patch, mock_open, MagicMock

from mindiesimulator.common.path_check import PathCheck


class TestPathCheck(unittest.TestCase):

    def test_check_path_valid(self):
        # Test valid path
        self.assertEqual(PathCheck.check_path_valid("/valid/path"), (True, ""))
        
        # Test empty path
        self.assertEqual(PathCheck.check_path_valid(""), 
                         (False, "The path is empty."))
        
        # Test path with invalid characters
        self.assertEqual(PathCheck.check_path_valid("/invalid@path"), 
                         (False, "The path contains invalid characters."))
        
        # Test path with directory traversal
        self.assertEqual(PathCheck.check_path_valid("../traversal"), 
                         (False, "The path contains invalid characters."))
        
        # Test long path
        long_path = "a" * 1025
        self.assertEqual(PathCheck.check_path_valid(long_path),
                         (False, "The length of the path exceeds 1024 characters."))

    @patch('os.path.exists')
    def test_check_path_exists(self, mock_exists):
        mock_exists.return_value = True
        self.assertEqual(PathCheck.check_path_exists("/exists"), (True, ""))
        
        mock_exists.return_value = False
        self.assertEqual(PathCheck.check_path_exists("/missing"), 
                         (False, "The path does not exist."))

    @patch('os.path.islink')
    def test_check_path_link(self, mock_islink):
        mock_islink.return_value = True
        self.assertEqual(PathCheck.check_path_link("/symlink"), 
                         (False, "The path is a symbolic link."))
        
        mock_islink.return_value = False
        self.assertEqual(PathCheck.check_path_link("/regular"), (True, ""))

    @patch('os.stat')
    @patch('os.getuid', return_value=1000)
    @patch('os.getgid', return_value=1000)
    def test_check_path_owner_group_valid(self, mock_gid, mock_uid, mock_stat):
        # Mock file_stat with same uid/gid
        mock_stat.return_value.st_uid = 1000
        mock_stat.return_value.st_gid = 1000
        self.assertEqual(PathCheck.check_path_owner_group_valid("/owned"), 
                         (True, ""))
        
        # Test root support case
        mock_stat.return_value.st_uid = 0
        mock_stat.return_value.st_gid = 0
        self.assertEqual(PathCheck.check_path_owner_group_valid("/root", True), 
                         (True, ""))
        
        # Test invalid owner and group
        mock_stat.return_value.st_uid = 2000
        mock_stat.return_value.st_gid = 3000
        result, msg = PathCheck.check_path_owner_group_valid("/invalid")
        self.assertFalse(result)
        self.assertIn("Incorrect path owner", msg)
        self.assertIn("Incorrect path group", msg)

    @patch('os.path.realpath')
    def test_check_system_path(self, mock_realpath):
        # Test system path
        mock_realpath.return_value = "/usr/bin/test"
        self.assertEqual(PathCheck.check_system_path("/usr/bin"), 
                         (False, "Invalid path: it is a system path."))
        
        # Test non-system path
        mock_realpath.return_value = "/home/user"
        self.assertEqual(PathCheck.check_system_path("/home/user"), 
                         (True, ""))

    @patch('os.path.getsize')
    def test_check_file_size(self, mock_getsize):
        # Test valid size
        mock_getsize.return_value = 5 * 1024 * 1024
        self.assertEqual(PathCheck.check_file_size("/small_file"), (True, ""))
        
        # Test large file
        mock_getsize.return_value = 15 * 1024 * 1024
        result, msg = PathCheck.check_file_size("/large_file", 10 * 1024 * 1024)
        self.assertFalse(result)
        self.assertIn("15728640", msg)  # 15MB in bytes
        
        # Test missing file
        mock_getsize.side_effect = FileNotFoundError
        self.assertEqual(PathCheck.check_file_size("/missing"), (True, ""))

    @patch('mindiesimulator.common.path_check.PathCheck.validate_path')
    def test_check_directory_path(self, mock_validate):
        mock_validate.return_value = (True, "")
        self.assertEqual(PathCheck.check_directory_path("/dir"), (True, ""))
        
        mock_validate.return_value = (False, "error")
        self.assertFalse(PathCheck.check_directory_path("/dir")[0])

    @patch('os.path.exists', return_value=True)
    @patch('os.path.islink', return_value=False)
    @patch('mindiesimulator.common.path_check.PathCheck.check_path_owner_group_valid', return_value=(True, ""))
    @patch('mindiesimulator.common.path_check.PathCheck.check_file_size', return_value=(True, ""))
    def test_check_path_full_read_mode(self, mock_size, mock_owner, mock_link, mock_exists):
        # Test successful read mode validation
        self.assertTrue(PathCheck.check_path_full("/valid")[0])
        
        # Test failed validation
        mock_exists.return_value = False
        self.assertFalse(PathCheck.check_path_full("/invalid")[0])

    @patch('os.path.exists')
    @patch('os.path.islink')
    def test_check_path_full_write_mode(self, mock_link, mock_exists):
        # Test write mode skips some checks
        with patch('mindiesimulator.common.path_check.PathCheck.check_path_owner_group_valid') as mock_owner:
            PathCheck.check_path_full("/write", is_write_mode=True)
            mock_owner.assert_not_called()

    @patch('mindiesimulator.common.path_check.PathCheck.check_path_full')
    def test_safe_open(self, mock_check):
        mock_check.return_value = (True, "")
        mock_open_func = mock_open()
        
        with patch('builtins.open', mock_open_func):
            f = PathCheck.safe_open("/valid", 'r')
            self.assertIsNotNone(f)
            mock_open_func.assert_called_once()

    @patch('mindiesimulator.common.path_check.PathCheck.check_path_full')
    def test_safe_open_validation_failure(self, mock_check):
        mock_check.return_value = (False, "Validation error")
        
        with self.assertRaises(FileNotFoundError) as context:
            PathCheck.safe_open("/invalid", 'r')
        self.assertIn("Validation error", str(context.exception))

if __name__ == '__main__':
    unittest.main()