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
from queue import Queue
from unittest.mock import patch

from mindiebenchmark.common.results import ResultData
from mindiebenchmark.common.datasetloaders import DatasetLoaderFactory
from mindiebenchmark.common.datasetloaders.boolq_loader import BoolqLoader
from mindiebenchmark.common.datasetloaders.ceval_loader import CEvalLoader
from mindiebenchmark.common.datasetloaders.cocotest_loader import CocotestLoader
from mindiebenchmark.common.datasetloaders.gsm8k_loader import Gsm8kLoader
from mindiebenchmark.common.datasetloaders.humaneval_loader import HumanevalLoader
from mindiebenchmark.common.datasetloaders.mmlu_loader import MmluLoader
from mindiebenchmark.common.datasetloaders.mtbench_loader import MtbenchLoader
from mindiebenchmark.common.datasetloaders.oasst1_loader import Oasst1Loader
from mindiebenchmark.common.datasetloaders.synthetic_loader import SyntheticLoader
from mindiebenchmark.common.datasetloaders.textvqa_loader import TextVQALoader
from mindiebenchmark.common.datasetloaders.tokenid_loader import TokenidLoader
from mindiebenchmark.common.datasetloaders.truthfulqa_loader import TruthfulqaLoader
from mindiebenchmark.common.datasetloaders.videobench_loader import VideoBenchLoader
from mindiebenchmark.common.datasetloaders.vocalsound_loader import VocalsoundLoader


LOADER_MOD_PATH = "mindiebenchmark.common.dataset_loader.DatasetLoader."


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

    def test_make_tokenid_loader(self):
        # 测试特定数据集加载器的情况
        self.command_line_args['Tokenizer'] = False
        self.command_line_args['DatasetType'] = 'boolq'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, TokenidLoader)

    def test_make_specific_loader(self):
        # 测试特定数据集加载器的情况
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'boolq'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, BoolqLoader)

        self.command_line_args['DatasetType'] = 'gsm8k'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, Gsm8kLoader)

    def test_make_dataset_loader_instance_boolq(self):
        # 测试特定数据集加载器的情况
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'boolq'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, BoolqLoader)

    def test_make_dataset_loader_instance_ceval(self):
        # 测试 ceval 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'ceval'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, CEvalLoader)

    def test_make_dataset_loader_instance_gsm8k(self):
        # 测试 gsm8k 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'gsm8k'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, Gsm8kLoader)

    def test_make_dataset_loader_instance_oasst1(self):
        # 测试 oasst1 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'oasst1'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, Oasst1Loader)

    def test_make_dataset_loader_instance_humaneval(self):
        # 测试 humaneval 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'humaneval'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, HumanevalLoader)

    def test_make_dataset_loader_instance_mmlu(self):
        # 测试 mmlu 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'mmlu'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, MmluLoader)

    def test_make_dataset_loader_instance_truthfulqa(self):
        # 测试 truthfulqa 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'truthfulqa'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, TruthfulqaLoader)

    def test_make_dataset_loader_instance_mtbench(self):
        # 测试 mtbench 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'mtbench'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, MtbenchLoader)

    def test_make_dataset_loader_instance_cocotest(self):
        # 测试 cocotest 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'cocotest'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, CocotestLoader)

    def test_make_dataset_loader_instance_synthetic(self):
        # 测试 synthetic 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'synthetic'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, SyntheticLoader)

    def test_make_dataset_loader_instance_textvqa(self):
        # 测试 textvqa 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'textvqa'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, TextVQALoader)

    def test_make_dataset_loader_instance_videobench(self):
        # 测试 videobench 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'videobench'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, VideoBenchLoader)

    def test_make_dataset_loader_instance_vocalsound(self):
        # 测试 vocalsound 数据集加载器
        self.command_line_args['Tokenizer'] = True
        self.command_line_args['DatasetType'] = 'vocalsound'
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.command_line_args, self.queue, self.result)
        self.assertIsInstance(loader, VocalsoundLoader)

    def tearDown(self) -> None:
        patch.stopall()


if __name__ == "__main__":
    unittest.main()
