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
import queue
from unittest.mock import patch
from mock_object import MockBenchmarkTokenizer
from benchmark.module import (
    MOCK_HOME, Config, ResultData, DatasetLoaderFactory,
    ClientFactory, BenchmarkServerException, server_mocker
    )


REQ_DATA = {"id": "001", "data": "hello world", "options": []}
CLI_MOD_PATH = "mindieclient.python.httpclient.client."


class TestBaseAPI(unittest.TestCase):
    @classmethod
    def init_config(cls):
        pass

    @classmethod
    def get_data_queue(cls):
        data_queue = queue.Queue()
        if cls.config.command_line_args['TaskKind'] == 'stream_token':
            data_queue.put({"id": "001", "data": [0], "options": []})
        else:
            data_queue.put(REQ_DATA.copy())
        data_queue.put(None)
        return data_queue

    @classmethod
    def setUpClass(cls) -> None:
        patch("mindieclient.python.common.config.Config._check_file_permission",
            return_value=(True, None)).start()
        cls.tokenize_mocker = patch(
            'mindiebenchmark.common.tokenizer.BenchmarkTokenizer',
            return_value=MockBenchmarkTokenizer
        )
        cls.tokenize_mocker.start()
        cls.mock_server_request = patch('urllib3.PoolManager.request', side_effect=server_mocker)
        cls.mock_server_request.start()
        os.environ["MIES_INSTALL_PATH"] = MOCK_HOME
        cls.current_dir = os.path.dirname(os.path.abspath(__file__))
        cls.benchmark_config_path = os.path.join(cls.current_dir, 'config', 'benchmark_config.json')
        cls.benchmark_output_path = os.path.join(cls.current_dir, 'save_token.csv')
        os.environ['MIES_INSTALL_PATH'] = os.path.join(cls.current_dir, 'mockers')
        os.environ['MINDIE_LLM_HOME_PATH'] = os.path.join(cls.current_dir)
        cls.config = Config()
        cls.config.command_line_args = {
            'TestType': 'client',
            'TaskKind': 'stream',
            'ModelName': "Llama2",
            'DatasetType':  "ceval",
            'Http': "http://127.0.0.1:1025",
            'MaxInputLen': 1024,
            'MaxOutputLen': 1024,
            "MAX_LINK_NUM": 1,
            "WarmupSize": 10,
            'ManagementHttp': "http://127.0.0.2:1026",
            "DoSampling": True,
            "SamplingParams": {
                "repetition_penalty": 1.0
            },
            "RequestRate": [1.0],
            "Distribution": "uniform",
            "Tokenizer": True,
            "SavePath": "./instance",
        }
        cls.config.config = {
            "MAX_LINK_NUM": 1,
            'ENABLE_MANAGEMENT': False
        }
        cls.init_config()
        cls.result = ResultData(cls.config.command_line_args)
        cls.loader = DatasetLoaderFactory.make_dataset_loader_instance(cls.config.command_line_args,
                                                                       cls.get_data_queue(), cls.result)
        cls.api = ClientFactory.make_client_instance(
            cls.config, result_container=cls.result, gevent_pool_size=1, dataloader=cls.loader
        )

    @classmethod
    def tearDownClass(cls) -> None:
        cls.tokenize_mocker.stop()
        cls.mock_server_request.stop()
        del cls.api

    def test_graceful_exit(self):
        patch("os.kill").start()
        self.assertIsNone(self.api.graceful_exit())
        patch("os.kill").stop()

    def test_prepare_input_exception(self):
        data = {}
        self.assertRaises(RuntimeError, self.api.prepare_input, data)

    def test_run_sync_task(self):
        os.makedirs("./instance", mode=0o640, exist_ok=True)
        self.assertIsNone(self.api.run_sync_task(self.get_data_queue()))
        self.assertEqual(self.api.results.success_req, self.api.results.total_req)


class TestSelfTritonTextClient(TestBaseAPI):
    @classmethod
    def init_config(cls):
        cls.config.command_line_args['TaskKind'] = 'text'


class TestSelfTritonStreamClient(TestBaseAPI):
    @classmethod
    def init_config(cls):
        cls.config.command_line_args['TaskKind'] = 'stream'


class TestSelfOpenaiTextClient(TestBaseAPI):
    @classmethod
    def init_config(cls):
        cls.config.command_line_args['TestType'] = 'openai'
        cls.config.command_line_args['TaskKind'] = 'text'


class TestSelfOpenaiStreamClient(TestBaseAPI):
    @classmethod
    def init_config(cls):
        cls.config.command_line_args['TestType'] = 'openai'
        cls.config.command_line_args['TaskKind'] = 'stream'


class TestSelfTokenClient(TestBaseAPI):
    @classmethod
    def init_config(cls):
        cls.config.command_line_args['TaskKind'] = 'stream_token'
        cls.config.command_line_args['Tokenizer'] = False


if __name__ == "__main__":
    unittest.main()