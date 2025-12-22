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

from queue import Queue
import unittest
from unittest.mock import MagicMock, patch, mock_open

import pandas as pd
from mock_path_check import PathCheckMocker

from mindiebenchmark.common.results import ResultData
from mindiebenchmark.common.datasetloaders import DatasetLoaderFactory


TEST_MOD_PATH = "mindiebenchmark.common.datasetloaders.ceval_loader."


class TestSynthicDatasetLoader(unittest.TestCase):
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
        self.command_line_args['DatasetType'] = 'synthetic'
        
        PathCheckMocker().mock_methods()
    
    @patch("builtins.open", mock_open(read_data='''{
            "Input": {
                "Method": "uniform",
                "Params": {"MinValue": 1, "MaxValue": 200}
            },
            "Output": {
                "Method": "gaussian",
                "Params": {"Mean": 100, "Var": 200, "MinValue": 1, "MaxValue": 100}
            },
            "RequestCount": 100
        }''')) 
    def test_read_file_json(self):
        # 模拟 pd.read_csv 返回的 CSV 数据
        
        # Simulate the file content for the read() method

        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)

        # 调用 read_file 方法
        loader.read_file("mock/synthetic_config.json")

    @patch(TEST_MOD_PATH + 'pd.read_csv')
    def test_read_file_csv(self, mock_read_csv: MagicMock):
        mock_data = {
            'input_len': [10, 20, 30],  # id 从0到3
            'output_len': [10, 20, 30]
        }
        mock_df = pd.DataFrame(mock_data)
        mock_read_csv.return_value = mock_df
        # 模拟 pd.read_csv 返回的 CSV 数据
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)

        # 调用 read_file 方法
        loader.read_file("mock/synthetic_config.csv")

    def tearDown(self) -> None:
        patch.stopall()
if __name__ == "__main__":
    unittest.main()
