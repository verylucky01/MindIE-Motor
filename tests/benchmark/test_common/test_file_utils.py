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
from unittest.mock import patch, MagicMock

from mindiebenchmark.common.file_utils import PathCheck, FileOperator

TEST_MOD_PATH = "mindiebenchmark.common.file_utils."
LOGGER_PATH = "mindiebenchmark.common.logging."


class TestPathCheck(unittest.TestCase):

    def setUp(self):
        patch.stopall()

    @patch("os.path.exists")
    def test_check_path_exists(self, mock_exists):
        mock_exists.return_value = True
        ret, msg = PathCheck.check_path_exists("/valid/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")

        mock_exists.return_value = False
        ret, msg = PathCheck.check_path_exists("/invalid/path")
        self.assertFalse(ret)
        self.assertEqual(msg, "The path is not exists.")

    @patch("os.path.islink")
    @patch("os.path.normpath")
    def test_check_path_link(self, mock_normpath, mock_islink):
        mock_normpath.return_value = "/valid/path"
        mock_islink.return_value = False
        ret, msg = PathCheck.check_path_link("/valid/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")

        mock_islink.return_value = True
        ret, msg = PathCheck.check_path_link("/link/path")
        self.assertFalse(ret)
        self.assertEqual(msg, "The path is a soft link.")

    def test_check_path_valid(self):
        # Test empty path
        ret, msg = PathCheck.check_path_valid("")
        self.assertFalse(ret)
        self.assertEqual(msg, "The path is empty.")

        # Test path length exceeding limit
        ret, msg = PathCheck.check_path_valid("a" * 1025)
        self.assertFalse(ret)
        self.assertEqual(msg, "The length of path exceeds 1024 characters.")

        # Test invalid characters in path
        ret, msg = PathCheck.check_path_valid("invalid|path")
        self.assertFalse(ret)
        self.assertEqual(msg, "The path contains special characters.")

        # Test valid path
        ret, msg = PathCheck.check_path_valid("valid/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")

    @patch("os.getuid")
    @patch("os.getgid")
    @patch("os.stat")
    def test_check_path_owner_group_valid(self, mock_stat, mock_getgid, mock_getuid):
        # Mocking os.getuid and os.getgid
        mock_getuid.return_value = 1000
        mock_getgid.return_value = 1000

        # Mocking os.stat
        mock_stat.return_value.st_uid = 1000
        mock_stat.return_value.st_gid = 1000

        ret, msg = PathCheck.check_path_owner_group_valid("/valid/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")

        # Simulating mismatch in user and group
        mock_stat.return_value.st_uid = 2000
        mock_stat.return_value.st_gid = 2000
        ret, msg = PathCheck.check_path_owner_group_valid("/invalid/path")
        self.assertFalse(ret)
        self.assertIn("Check the path owner failed.", msg)
        self.assertIn("Check the path group failed.", msg)

    @patch("os.path.getsize")
    def test_check_file_size(self, mock_getsize):
        mock_getsize.return_value = 5 * 1024 * 1024
        ret, msg = PathCheck.check_file_size("/valid/file", 10 * 1024 * 1024)
        self.assertTrue(ret)
        self.assertEqual(msg, "")

        mock_getsize.return_value = 15 * 1024 * 1024
        ret, msg = PathCheck.check_file_size("/large/file", 10 * 1024 * 1024)
        self.assertFalse(ret)
        self.assertEqual(msg, "Invalid file size, should be no more than 10485760 but got 15728640")

    @patch("os.path.realpath")
    def test_check_system_path(self, mock_realpath):
        mock_realpath.return_value = "/usr/bin/some/path"
        ret, msg = PathCheck.check_system_path("/usr/bin/some/path")
        self.assertFalse(ret)
        self.assertEqual(msg, "Invalid path, it is a system path.")

        mock_realpath.return_value = "/home/user/some/path"
        ret, msg = PathCheck.check_system_path("/home/user/some/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")


    def test_check_path_full(self):
        with patch("os.path.exists") as mock_exists, \
             patch("os.path.islink") as mock_islink, \
             patch("os.path.normpath") as mock_normpath, \
             patch("os.getuid") as mock_getuid, \
             patch("os.getgid") as mock_getgid, \
             patch("os.stat") as mock_stat:
            # Set up the mocks
            valid_path = "/valid/path"
            mock_exists.return_value = True
            mock_islink.return_value = False
            mock_normpath.return_value = valid_path
            mock_getuid.return_value = 1000
            mock_getgid.return_value = 1000
            mock_stat.return_value.st_uid = 1000
            mock_stat.return_value.st_gid = 1000

            # Test without size check
            ret, msg = PathCheck.check_path_full(valid_path, is_size_check=False)
            self.assertTrue(ret)
            self.assertEqual(msg, "")

            # Test with size check
            mock_stat.return_value.st_size = 5 * 1024 * 1024
            ret, msg = PathCheck.check_path_full(valid_path)
            self.assertTrue(ret)
            self.assertEqual(msg, "")

            # Test with size exceeding limit
            mock_stat.return_value.st_size = 15 * 1024 * 1024
            ret, msg = PathCheck.check_path_full("/large/path")
            self.assertFalse(ret)
            self.assertIn("Invalid file size", msg)

    def tearDown(self) -> None:
        patch.stopall()


