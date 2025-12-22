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
import logging
import os
import tempfile
from datetime import datetime, timezone, timedelta
from pathlib import Path

from mindiesimulator.common.utils import Logger  


class TestLogger(unittest.TestCase):
    def setUp(self):
        """Reset Logger state using mocking instead of direct access"""
        # Use patchers to safely modify class state
        self.instance_patcher = patch.object(Logger, '_instance', None)
        self.init_patcher = patch.object(Logger, '_initialized', False)
        self.path_patcher = patch.object(Logger, '_log_file_path', None)
        
        self.instance_patcher.start()
        self.init_patcher.start()
        self.path_patcher.start()

    def tearDown(self):
        """Clean up patchers after each test"""
        self.instance_patcher.stop()
        self.init_patcher.stop()
        self.path_patcher.stop()

    def test_init_logger_initializes_correctly(self):
        """Test logger initialization with default parameters"""
        with patch('mindiesimulator.common.utils.datetime') as mock_dt:
            # Mock current time for predictable log filename
            mock_dt.now.return_value = datetime(2023, 1, 1, tzinfo=timezone.utc)
            logger = Logger.init_logger(save_to_file=True)
            
            # Verify logger properties through public interfaces
            self.assertEqual(logger.level, logging.INFO)
            handlers = logger.handlers
            self.assertEqual(len(handlers), 3)  # Stream + file handler

    def test_logger_is_singleton(self):
        """Test that multiple init_logger calls return same instance"""
        logger1 = Logger.init_logger()
        logger2 = Logger.init_logger()
        self.assertIs(logger1, logger2)

    def test_console_logging(self):
        """Test console logging output"""
        logger = Logger.init_logger()
        with self.assertLogs('GlobalLogger', level='INFO') as cm:
            logger.info("Test message")
            self.assertIn("Test message", cm.output[0])

    def test_log_file_creation(self):
        """Test log file creation and message writing"""
        with tempfile.TemporaryDirectory() as tmpdir:
            # Capture log path from logging output
            with self.assertLogs('GlobalLogger', level='INFO') as cm:
                Logger.init_logger(log_dir=tmpdir, exec_type="test", save_to_file=True)
                log_path = cm.records[-1].getMessage().split()[-1].strip('{}').strip('\'')
            
            # Verify file handler exists through handler types
            logger = Logger.get_logger()
            file_handlers = [h for h in logger.handlers if isinstance(h, logging.FileHandler)]
            self.assertEqual(len(file_handlers), 1)
            
    def test_get_logger_before_initialization(self):
        """Test that get_logger raises error before initialization"""
        with self.assertRaises(RuntimeError) as context:
            Logger.get_logger()
        self.assertIn("Logger not initialized", str(context.exception))

    def test_log_file_rotation_config(self):
        """Test file rotation parameters are correctly set"""
        with tempfile.TemporaryDirectory() as tmpdir:
            Logger.init_logger(log_dir=tmpdir, save_to_file=True)
            # Access handler through public handler list
            file_handler = next(h for h in Logger.get_logger().handlers 
                              if isinstance(h, logging.handlers.RotatingFileHandler))
            self.assertEqual(file_handler.maxBytes, 50 * 1024 * 1024)
            self.assertEqual(file_handler.backupCount, 5)

    @patch('mindiesimulator.common.utils.datetime', wraps=datetime)
    def test_timezone_handling(self, mock_dt):
        """Test correct timezone handling in timestamp"""

        def mock_now(tz):
            return datetime(2023, 1, 1, 8, 0, 0, tzinfo=timezone(timedelta(hours=8)))

        mock_dt.now.side_effect = mock_now
        # Capture log path from logging output
        with self.assertLogs('GlobalLogger', level='INFO') as cm:
            Logger.init_logger(save_to_file=True)
            log_path = cm.records[-1].getMessage().split()[-1].strip('{}').strip('\'')
        
        expected_path = os.path.join("./logs", "default_20230101_080000", "runtime.log")
        self.assertIn(log_path, expected_path)

if __name__ == '__main__':
    unittest.main()