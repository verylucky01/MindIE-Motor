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
import urllib3

from mindie_client.unit_utils import create_client
from mindieclient.python.common.logging import Log
from mindieclient.python.httpclient.utils import MindIEClientException


TEST_MODULE_PATH = 'mindieclient.python.httpclient.clients.base_client'

class TestMindIEClient(unittest.TestCase):

    def setUp(self):
        # Setup common mocks
        patcher_request = patch(TEST_MODULE_PATH + '.urllib3.PoolManager.request')
        self.mock_request = patcher_request.start()
        self.addCleanup(patcher_request.stop)
        self.request_count = 2
        patch("mindieclient.python.common.config.Config._check_file_permission",
            return_value=(True, None)).start()
        self.logger = Log(__name__).getlog()
        try:
            self.client = create_client(self.request_count)
        except Exception as e:
            self.logger.exception("Client Creation failed!")

    def tearDown(self):
        patch.stopall()

    def test_status_metadata_ok(self):
        # set up
        self._set_response_ok()
        model_name = 'test_model_name'

        res = self.client.is_server_live()
        self.assertIsInstance(
            res, bool, msg="is_server_live result is not instance of bool"
        )
        res = self.client.is_server_ready()
        self.assertIsInstance(
            res, bool, msg="is_server_ready result is not instance of bool"
        )
        res = self.client.is_model_ready(model_name)
        self.assertIsInstance(
            res, bool, msg="is_model_ready result is not instance of bool"
        )
        res = self.client.get_server_metadata()
        self.assertIsInstance(
            res, dict, msg="get_server_metadata result is not instance of dict"
        )
        res = self.client.get_model_metadata(model_name)
        self.assertIsInstance(
            res, dict, msg="get_model_metadata result is not instance of dict"
        )
        res = self.client.get_model_config(model_name)
        self.assertIsInstance(
            res, dict, msg="get_model_config result is not instance of dict"
        )
        res = self.client.get_slot_count(model_name).get_response()
        self.assertIsInstance(
            res, dict, msg="get_slot_count result is not instance of dict"
        )

    def test_generate_stream_ok(self):
        # set up mock objects
        self._set_response_ok()
        # create input
        model_name = 'mock_model'
        prompt = "My name is Olivier and I"

        results = self.client.generate_stream(
            model_name,
            prompt,
            request_id="1",
        )

        # assert
        generated_text = list(results)
        generated_text = "".join(
            [token for token in generated_text if token != "</s>"])

        expect_stream_output = "streamunittest"
        self.assertEqual(generated_text, expect_stream_output)

    def test_generate_stream_when_multi_data_in_oneline(self):
        # set up mock objects
        self._set_response_ok_when_multi_data_in_oneline()
        # create input
        model_name = 'mock_model'
        prompt = "My name is Olivier and I"

        results = self.client.generate_stream(
            model_name,
            prompt,
            request_id="1",
        )

        # assert
        generated_text_list = list(results)
        generated_text = "".join(
            [token for token in generated_text_list if token != "</s>"])

        expect_stream_output = "multidata"
        self.assertEqual(generated_text, expect_stream_output)

    def test_generate_stream_server_timeout(self):
        # set up mock objects
        self._set_response_server_timeout()

        # create input
        model_name = 'mock_model'
        prompt = "My name is Olivier and I"
        expected_error_msg = 'Engine time out'

        with self.assertRaises(MindIEClientException) as context:
            results = self.client.generate_stream(
                model_name,
                prompt,
                request_id="1",
            )
            generated_text = list(results)
            self.assertIsNone(generated_text)
        # assert
        self.assertIn(expected_error_msg, context.exception.get_message())

    def _set_response_ok(self):
        # add basic return value
        mock_response = MagicMock()
        mock_response.status_code = 200
        mock_response.status = 200
        mock_response.text = '{"total_slots":200,"free_slots":200,"available_tokens_length":65536}'
        json_content = '{"id": "1", "outputs":  \
            [{"name": "OUTPUT0", "shape": [1, 3], "datatype": "UINT32", "data": [1, 2, 3]}]}'
        mock_response.data = json_content.encode("utf-8")

        # add stream return value
        stream_data = [
            b'data:{"id":"1","model_name":"llama_65b","model_version":null,'
            b'"text_output":"stream","details":{"generated_tokens":1,'
            b'"first_token_cost":1,"decode_cost":1,"batch_size":1,"queue_wait_time":1},'
            b'"prefill_time":null,"decode_time":1}',

            b"\n",

            b'data:{"id":"2","model_name":"llama_65b","model_version":null,'
            b'"text_output":"unit","details":{"generated_tokens":1,'
            b'"first_token_cost":1,"decode_cost":1,"batch_size":1,"queue_wait_time":1},'
            b'"prefill_time":null,"decode_time":1}',

            b"\n",

            b'data:{"id":"3","model_name":"llama_65b","model_version":null,'
            b'"text_output":"test","details":{"generated_tokens":1,'
            b'"first_token_cost":1,"decode_cost":1,"batch_size":1,"queue_wait_time":1},'
            b'"prefill_time":null,"decode_time":1}',

            b"\n",
        ]
        mock_response.stream.return_value = stream_data

        self.mock_request.return_value = mock_response

    def _set_response_ok_when_multi_data_in_oneline(self):
        # add basic return value
        mock_response = MagicMock()
        mock_response.status_code = 200
        mock_response.status = 200
        mock_response.text = '{"total_slots":200,"free_slots":200,"available_tokens_length":65536}'
        json_content = '{"id": "1", "outputs":  \
            [{"name": "OUTPUT0", "shape": [1, 3], "datatype": "UINT32", "data": [1, 2, 3]}]}'
        mock_response.data = json_content.encode("utf-8")

        # add stream return value
        stream_data = [
            b'data:{"id":"1","model_name":"llama_65b","model_version":null,'
            b'"text_output":"multi","details":{"generated_tokens":1,'
            b'"first_token_cost":1,"decode_cost":1,"batch_size":1,"queue_wait_time":1},'
            b'"prefill_time":null,"decode_time":1}',

            b'data:{"id":"2","model_name":"llama_65b","model_version":null,'
            b'"text_output":"data","details":{"generated_tokens":1,'
            b'"first_token_cost":1,"decode_cost":1,"batch_size":1,"queue_wait_time":1},'
            b'"prefill_time":null,"decode_time":1}',
        ]
        mock_response.stream.return_value = stream_data

        self.mock_request.return_value = mock_response

    def _set_response_url_timeout(self):
        self.mock_request.side_effect = ValueError('wtf')

    def _set_response_server_timeout(self):
        # add basic return value
        mock_response = MagicMock()
        mock_response.status_code = 200
        mock_response.status = 200
        mock_response.text = '{"total_slots":200,"free_slots":200,"available_tokens_length":65536}'
        json_content = '{"id": "1", "outputs":  \
            [{"name": "OUTPUT0", "shape": [1, 3], "datatype": "UINT32", "data": [1, 2, 3]}]}'
        mock_response.data = json_content.encode("utf-8")

        # add stream return value
        stream_data = [
            b"engine callback timeout.",
            b"\n",
            b"engine callback timeout.",
            b"\n",
        ]
        mock_response.stream.return_value = stream_data

        self.mock_request.return_value = mock_response