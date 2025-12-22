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

import argparse
import logging
import os
import unittest
from unittest.mock import patch, MagicMock
from parameterized import parameterized

from mock_config import MockBenchmarkConfig
from mock_jsonl import JsonlinesMocker, gsm8k_data
from mock_object import MockBenchmarkTokenizer
from mindiebenchmark.common.logging import Logger
from mindiebenchmark.common.tokenizer import BenchmarkTokenizer
from benchmark.module import (
    BenchmarkRun, server_mocker, main
)


logger = logging.getLogger(__file__)
TEST_MOD_PATH = "mindiebenchmark.benchmark_run."
MIE_CLIENT_MOD_PATH = 'mindieclient.python.httpclient.clients.base_client.'


class TestBenchmarker(unittest.TestCase):
    def mock_file_checks(self) -> None:
        from mindiebenchmark.common.file_utils import PathCheck
        from mindiebenchmark.common.results import ResultData
        patch.multiple(
            PathCheck,
            check_path_valid=MagicMock(return_value=(True, "")),
            check_path_link=MagicMock(return_value=(True, "")),
            check_path_owner_group_valid=MagicMock(return_value=(True, "")),
            check_path_mode=MagicMock(return_value=(True, "")),
            check_system_path=MagicMock(return_value=(True, "")),
            check_file_size=MagicMock(return_value=(True, ""))
        ).start()

        patch("mindiebenchmark.common.config.Config._check_file_permission",
              return_value=(True, None)).start()

        patch("mindieclient.python.common.config.Config._check_file_permission",
                            return_value=(True, None)).start()

        patch.multiple(
            ResultData,
            save_request_id_dict=MagicMock(),
            save_eval_results=MagicMock(),
            save_humaneval_output=MagicMock()
        ).start()

    def mock_tokenizer(self):
        patch.object(BenchmarkTokenizer, 'init', MockBenchmarkTokenizer.init).start()
        patch.object(BenchmarkTokenizer, 'encode', MockBenchmarkTokenizer.encode).start()
        patch.object(BenchmarkTokenizer, 'decode', MockBenchmarkTokenizer.decode).start()

        MockBenchmarkTokenizer.init("MockPath")

    def setUp(self) -> None:
        # setup common command line config
        self.current_dir = os.path.dirname(os.path.abspath(__file__))
        self.benchmark_config_path = os.path.join(self.current_dir, 'config', 'benchmark_config.json')
        self.benchmark_output_path = os.path.join(self.current_dir, 'save_token.csv')
        os.environ['MIES_INSTALL_PATH'] = os.path.join(self.current_dir, 'mockers')
        os.environ['MINDIE_LLM_HOME_PATH'] = os.path.join(self.current_dir)
        self.base_config = MockBenchmarkConfig().update("ConfigPath", self.benchmark_config_path).update(
            "SaveTokensPath", self.benchmark_output_path)

        os.environ['MIES_INSTALL_PATH'] = os.path.join(self.current_dir, 'mockers')
        os.environ['MINDIE_LLM_HOME_PATH'] = os.path.join(self.current_dir)

        self.patcher_request = patch(MIE_CLIENT_MOD_PATH + 'urllib3.PoolManager.request', side_effect=server_mocker)
        self.mock_get = self.patcher_request.start()

        self.mock_file_checks()
        self.mock_tokenizer()

    def test_init(self) -> None:
        # 将Benchmark的command_lines mock掉
        test_commandline_config = self.base_config.get_config()

        patch("mindiebenchmark.benchmark_run.BenchmarkRun.run_engine",
            return_value=True).start()

        patch("mindiebenchmark.benchmark_run.BenchmarkRun.run_client",
            return_value=True).start()

        with patch('mindiebenchmark.common.config.argparse.ArgumentParser.parse_args',
                   return_value=argparse.Namespace(**test_commandline_config)):
            self.assertIsNone(main())

    @parameterized.expand([
        ("text",),
        ("stream",),
    ])
    def test_run_mindie(self, task_kind) -> None:
        test_config = self.base_config.update("TaskKind", task_kind)
        mocker = JsonlinesMocker()
        mocker.__enter__()
        # set up mock
        mocker.read_data = gsm8k_data

        test_commandline_config = test_config.get_config()
        with patch('mindiebenchmark.common.config.argparse.ArgumentParser.parse_args',
                   return_value=argparse.Namespace(**test_commandline_config)):
            with patch.object(Logger, 'logger', autospec=True) as mock_logger:
                # Call the code that should not trigger logger.error
                self.assertIsNone(main())
                # Assert that error was not called
                mock_logger.error.assert_not_called()

    def test_run_mindie_stream_token(self):
        test_config = self.base_config.update("TaskKind", "stream_token")
        test_config = test_config.update("Tokenizer", False)
        tokenid_data = [[1, 2, 3], [4, 5, 6], [7, 8, 9], ]
        patch("mindiebenchmark.common.datasetloaders.tokenid_loader.open").start()
        patch("mindiebenchmark.common.datasetloaders.tokenid_loader.csv.reader", return_value=tokenid_data).start()

        test_commandline_config = test_config.get_config()
        with patch('mindiebenchmark.common.config.argparse.ArgumentParser.parse_args',
                   return_value=argparse.Namespace(**test_commandline_config)):
            with patch.object(Logger, 'logger', autospec=True) as mock_logger:
                # Call the code that should not trigger logger.error
                self.assertIsNone(main())
                # Assert that error was not called
                mock_logger.error.assert_not_called()

    def test_run_vllm(self) -> None:
        test_config = self.base_config.update("TestType", 'vllm_client')
        test_commandline_config = test_config.get_config()

        with patch('mindiebenchmark.common.config.argparse.ArgumentParser.parse_args',
                   return_value=argparse.Namespace(**test_commandline_config)):
            self.assertIsNone(main())

    def tearDown(self) -> None:
        patch.stopall()


if __name__ == "__main__":
    unittest.main()
