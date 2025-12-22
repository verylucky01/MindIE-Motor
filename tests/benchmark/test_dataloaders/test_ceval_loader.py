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
import json
from queue import Queue
from unittest.mock import MagicMock, patch

import pandas as pd
from mock_path_check import PathCheckMocker
from mindiebenchmark.common.datasetloaders import DatasetLoaderFactory
from mindiebenchmark.common.results import ResultData


TEST_MOD_PATH = "mindiebenchmark.common.datasetloaders.ceval_loader."


class TestDatasetLoaderFactory(unittest.TestCase):
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
        self.command_line_args['DatasetType'] = 'ceval'

        PathCheckMocker().mock_methods()

    @patch(TEST_MOD_PATH + 'pd.read_csv')
    def test_read_file(self, mock_read_csv: MagicMock):
        # 模拟 pd.read_csv 返回的 CSV 数据
        mock_data = {
            'id': [i for i in range(3)],  # id 从0到3
            'question': [
                "Dummy Question 1",
                "Dummy Question 2",
                "Dummy Question 3",
            ],
            'A': [
                "Dummy Quetion 1 Option A", "Dummy Quetion 2 Option A", "Dummy Quetion 3 Option A"
            ],
            'B': [
                "Dummy Quetion 1 Option B", "Dummy Quetion 2 Option B", "Dummy Quetion 3 Option B"
            ],
            'C': [
                "Dummy Quetion 1 Option C", "Dummy Quetion 2 Option C", "Dummy Quetion 3 Option C",
            ],
            'D': [
                "Dummy Quetion 1 Option D", "Dummy Quetion 2 Option D", "Dummy Quetion 3 Option D",
            ],
            'answer': ['A', 'B', 'C']
        }
        mock_df = pd.DataFrame(mock_data)
        mock_read_csv.return_value = mock_df
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        loader.test_accuracy = True
        loader.dataset_path = "mock_dataset"
        loader.stop = False  # 确保不停止
        loader.gt_answer = {}

        # 调用 read_file 方法
        loader.read_file("mock_val_file.csv")

        # 验证读取的文件和生成的字典
        mock_read_csv.assert_called()
        self.assertIn("mock_val__0", loader.gt_answer)
        self.assertEqual(loader.gt_answer["mock_val__0"], "A")

        # 验证处理逻辑
        self.assertEqual(len(loader.gt_answer), 3)

    @patch("os.open")
    @patch("os.fdopen")
    def test_load_lora_adapter_success(self, mock_os_fdopen: MagicMock, mock_os_open: MagicMock):
        self.command_line_args["TestType"] = "vllm_client"
        self.command_line_args["LoraDataMappingFile"] = "./lora_data_mapping.json"
        mock_data = [
            {"0": "LoraAdapter1"},
            {"1": "LoraAdapter2"},
            {"2": "LoraAdapter3"}
        ]

        def json_iterator(data):
            for item in data:
                yield json.dumps(item)

        mock_os_open.return_value = 'mock_file'
        mock_file = MagicMock()
        mock_file.__enter__.return_value = json_iterator(mock_data)
        mock_os_fdopen.return_value = mock_file
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertEqual(loader.lora_adapter_dict, {'0': 'LoraAdapter1', '1': 'LoraAdapter2', '2': 'LoraAdapter3'})

    def test_load_lora_adapter_unspport_api(self):
        self.command_line_args["TestType"] = "client"
        self.command_line_args["LoraDataMappingFile"] = "./lora_data_mapping.json"
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertEqual(loader.lora_adapter_dict, {})

    def tearDown(self) -> None:
        patch.stopall()


if __name__ == "__main__":
    unittest.main()
