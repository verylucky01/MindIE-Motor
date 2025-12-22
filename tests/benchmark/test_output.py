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

import time
import unittest
from unittest.mock import patch
from benchmark.module import Config, ResultData, MetricsCalculator

TEST_MOD_PATH = "mindiebenchmark.common.output."


def mock_getenv(var, default=None):
    env_vars = {
        "MINDIE_LLM_BENCHMARK_FILEPATH": ".",
        "MINDIE_LLM_HOME_PATH": "."
    }
    return env_vars.get(var, default)


class TestMetricsCalculator(unittest.TestCase):
    def setUp(self):
        self.config = Config()
        self.config.command_line_args.update(
            {
                'TaskKind': "stream",
                'ModelName': "Llama2",
                'DatasetType':  "ceval",
                'Http': "http://127.0.0.1:1025",
                'MaxInputLen': 1024,
                'MaxOutputLen': 1024,
                "MAX_LINK_NUM": 1,
                "RequestRate": "",
                "WarmupSize": 10,
                'ManagementHttp': "http://127.0.0.2:1026",
            }
        )
        result = ResultData(self.config.command_line_args)
        self.output = MetricsCalculator(result)

    @patch('mindiebenchmark.common.file_utils.PathCheck.check_path_full')
    @patch(TEST_MOD_PATH + 'os.getenv', side_effect=mock_getenv)
    @patch(TEST_MOD_PATH + 'jsonlines.open')
    def test_calculate(self, mock_jsonlines_open, mock_os_getenv, mock_check_path):
        start_time = time.time()

        # Mock path check to return success
        mock_check_path.return_value = (False, None)

        self.assertIsNone(self.output.calculate(self.config, start_time))

    def test_save_to_csv(self):
        save_path = "./instance/result.csv"
        self.assertIsNone(self.output.save_to_csv(save_path))

    def test_display_metrics_as_table(self):
        self.assertIsNone(self.output.display_metrics_as_table())

    def test_display_common_metrics_as_table(self):
        self.assertIsNone(self.output.display_common_metrics_as_table())

    def tearDown(self):
        del self.config
        del self.output
        patch.stopall()

if __name__ == "__main__":
    unittest.main()