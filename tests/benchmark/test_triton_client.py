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
import logging
from unittest.mock import patch, MagicMock

from mock_object import MockBenchmarkTokenizer
from benchmark.module import (
    TokenizerModel, MOCK_HOME, Config, ResultData, DatasetLoaderFactory,
    TritonClient, ModelClient, InferRequest
)


REQ_DATA = {"id": "001", "data": [1, 2, 3], "options": []}
CLI_MOD_PATH = "mindiebenchmark.inference.triton_client."
logger = logging.getLogger(__file__)


class TestTritonClient(unittest.TestCase):
    def setUp(self) -> None:
        self.client_mocker = patch("geventhttpclient.HTTPClient.from_url")
        self.request_mocker = patch("urllib3.PoolManager.request")
        self.client_mocker.start()
        self.request_mocker.start()
        self.tokenize_mocker = patch(
            'mindiebenchmark.inference.triton_client.AutoTokenizer.from_pretrained',
            return_value=TokenizerModel()
        )
        self.tokenize_mocker.start()
        patch(
            'mindiebenchmark.common.tokenizer.BenchmarkTokenizer',
            return_value=MockBenchmarkTokenizer).start()

        os.environ["MIES_INSTALL_PATH"] = MOCK_HOME

        self.config = Config()
        self.config.command_line_args.update(
            {
                'TaskKind': "stream",
                'ModelName': "Llama2",
                'DatasetType': "ceval",
                'Http': "http://127.0.0.1:1025",
                'MaxInputLen': 1024,
                'MaxOutputLen': 1024,
                "MAX_LINK_NUM": 1,
                "RequestRate": "",
                "WarmupSize": 10,
                "DoSampling": False,
                "Concurrency": 1,
                'ManagementHttp': "http://127.0.0.2:1026",
            }
        )
        self.config.config = {"MAX_LINK_NUM": 1}

        result = ResultData(self.config.command_line_args)
        q = queue.Queue()
        q.put(REQ_DATA.copy())
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.config.command_line_args, q, result)

        self.modelname = self.config.command_line_args.get('ModelName')
        self.client = TritonClient(
            client_config=self.config, result_container=result,
            dataloader=loader
        )

    def test_prepare_input_raise(self):
        data = queue.Queue()
        data.put({})
        self.assertRaises(RuntimeError, self.client.prepare_input, data)

    def test_prepare_input(self):
        data = queue.Queue()
        data.put(REQ_DATA.copy())
        self.client.prepare_input(data)
        self.assertIsInstance(self.client.requests.get(), InferRequest)

    def test_get_sampling_params_true(self):
        self.config.command_line_args["DoSampling"] = True
        self.assertTrue(self.client.get_sampling_params()["do_sample"])

    def test_get_sampling_params_false(self):
        self.config.command_line_args["DoSampling"] = False
        self.assertEqual(len(self.client.get_sampling_params()), 0)

    def test_submit_requests(self):
        default_sampling_params = {
            'temperature': 1.0,
            'top_k': 0,
            'top_p': 1.0,
            'typical_p': 1.0,
            'do_sample': 0,
            'seed': 0,
            'repetition_penalty': 1.0,
            'frequency_penalty': 0,
            'presence_penalty': 0,
            'watermark': 0
        }
        self.client.requests.put(InferRequest(request_id="1230",
                                              question="Where are you from",
                                              tokenizer=TokenizerModel(),
                                              sampling_params=default_sampling_params,
                                              ))
        init = queue.Queue(1)
        self.client.submit_requests(client_id=0, init=init)
        self.assertEqual(self.client.results.success_req, 1)

    def test_run_triton_sync_task(self):
        data = queue.Queue()
        data.put(REQ_DATA.copy())
        self.assertIsNone(self.client.run_triton_sync_task(data))

    def tearDown(self) -> None:
        patch.stopall()
        del self.client


if __name__ == "__main__":
    unittest.main()
