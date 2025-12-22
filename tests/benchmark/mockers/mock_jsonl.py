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

from unittest.mock import mock_open, MagicMock, patch

mock_jsonl_data = [
    {"request_id": "1", "forward": 0.1, "sample": 0.2},
    {"request_id": "2", "forward": 0.3, "sample": 0.4},
]

gsm8k_data = [
    {
        "question": "mock question 1",
        "answer": "mock answer 1"
    },
    {
        "question": "mock question 2",
        "answer": "mock answer 2"
    },
    {
        "question": "mock question 3",
        "answer": "mock answer 3"
    },
]

tokenid_data = ["0, 1, 2", "3, 4, 5"]


class JsonlinesMocker:
    def __init__(self, read_data=None):
        self.read_data = read_data or []
        self.written_data = []
        # Create a mock for jsonlines.open
        self.mock_open = mock_open()
        self.patch = None

    def __enter__(self):
        # Create a mock for the reader
        mock_reader = MagicMock()
        mock_reader.__iter__.return_value = iter(self.read_data)  # Make it iterable
        mock_reader.__enter__.return_value = mock_reader

        # Create a mock for the writer
        mock_writer = MagicMock()
        mock_writer.write.side_effect = self.written_data.append  # Capture written data
        mock_writer.__enter__.return_value = mock_writer

        # Set up the mock_open return value
        self.mock_open.side_effect = [mock_reader, mock_writer]  # Mock reader first, then writer

        # Patch jsonlines.open
        self.patch = patch('jsonlines.open', self.mock_open)
        self.patch.start()

        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.patch.stop()

    def get_written_data(self):
        return self.written_data

    def clear_written_data(self):
        self.written_data.clear()