class TestFileOperator(unittest.TestCase):

    @patch("os.chmod")
    @patch(TEST_MOD_PATH + "PathCheck.check_path_exists")
    def test_set_path_permission(self, mock_check_path_exists, mock_chmod):
        # Test when path exists
        mock_check_path_exists.return_value = (True, "")
        ret = FileOperator.set_path_permission("/valid/path", 0o755)
        self.assertTrue(ret)
        mock_chmod.assert_called_with("/valid/path", 0o755)

        # Test when path doesn't exist
        mock_check_path_exists.return_value = (False, "Path does not exist.")
        ret = FileOperator.set_path_permission("/invalid/path", 0o755)
        self.assertFalse(ret)

    @patch("os.open")
    @patch("os.fdopen")
    @patch(TEST_MOD_PATH + "Logger")
    @patch(TEST_MOD_PATH + "PathCheck.check_path_exists")
    @patch(TEST_MOD_PATH + "FileOperator.set_path_permission")
    def test_create_file(self, mock_set_path_permission, mock_check_path_exists, mock_logger, mock_fdopen,
                         mock_os_open):
        # Mock path does not exist
        mock_check_path_exists.return_value = (False, "")
        mock_set_path_permission.return_value = True
        mock_os_open.return_value = MagicMock()

        # Test file creation when file does not exist
        ret = FileOperator.create_file("/new/file", 0o640)
        self.assertTrue(ret)
        mock_fdopen.assert_called()
        mock_set_path_permission.assert_called_with("/new/file", 0o640)

        # Test when file exists
        mock_check_path_exists.return_value = (True, "File exists.")
        ret = FileOperator.create_file("/existing/file", 0o640)
        mock_logger().logger.warning.assert_called_with('[MIE01W000000] File exists.')

    @patch("os.makedirs")
    @patch(TEST_MOD_PATH + "PathCheck.check_path_valid")
    @patch(TEST_MOD_PATH + "PathCheck.check_system_path")
    @patch(TEST_MOD_PATH + "PathCheck.check_path_owner_group_valid")
    @patch(TEST_MOD_PATH + "FileOperator.set_path_permission")
    def test_create_dir(self, mock_set_path_permission, mock_check_path_owner_group_valid, mock_check_system_path,
                        mock_check_path_valid, mock_makedirs):
        # Test valid directory creation
        new_dir_path = "/new/dir"
        mock_check_path_valid.return_value = (True, "")
        mock_check_system_path.return_value = (True, "")
        mock_check_path_owner_group_valid.return_value = (True, "")
        mock_set_path_permission.return_value = True
        mock_makedirs.return_value = None

        ret = FileOperator.create_dir(new_dir_path, 0o750)
        self.assertTrue(ret)
        mock_makedirs.assert_called_with(new_dir_path, mode=0o750, exist_ok=True)
        mock_set_path_permission.assert_called_with(new_dir_path, 0o750)

        # Test invalid directory path
        mock_check_path_valid.return_value = (False, "Invalid path")
        ret = FileOperator.create_dir("/invalid/dir", 0o750)
        self.assertFalse(ret)

        # Test system path check
        mock_check_path_valid.return_value = (True, "")
        mock_check_system_path.return_value = (False, "System path")
        ret = FileOperator.create_dir("/usr/bin/dir", 0o750)
        self.assertFalse(ret)

        # Test owner/group check failure
        mock_check_path_owner_group_valid.return_value = (False, "Owner/group mismatch")
        ret = FileOperator.create_dir(new_dir_path, 0o750)
        self.assertFalse(ret)

    @patch("os.chmod")
    @patch(TEST_MOD_PATH + "Logger")
    @patch(TEST_MOD_PATH + "PathCheck.check_path_exists")
    def test_create_file_logging(self, mock_check_path_exists, mock_logger, mock_os_chmod):
        # Test logging when file creation fails
        mock_check_path_exists.return_value = (True, "File exists.")
        mock_logger.return_value = MagicMock()

        ret = FileOperator.create_file("/existing/file", 0o640)
        mock_logger().logger.warning.assert_called_with('[MIE01W000000] File exists.')

    @patch(TEST_MOD_PATH + "Logger")
    @patch(TEST_MOD_PATH + "PathCheck.check_path_exists")
    def test_set_path_permission_logging(self, mock_check_path_exists, mock_logger):
        # Test logging when setting path permission fails
        mock_check_path_exists.return_value = (False, "Path does not exist.")
        mock_logger.return_value = MagicMock()

        ret = FileOperator.set_path_permission("/invalid/path", 0o755)
        self.assertFalse(ret)
        mock_logger().logger.warning.assert_called_with('[MIE01W000000] Path does not exist.')

    @patch(TEST_MOD_PATH + "Logger")
    @patch(TEST_MOD_PATH + "PathCheck.check_path_valid")
    @patch(TEST_MOD_PATH + "PathCheck.check_system_path")
    @patch(TEST_MOD_PATH + "PathCheck.check_path_owner_group_valid")
    def test_create_dir_logging(self, mock_check_path_owner_group_valid,
                                mock_check_system_path, mock_check_path_valid, mock_logger):
        # Test logging when directory creation fails due to invalid path
        mock_check_path_valid.return_value = (False, "Invalid path")
        mock_logger.return_value = MagicMock()

        ret = FileOperator.create_dir("/invalid/dir", 0o750)
        self.assertFalse(ret)
        mock_logger().logger.error.assert_called_with('[MIE01E000000] Invalid path')

        # Test logging when directory creation fails due to system path
        mock_check_path_valid.return_value = (True, "")
        mock_check_system_path.return_value = (False, "System path")
        ret = FileOperator.create_dir("/usr/bin/dir", 0o750)
        self.assertFalse(ret)
        mock_logger().logger.error.assert_called_with('[MIE01E000000] System path')

        # Test logging when directory creation fails due to owner/group mismatch
        mock_check_system_path.return_value = (True, "")
        mock_check_path_owner_group_valid.return_value = (False, "Owner/group mismatch")
        ret = FileOperator.create_dir("/new/dir", 0o750)
        self.assertFalse(ret)
        mock_logger().logger.error.assert_called_with('[MIE01E000000] Owner/group mismatch')

    def tearDown(self) -> None:
        patch.stopall()


if __name__ == "__main__":
    unittest.main()
