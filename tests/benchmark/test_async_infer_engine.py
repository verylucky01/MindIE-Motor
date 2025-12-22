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
import copy
import unittest
import queue
import threading
from unittest.mock import patch
from mock_object import MockBenchmarkTokenizer
from mindiebenchmark.inference.async_inference_engine import InferenceEngine
from mindiebenchmark.inference.async_inference_engine import g_infer_result, engine_result_template
from benchmark.module import MOCK_HOME, Config, ResultData, DatasetLoaderFactory


class TestInferEngine(unittest.TestCase):
    def setUp(self) -> None:
        self.tokenize_mocker = patch(
            'mindiebenchmark.common.tokenizer.BenchmarkTokenizer',
            return_value=MockBenchmarkTokenizer
        )
        self.tokenize_mocker.start()

        self.register_signal_mocker = patch('mindiebenchmark.common.thread_utils.register_signal')
        self.register_signal_mocker.start()

        self.max_batch_size = 200
        self.get_max_batch_size_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.get_max_batch_size',
            return_value=self.max_batch_size
        )
        self.get_max_batch_size_mocker.start()

        self.get_engine_config_path_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.get_engine_config_path',
            return_value="config.json"
        )
        self.get_engine_config_path_mocker.start()

        os.environ["MIES_INSTALL_PATH"] = MOCK_HOME

        self.config = Config()
        self.config.command_line_args.update(
            {
                "TaskKind": "stream",
                "ModelName": "Llama2",
                "DatasetType": "gsm8k",
                "DatasetPath": "/data/path",
                "MaxInputLen": 1024,
                "MaxOutputLen": 512,
                "WarmupSize": 10,
                "DoSampling": False,
                "SamplingParams": {},
                "Tokenizer":True
            }
        )
        result = ResultData(self.config.command_line_args)
        q = queue.Queue()
        q.put({"id": "1", "data": [1, 2, 3]})
        loader = DatasetLoaderFactory.make_dataset_loader_instance(self.config.command_line_args, q, result)
        self.infer_engine = InferenceEngine(self.config, q, loader, result)

    def test_stop_infer(self):
        self.infer_engine.stop_infer()
        self.assertTrue(self.infer_engine.stop)

    def test_init_engine_failed(self):
        from llm_manager_python_api_demo.status import Code, Status
        with patch("llm_manager_python_api_demo.engine.Engine.init", return_value=Status(Code.ERROR, "error")):
            self.infer_engine.init_engine()
            self.assertTrue(self.infer_engine.loader.stop)

    def test_forward_one(self):
        from llm_manager_python_api_demo.status import Code, Status
        from llm_manager_python_api_demo.request import Request
        from llm_manager_python_api_demo.request_id import RequestId
        async_forward_mocker = patch(
            "llm_manager_python_api_demo.engine.Engine.async_forward",
            return_value=Status(Code.ERROR, "error")
        )
        async_forward_mocker.start()
        get_request_id_mocker = patch(
            "llm_manager_python_api_demo.request.Request.get_request_id",
            return_value=RequestId("req_id_0")
        )
        get_request_id_mocker.start()
        self.infer_engine.forward_one(Request("req_id_0"))
        self.assertEqual(self.infer_engine.failed_req, 1)

    def test_get_max_batch_size(self):
        max_batch_size = 200
        with patch("llm_manager_python_api_demo.engine.Engine.get_scheduler_config", return_value=max_batch_size):
            self.assertEqual(max_batch_size, self.infer_engine.get_max_batch_size())

    def test_prepare_input_success(self):
        from llm_manager_python_api_demo.data import Data
        with patch("llm_manager_python_api_demo.dtype.DType"):
            raw_data = {"id": "123", "data": "hello test"}
            result = self.infer_engine.prepare_input(raw_data)
            self.assertIsInstance(result, Data)

    def test_get_sampling_params(self):
        from llm_manager_python_api_demo.sampling import SamplingParams
        self.infer_engine.sampling_params = {}
        result = self.infer_engine.get_sampling_params()
        assert isinstance(result, SamplingParams)

    def test_get_quotas(self):
        result = self.infer_engine.get_quotas()
        self.assertEqual(result, ((1, 1, self.max_batch_size - 1)))

    def test_get_input_data_by_quotas(self):
        from llm_manager_python_api_demo.data import Data
        get_quotas_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.get_quotas',
            return_value=(3, 3, 30)
        )
        get_quotas_mocker.start()
        prepare_input_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.prepare_input',
            return_value=Data()
        )
        prepare_input_mocker.start()
        q = queue.Queue()
        q.put({"id": "1", "data": "a a a"})
        q.put({"id": "2", "data": "b b b"})
        self.infer_engine.data_queue = q
        result = self.infer_engine.get_input_data_by_quotas()
        self.assertEqual(len(result), 2)

    def test_update_metrics_request(self):
        data_id = "1"
        token_ids = [1, 2, 3]
        action = "request"
        g_infer_result.clear()
        with patch("time.time", return_value=100.0):
            self.infer_engine.update_metrics(data_id, token_ids, action)
        self.assertIn(data_id, g_infer_result)
        self.assertEqual(g_infer_result[data_id]["input_token_ids"], token_ids)
        self.assertEqual(g_infer_result[data_id]["start_time"], 100.0)

    def test_update_metrics_response(self):
        data_id = "2"
        token_ids = [4, 5, 6]
        action = "response"
        g_infer_result[data_id] = copy.deepcopy(engine_result_template)
        with patch("time.time", return_value=200.0):
            self.infer_engine.update_metrics(data_id, token_ids, action)
        self.assertIn(data_id, g_infer_result)
        self.assertEqual(g_infer_result[data_id]["generate_token_ids"], token_ids)
        self.assertEqual(g_infer_result[data_id]["response_time"], [200.0])

    def test_update_metrics_remove(self):
        data_id = "3"
        action = "remove"
        g_infer_result[data_id] = copy.deepcopy(engine_result_template)
        self.infer_engine.update_metrics(data_id, [], action)
        self.assertNotIn(data_id, g_infer_result)

    def test_async_forward(self):
        from llm_manager_python_api_demo.data import Data
        from llm_manager_python_api_demo.dtype import DType
        from llm_manager_python_api_demo.request import Request
        get_quotas_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.get_quotas',
            return_value=(3, 3, 30)
        )
        get_quotas_mocker.start()
        prepare_input_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.prepare_input',
            return_value=Data()
        )
        prepare_input_mocker.start()
        data_to_request_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.data_to_request',
            return_value=Request('0')
        )
        data_to_request_mocker.start()
        token_ids_to_data_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.token_ids_to_data',
            return_value=Data()
        )
        token_ids_to_data_mocker.start()
        is_response_timeout_mocker = patch(
            'mindiebenchmark.inference.async_inference_engine.InferenceEngine.is_response_timeout',
            return_value=True
        )
        is_response_timeout_mocker.start()

        q = queue.Queue()
        q.put({"id": "1", "data": "a a a"})
        q.put({"id": "2", "data": "b b b"})
        self.infer_engine.data_queue = q
        self.infer_engine.async_forward()
        self.assertEqual(self.infer_engine.results.total_req, 2)

    def test_process(self):
        self.infer_engine.last_response_time = 2.5
        self.infer_engine.first_request_time = 1.0
        g_infer_result.update({
            "data1": {
                "input_token_ids": [1, 2, 3],
                "generate_token_ids": [4, 5, 6],
                "start_time": 0.0,
                "response_time": [0.5, 1.0, 1.5]
            }
        })
        self.infer_engine.process()
        self.assertEqual(self.infer_engine.results.prefill_latencies, [0.5 * 1000.0])
        self.assertEqual(self.infer_engine.results.decode_token_latencies, [500.0, 500.0])

    def tearDown(self) -> None:
        patch.stopall()
        del self.infer_engine

if __name__ == "__main__":
    unittest.main()
