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
from unittest.mock import MagicMock, patch
import numpy as np
from mindie_client.unit_utils import create_client
from mindieclient.python.httpclient import Input, RequestedOutput
from mindieclient.python.common.logging import Log


test_module_path = 'mindieclient.python.httpclient.client'


class TestClient(unittest.TestCase):

    @patch("urllib3.PoolManager")
    def mockclient(self, mocktool: MagicMock):
        mock_http_client = MagicMock()
        mock_response = MagicMock()
        mocktool.return_value = mock_http_client
        mock_http_client.request.return_value = mock_response
        mock_response.status = 200
        mock_response.status_code = 200

        json_content = '{"id": "1", "outputs":  \
        [{"name": "OUTPUT0", "shape": [1, 3], "datatype": "UINT32", "data": [1, 2, 3]}]}'
        mock_response.data = json_content.encode("utf-8")
        self.text = [
            b'data:{"id":"1","model_name":"llama_65b","model_version":null,"text_output":"stream","details":{'
            b'"generated_tokens":1,"first_token_cost":1,"decode_cost":1,"batch_size":1,"queue_wait_time":1},'
            b'"prefill_time":null,"decode_time":1}',
            b"\n",
            b'data:{"id":"2","model_name":"llama_65b","model_version":null,"text_output":"unit","details":{'
            b'"generated_tokens":1,"first_token_cost":1,"decode_cost":1,"batch_size":1,"queue_wait_time":1},'
            b'"prefill_time":null,"decode_time":1}',
            b"\n",
            b'data:{"id":"3","model_name":"llama_65b","model_version":null,"text_output":"test","details":{'
            b'"generated_tokens":1,"first_token_cost":1,"decode_cost":1,"batch_size":1,"queue_wait_time":1},'
            b'"prefill_time":null,"decode_time":1}',
            b"\n",
        ]
        mock_response.stream.return_value = self.text
        self.model_name = "llama_65b"
        self.request_count = 2
        try:
            self.mindie_client = create_client(self.request_count)
        except Exception as e:
            self.logger.exception("Client Creation failed!")

    def setUp(self):
        patch("mindieclient.python.common.config.Config._check_file_permission",
                    return_value=(True, None)).start()
        self.mockclient()
        self.logger = Log(__name__).getlog()

    def tearDown(self):
        patch.stopall()

    def test_async_infer(self):
        # create input and requested output
        inputs = []
        outputs = []
        input_data = np.arange(start=0, stop=16, dtype=np.uint32)
        input_data = np.expand_dims(input_data, axis=0)
        inputs.append(Input("INPUT0", [1, 16], "UINT32"))
        inputs[0].initialize_data(input_data)
        outputs.append(RequestedOutput("OUTPUT0"))

        # apply async inference
        async_requests = []
        for _ in range(self.request_count):
            async_requests.append(
                self.mindie_client.async_infer(
                    self.model_name,
                    inputs,
                    outputs=outputs,
                )
            )

        # get_result
        for async_request in async_requests:
            result = async_request.get_result()
            self.logger.info(result.get_response())

            output_data = result.retrieve_output_name_to_numpy("OUTPUT0")
            self.assertIsNotNone(output_data, msg="async_infer result is none")
            self.logger.info("output_data: %s", output_data)

    def test_infer(self):
        # create input and requested output
        inputs = []
        outputs = []
        input_data = np.arange(start=0, stop=16, dtype=np.uint32)
        input_data = np.expand_dims(input_data, axis=0)
        inputs.append(Input("INPUT0", [1, 16], "UINT32"))
        inputs[0].initialize_data(input_data)
        outputs.append(RequestedOutput("OUTPUT0"))

        # apply model inference
        results = self.mindie_client.infer(
            self.model_name,
            inputs,
            outputs=outputs,
        )

        output_data = results.retrieve_output_name_to_numpy("OUTPUT0")
        self.assertIsNotNone(output_data, msg="infer result is none")
        self.logger.info("output_data: %s", output_data)

    def test_generate(self):
        # create input
        prompt = "My name is Olivier and I"
        parameters = {
            "do_sample": True,
            "temperature": 0.5,
            "top_k": 10,
            "top_p": 0.9,
            "truncate": 5,
            "typical_p": 0.9,
            "seed": 1,
            "repetition_penalty": 1,
            "watermark": True,
            "details": True,
        }
        # apply model inference
        result = self.mindie_client.generate(
            self.model_name,
            prompt,
            request_id="1",
            parameters=parameters,
        )

        res = result.get_response()
        self.assertIsInstance(
            res, dict, msg="generate result is not instance of dict")

    @patch("requests.get", autospec=True)
    def test_status_metadata(self, mocktool: MagicMock):
        mock_response = MagicMock()
        mocktool.return_value = mock_response
        mock_response.status_code = 200
        mock_response.text = '{"total_slots":200,"free_slots":200,"available_tokens_length":65536}'
        res = self.mindie_client.is_server_live()
        self.assertIsInstance(
            res, bool, msg="is_server_live result is not instance of bool"
        )
        res = self.mindie_client.is_server_ready()
        self.assertIsInstance(
            res, bool, msg="is_server_ready result is not instance of bool"
        )
        res = self.mindie_client.is_model_ready(self.model_name)
        self.assertIsInstance(
            res, bool, msg="is_model_ready result is not instance of bool"
        )
        res = self.mindie_client.get_server_metadata()
        self.assertIsInstance(
            res, dict, msg="get_server_metadata result is not instance of dict"
        )
        res = self.mindie_client.get_model_metadata(self.model_name)
        self.assertIsInstance(
            res, dict, msg="get_model_metadata result is not instance of dict"
        )
        res = self.mindie_client.get_model_config(self.model_name)
        self.assertIsInstance(
            res, dict, msg="get_model_config result is not instance of dict"
        )
        res = self.mindie_client.get_slot_count(self.model_name).get_response()
        self.assertIsInstance(
            res, dict, msg="get_slot_count result is not instance of dict"
        )

    @patch("requests.post", autospec=True)
    def test_generate_stream_self(self, mocktool: MagicMock):
        mock_response = MagicMock()
        mocktool.return_value = mock_response
        mock_response.status_code = 200
        mock_response.iter_lines.return_value = self.text
        # create input
        prompt = "My name is Olivier and I"
        parameters = {
            "do_sample": True,
            "temperature": 0.5,
            "top_k": 10,
            "top_p": 0.9,
            "truncate": 5,
            "typical_p": 0.9,
            "seed": 1,
            "repetition_penalty": 1,
            "watermark": True,
            "details": True,
        }
        token = np.array([1, 2, 3])
        if token.ndim == 1:
            token = np.expand_dims(token, axis=0)
        self.assertIsNotNone(self.mindie_client.generate_stream_self(
            token,
            request_id="1",
            parameters=parameters,
        ))

    @patch("requests.post", autospec=True)
    def test_cancel(self, mocktool: MagicMock):
        mock_response = MagicMock()
        mocktool.return_value = mock_response
        mock_response.status_code = 200
        mock_response.iter_lines.return_value = self.text
        # create input
        prompt = "My name is Olivier and I"
        parameters = {
            "do_sample": True,
            "temperature": 0.5,
            "top_k": 10,
            "top_p": 0.9,
            "truncate": 5,
            "typical_p": 0.9,
            "seed": 1,
            "repetition_penalty": 1,
            "watermark": True,
            "details": True,
        }
        results = self.mindie_client.generate_stream(
            self.model_name,
            prompt,
            request_id="1",
            parameters=parameters,
        )
        generated_text = ""
        index = 0
        for cur_res in results:
            index += 1
            if index == 10:
                flag = self.mindie_client.cancel(self.model_name, "1")
                self.assertTrue(flag, "Test cancel api failed!")
            if cur_res == "</s>":
                break
            generated_text += cur_res
            self.logger.info("current result: %s", cur_res)

        self.logger.info("final_generated_text: %s", generated_text)

    @patch.object(Log, 'getlog')
    @patch('urllib3.PoolManager.request')
    def test_generate_stream_with_details(self,
                                          mocktool: MagicMock,
                                          mock_log: MagicMock
                                          ):
        mock_response = MagicMock()
        mocktool.return_value = mock_response
        mock_logger = MagicMock()
        mock_log.return_value = mock_logger
        # create input
        prompt = "My name is Olivier and I"
        parameters = {
            "details": True,
        }
        mock_response.stream.return_value = [
            b'data: {"text_output": "hello", "details": {"batch_size": 32, "queue_wait_time": 1.2}, "decode_time": '
            b'0.3, "prefill_time": 0.4}\n',
            b'\n'
        ]

        stream_generator = self.mindie_client.generate_stream_with_details(
            self.model_name,
            prompt,
            request_id="1",
            parameters=parameters,
        )

        result = next(stream_generator)

        generated_text = list(result)
        generated_text = "".join(
            [token for token in generated_text if token != "</s>"])

        expect_stream_output = ["stream", "unit", "test"]

        req_id = 0
        for idx, output in enumerate(self.mindie_client.generate_stream_with_details(
                self.model_name,
                prompt,
                request_id=str(req_id),
                parameters=parameters,
        )):
            self.assertEqual(output["text_output"], expect_stream_output[idx])
            req_id += 1


if __name__ == "__main__":
    unittest.main()
