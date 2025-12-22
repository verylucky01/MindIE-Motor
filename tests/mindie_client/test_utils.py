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

import sys
import unittest
from unittest.mock import patch, MagicMock
from pathlib import Path
import pytest

import numpy as np
import rapidjson as json
from mindieclient.python.httpclient.utils import (
    _check_ca_cert,
    _check_directory_permissions,
    _check_invalid_ssl_filesize,
    _check_invalid_url,
    PathCheck,
    validate_model_string,
    check_response,
    MindIEClientException,
    get_infer_request_uri,
    generate_request_body,
    generate_request_body_token,
)
from mindieclient.python.httpclient.utils import check_password

current_dir = Path(__file__).parent.resolve()
target_dir = (current_dir / "../../utils/cert/scripts").resolve()
sys.path.append(target_dir)


def raise_error(message):
    raise ValueError(message)


class TestCheckPassword(unittest.TestCase):

    def setUp(self):
        patch("mindieclient.python.common.config.Config._check_file_permission",
              return_value=(True, None)).start()

    def tearDown(self):
        patch.stopall()

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_short_password(self, mock_raise_error):
        """Test password that is too short."""
        check_password("Ab1!")
        mock_raise_error.assert_called_once_with("The length of password must be >= 8.")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_only_uppercase(self, mock_raise_error):
        """Test password with only uppercase letters."""
        check_password("ABCDEFGHI")
        mock_raise_error.assert_called_once_with("Password check failed.")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_only_lowercase(self, mock_raise_error):
        """Test password with only lowercase letters."""
        check_password("abcdefgh")
        mock_raise_error.assert_called_once_with("Password check failed.")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_only_digits(self, mock_raise_error):
        """Test password with only digits."""
        check_password("12345678")
        mock_raise_error.assert_called_once_with("Password check failed.")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_only_special_characters(self, mock_raise_error):
        """Test password with only special characters."""
        check_password("!@#$%^&*")
        mock_raise_error.assert_called_once_with("Password check failed.")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_valid_password_mixed_types(self, mock_raise_error):
        """Test password with at least two types of characters."""
        check_password("Abcdefg1")
        mock_raise_error.assert_not_called()

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_invalid_character(self, mock_raise_error):
        """Test password with invalid characters."""
        check_password("Abcdefg1\x00")
        mock_raise_error.assert_called_once_with("Password check failed.")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_valid_password_with_special_chars(self, mock_raise_error):
        """Test password with valid length and special characters."""
        check_password("Abcd@123")
        mock_raise_error.assert_not_called()


class TestCheckInvalidURL(unittest.TestCase):

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_invalid_url_type(self, mock_raise_error):
        with self.assertRaises(AttributeError):
            url = 12345  # Invalid URL type
            enable_ssl = True
            _check_invalid_url(url, enable_ssl)
            mock_raise_error.assert_called_with("[MIE02E000002] URL version should be a string!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_invalid_url_scheme_ssl_enabled(self, mock_raise_error):
        url = "http://example.com"
        enable_ssl = True
        _check_invalid_url(url, enable_ssl)
        mock_raise_error.assert_called_with("[MIE02E000002] The scheme should be https when ssl enables!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_invalid_url_scheme_ssl_disabled(self, mock_raise_error):
        url = "https://example.com"
        enable_ssl = False
        _check_invalid_url(url, enable_ssl)
        mock_raise_error.assert_called_with("[MIE02E000002] The scheme should be http when ssl disables!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_missing_netloc(self, mock_raise_error):
        url = "https:///path/to/resource"
        enable_ssl = True
        _check_invalid_url(url, enable_ssl)
        mock_raise_error.assert_called_with("[MIE02E000002] URL should contain netloc!")


class TestCheckDirectoryPermissions(unittest.TestCase):

    @patch('os.stat')
    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_directory_permission_ok(self, mock_raise_error, mock_stat):
        mock_stat.return_value.st_mode = 0o700  # Correct permissions
        _check_directory_permissions("/valid/path")
        mock_raise_error.assert_not_called()

    @patch('os.stat')
    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_directory_permission_error(self, mock_raise_error, mock_stat):
        mock_stat.return_value.st_mode = 0o755  # Incorrect permissions
        _check_directory_permissions("/invalid/path")
        mock_raise_error.assert_called_with("The permission of ssl directory should be 700")


class TestCheckInvalidSSLFileSize(unittest.TestCase):

    @patch('os.path.getsize')
    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_ssl_file_size_ok(self, mock_raise_error, mock_getsize):
        mock_getsize.return_value = 5 * 1024 * 1024  # 5MB, within the limit
        ssl_options = {"ca_certs": "/path/to/ca_certs", "certfile": "/path/to/certfile", "keyfile": "/path/to/keyfile"}
        _check_invalid_ssl_filesize(ssl_options)
        mock_raise_error.assert_not_called()

    @patch('os.path.getsize')
    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_ssl_file_size_too_large(self, mock_raise_error, mock_getsize):
        mock_getsize.return_value = 11 * 1024 * 1024  # 11MB, exceeds the limit
        ssl_options = {"ca_certs": "/path/to/ca_certs", "certfile": "/path/to/certfile", "keyfile": "/path/to/keyfile"}
        _check_invalid_ssl_filesize(ssl_options)
        mock_raise_error.assert_called_with("SSL file should not exceed 10MB!")


class TestPathCheck(unittest.TestCase):

    def setUp(self):
        patch.stopall()

    @patch("os.path.exists")
    def test_check_path_exists(self, mock_exists):
        mock_exists.return_value = True
        ret, msg = PathCheck.check_path_exists("/valid/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")

        mock_exists.return_value = False
        ret, msg = PathCheck.check_path_exists("/invalid/path")
        self.assertFalse(ret)
        self.assertEqual(msg, "The path is not exists.")

    @patch("os.path.islink")
    @patch("os.path.normpath")
    def test_check_path_link(self, mock_normpath, mock_islink):
        mock_normpath.return_value = "/valid/path"
        mock_islink.return_value = False
        ret, msg = PathCheck.check_path_link("/valid/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")

        mock_islink.return_value = True
        ret, msg = PathCheck.check_path_link("/link/path")
        self.assertFalse(ret)
        self.assertEqual(msg, "The path is a soft link.")

    def test_check_path_valid(self):
        # Test empty path
        ret, msg = PathCheck.check_path_valid("")
        self.assertFalse(ret)
        self.assertEqual(msg, "The path is empty.")

        # Test path length exceeding limit
        ret, msg = PathCheck.check_path_valid("a" * 1025)
        self.assertFalse(ret)
        self.assertEqual(msg, "The length of path exceeds 1024 characters.")

        # Test invalid characters in path
        ret, msg = PathCheck.check_path_valid("invalid|path")
        self.assertFalse(ret)
        self.assertEqual(msg, "The path contains special characters.")

        # Test valid path
        ret, msg = PathCheck.check_path_valid("valid/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")

    @patch("os.getuid")
    @patch("os.getgid")
    @patch("os.stat")
    def test_check_path_owner_group_valid(self, mock_stat, mock_getgid, mock_getuid):
        # Mocking os.getuid and os.getgid
        mock_getuid.return_value = 1000
        mock_getgid.return_value = 1000

        # Mocking os.stat
        mock_stat.return_value.st_uid = 1000
        mock_stat.return_value.st_gid = 1000

        ret, msg = PathCheck.check_path_owner_group_valid("/valid/path")
        self.assertTrue(ret)
        self.assertEqual(msg, "")

        # Simulating mismatch in user and group
        mock_stat.return_value.st_uid = 2000
        mock_stat.return_value.st_gid = 2000
        ret, msg = PathCheck.check_path_owner_group_valid("/invalid/path")
        self.assertFalse(ret)
        self.assertIn("Check the path owner and group failed.", msg)

    def test_check_path_full(self):
        with patch("os.path.exists") as mock_exists, \
            patch("os.path.islink") as mock_islink, \
            patch("os.path.normpath") as mock_normpath, \
            patch("os.getuid") as mock_getuid, \
            patch("os.getgid") as mock_getgid, \
            patch("os.stat") as mock_stat:
            # Set up the mocks
            valid_path = "/valid/path"
            mock_exists.return_value = True
            mock_islink.return_value = False
            mock_normpath.return_value = valid_path
            mock_getuid.return_value = 1000
            mock_getgid.return_value = 1000
            mock_stat.return_value.st_uid = 1000
            mock_stat.return_value.st_gid = 1000

            ret, msg = PathCheck.check_path_full(valid_path)
            self.assertTrue(ret)
            self.assertEqual(msg, "")

    def tearDown(self) -> None:
        patch.stopall()


class TestValidateModelString(unittest.TestCase):

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_valid_model_name_and_version(self, mock_raise_error):
        try:
            validate_model_string("valid_model_1", "1.0")
        except ValueError:
            self.fail("validate_model_string raised ValueError unexpectedly!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_model_name_length_exceeds_limit(self, mock_raise_error):
        long_name = "a" * 1001
        validate_model_string(long_name)
        mock_raise_error.assert_called_with("The length of model name should be no more than 1000!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_model_version_length_exceeds_limit(self, mock_raise_error):
        long_version = "v" * 1001
        validate_model_string("valid_model", long_version)
        mock_raise_error.assert_called_with("The length of model version should be no more than 1000!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_invalid_characters_in_model_name(self, mock_raise_error):
        validate_model_string("invalid model!")
        mock_raise_error.assert_called_with("Model name should be a string and valid!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_invalid_characters_in_model_version(self, mock_raise_error):
        validate_model_string("valid_model", "version!")
        mock_raise_error.assert_called_with("Model version should be a string and valid!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_invalid_model_version(self, mock_raise_error):
        validate_model_string("valid_model", "invalid version!")
        mock_raise_error.assert_called_with("Model version should be a string and valid!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_empty_model_name(self, mock_raise_error):
        validate_model_string("")
        mock_raise_error.assert_called_with("Model name should be a string and valid!")

    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_empty_model_version(self, mock_raise_error):
        try:
            validate_model_string("valid_model", "")
        except ValueError:
            self.fail("validate_model_string raised ValueError unexpectedly!")


class TestCheckResponse(unittest.TestCase):

    @patch('mindieclient.python.httpclient.utils.Log')  # Mock Log class
    def test_status_200(self, mock_log):
        response = MagicMock()
        response.status = 200
        response.data = b'{"data": "value"}'  # Valid JSON body
        try:
            check_response(response)
        except MindIEClientException:
            self.fail("check_response raised MindIEClientException unexpectedly!")

    @patch('mindieclient.python.httpclient.utils.Log')  # Mock Log class
    def test_status_non_200(self, mock_log):
        response = MagicMock()
        response.status = 404
        response.data = b'{"error": "Not Found"}'

        with self.assertRaises(MindIEClientException) as context:
            check_response(response)

            mock_log().error.assert_called_with("[MIE02W00006] Get response failed : %s", "Not Found")

    @patch('mindieclient.python.httpclient.utils.Log')  # Mock Log class
    def test_status_non_200_without_error_field(self, mock_log):
        response = MagicMock()
        response.status = 500
        response.data = b'{}'  # JSON without "error" field

        with self.assertRaises(MindIEClientException) as context:
            check_response(response)
            self.assertEqual(str(context.exception), "Error status code: 500")
            mock_log().error.assert_called_with("[MIE02W00006] Get response failed : %s", "Error status code: 500")

    @patch('mindieclient.python.httpclient.utils.Log')  # Mock Log class
    def test_invalid_json_response(self, mock_log):
        response = MagicMock()
        response.status = 400
        response.data = b'Invalid JSON'

        with self.assertRaises(MindIEClientException) as context:
            check_response(response)
            self.test_logger.info(str(context.exception))
            self.assertTrue(
                str(context.exception).startswith("An exception occurred in client while decoding the response"))

    @patch('mindieclient.python.httpclient.utils.Log')  # Mock Log class
    def test_json_without_error_field(self, mock_log):
        response = MagicMock()
        response.status = 400
        response.data = b'{"message": "Bad Request"}'  # JSON without "error" field

        with self.assertRaises(MindIEClientException) as context:
            check_response(response)
            self.assertEqual(str(context.exception), "Error status code: 400")
            mock_log().error.assert_called_with("[MIE02W00006] Get response failed : %s", "Error status code: 400")


class TestGetInferRequestUri(unittest.TestCase):

    @patch('mindieclient.python.httpclient.utils.raise_error')  # Mock raise_error function
    def test_valid_model_version(self, mock_raise_error):
        model_name = "test_model"
        model_version = "v1.0"

        result = get_infer_request_uri(model_name, model_version)
        expected_uri = "/v2/models/test_model/versions/v1.0/infer"

        self.assertEqual(result, expected_uri)
        mock_raise_error.assert_not_called()  # Ensure raise_error was not called

    @patch('mindieclient.python.httpclient.utils.raise_error')  # Mock raise_error function
    def test_empty_model_version(self, mock_raise_error):
        model_name = "test_model"
        model_version = ""

        result = get_infer_request_uri(model_name, model_version)
        expected_uri = "/v2/models/test_model/infer"

        self.assertEqual(result, expected_uri)
        mock_raise_error.assert_not_called()  # Ensure raise_error was not called

    @patch('mindieclient.python.httpclient.utils.raise_error')  # Mock raise_error function
    def test_invalid_model_version_type(self, mock_raise_error):
        model_name = "test_model"
        model_version = 1.0  # Invalid type (not a string)

        # Ensure raise_error is called with the correct message
        mock_raise_error.reset_mock()  # Reset previous calls to mock

        get_infer_request_uri(model_name, model_version)
        mock_raise_error.assert_called_once_with("Model version should be a string!")

    @patch('mindieclient.python.httpclient.utils.raise_error')  # Mock raise_error function
    def test_model_name_with_special_characters(self, mock_raise_error):
        model_name = "model@name"
        model_version = "v1.0"

        result = get_infer_request_uri(model_name, model_version)
        expected_uri = "/v2/models/model%40name/versions/v1.0/infer"

        self.assertEqual(result, expected_uri)
        mock_raise_error.assert_not_called()


class TestGenerateRequestBodyToken(unittest.TestCase):

    def test_with_parameters(self):
        # 使用 NumPy 数组来模拟 token
        token = [np.array([123, 456, 789])]  # 使用 NumPy 数组，这样可以调用 tolist()
        is_stream = True
        parameters = {"param1": "value1", "param2": "value2"}  # 包含参数

        # 调用被测试函数
        request_body = generate_request_body_token(token, is_stream, parameters)

        # 解码并检查内容
        decoded_request_body = request_body.decode()
        expected_json = {
            "input_id": [123, 456, 789],
            "stream": True,
            "parameters": {"param1": "value1", "param2": "value2"}
        }

        # 验证生成的 JSON 是否匹配预期
        self.assertEqual(decoded_request_body, json.dumps(expected_json, ensure_ascii=False))

    def test_without_parameters(self):
        # 使用 NumPy 数组来模拟 token
        token = [np.array([123, 456, 789])]  # 使用 NumPy 数组，这样可以调用 tolist()
        is_stream = True
        parameters = {}  # 空参数字典

        # 调用被测试函数
        request_body = generate_request_body_token(token, is_stream, parameters)

        # 解码并检查内容
        decoded_request_body = request_body.decode()
        expected_json = {
            "input_id": [123, 456, 789],
            "stream": True
        }

        # 验证生成的 JSON 是否匹配预期
        self.assertEqual(decoded_request_body, json.dumps(expected_json, ensure_ascii=False))


class TestGenerateRequestBody(unittest.TestCase):

    @patch('mindieclient.python.httpclient.utils.json.dumps')  # Mock json.dumps to ensure it's called correctly
    def test_generate_request_body_with_generate(self, mock_json_dumps):
        # Prepare test data
        inputs = "some text input"
        request_id = "request123"
        parameters = {"param1": "value1"}
        outputs = None
        is_generate = True

        # Call the function
        result = generate_request_body(inputs, request_id, parameters, outputs, is_generate)

        # Assert json.dumps is called with the correct dictionary
        expected_dict = {
            "id": "request123",
            "text_input": "some text input",
            "parameters": {"param1": "value1"}
        }
        mock_json_dumps.assert_called_once_with(expected_dict)

        # Verify that the result is the expected byte-encoded string
        expected_body = json.dumps(expected_dict).encode()
        self.assertEqual(result, expected_body)

    @patch('mindieclient.python.httpclient.utils.json.dumps')  # Mock json.dumps to ensure it's called correctly
    def test_generate_request_body_without_generate(self, mock_json_dumps):
        # Prepare test data
        inputs = [MagicMock()]  # Mock input objects
        inputs[0].get_input_tensor.return_value = "tensor_input_1"  # Mock the method
        request_id = "request123"
        parameters = {"param1": "value1"}
        outputs = [MagicMock()]  # Mock output objects
        outputs[0].get_output_tensor.return_value = "tensor_output_1"  # Mock the method
        is_generate = False

        # Call the function
        result = generate_request_body(inputs, request_id, parameters, outputs, is_generate)

        # Assert json.dumps is called with the correct dictionary
        expected_dict = {
            "id": "request123",
            "inputs": ["tensor_input_1"],
            "outputs": ["tensor_output_1"],
            "parameters": {"param1": "value1"}
        }
        mock_json_dumps.assert_called_once_with(expected_dict)

        # Verify that the result is the expected byte-encoded string
        expected_body = json.dumps(expected_dict).encode()
        self.assertEqual(result, expected_body)

    @patch('mindieclient.python.httpclient.utils.json.dumps')  # Mock json.dumps to ensure it's called correctly
    def test_generate_request_body_with_empty_outputs(self, mock_json_dumps):
        # Prepare test data
        inputs = [MagicMock()]
        inputs[0].get_input_tensor.return_value = "tensor_input_1"
        request_id = "request123"
        parameters = {"param1": "value1"}
        outputs = []  # Empty outputs
        is_generate = False

        # Call the function
        result = generate_request_body(inputs, request_id, parameters, outputs, is_generate)

        # Assert json.dumps is called with the correct dictionary
        expected_dict = {
            "id": "request123",
            "inputs": ["tensor_input_1"],
            "outputs": [],
            "parameters": {"param1": "value1"}
        }
        mock_json_dumps.assert_called_once_with(expected_dict)

        # Verify that the result is the expected byte-encoded string
        expected_body = json.dumps(expected_dict).encode()
        self.assertEqual(result, expected_body)

    @patch('mindieclient.python.httpclient.utils.json.dumps')  # Mock json.dumps to ensure it's called correctly
    def test_generate_request_body_without_parameters(self, mock_json_dumps):
        # Prepare test data
        inputs = [MagicMock()]
        inputs[0].get_input_tensor.return_value = "tensor_input_1"
        request_id = "request123"
        parameters = {}  # Empty parameters
        outputs = [MagicMock()]
        outputs[0].get_output_tensor.return_value = "tensor_output_1"
        is_generate = False

        # Call the function
        result = generate_request_body(inputs, request_id, parameters, outputs, is_generate)

        # Assert json.dumps is called with the correct dictionary
        expected_dict = {
            "id": "request123",
            "inputs": ["tensor_input_1"],
            "outputs": ["tensor_output_1"]
        }
        mock_json_dumps.assert_called_once_with(expected_dict)

        # Verify that the result is the expected byte-encoded string
        expected_body = json.dumps(expected_dict).encode()
        self.assertEqual(result, expected_body)


class TestCheckCACert(unittest.TestCase):
    sys.path.append('utils/cert/scripts')
    
    @patch('os.getenv')  # Mock os.getenv
    @patch('os.path.exists')  # Mock os.path.exists
    @patch('utils.CertUtil')  # Mock CertUtil
    @patch('mindieclient.python.httpclient.utils.raise_error')  # Mock raise_error
    def test_check_ca_cert_success(self, mock_raise_error, mock_cert_util, mock_exists, mock_getenv):
        # Prepare test data
        ssl_options = {
            "ca_certs": "fake_ca_cert",
            "certfile": "fake_cert",
            "keyfile": "fake_key"
        }
        plain_text = "fake_plain_text"

        # Mock environment variables
        mock_getenv.return_value = "/fake/server/directory"

        # Mock path existence check
        mock_exists.return_value = True

        # Mock CertUtil methods
        mock_cert_util = mock_cert_util.return_value
        mock_cert_util.validate_ca_certs.return_value = True
        mock_cert_util.validate_ca_crl.return_value = True
        mock_cert_util.validate_cert_and_key.return_value = True

        _check_ca_cert(ssl_options, plain_text)

    @patch('os.getenv')
    @patch('os.path.exists')
    @patch('utils.CertUtil')  # Mock CertUtil
    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_check_ca_cert_cert_utils_not_found(self, mock_raise_error, mock_cert_util, mock_exists, mock_getenv):
        # Prepare test data
        ssl_options = {
            "ca_certs": "fake_ca_cert",
            "certfile": "fake_cert",
            "keyfile": "fake_key"
        }
        plain_text = "fake_plain_text"

        # Mock environment variables
        mock_getenv.return_value = "/fake/server/directory"

        # Simulate the cert utils path not existing
        mock_exists.return_value = False

        _check_ca_cert(ssl_options, plain_text)

    @patch('os.getenv')
    @patch('os.path.exists')
    @patch('utils.CertUtil')  # Mock CertUtil
    @patch('mindieclient.python.httpclient.utils.raise_error')
    def test_check_ca_cert_ca_check_failed(self, mock_raise_error, mock_cert_util, mock_exists, mock_getenv):
        # Prepare test data
        ssl_options = {
            "ca_certs": "fake_ca_cert",
            "certfile": "fake_cert",
            "keyfile": "fake_key"
        }
        plain_text = "fake_plain_text"

        # Mock environment variables
        mock_getenv.return_value = "/fake/server/directory"

        # Mock path existence check
        mock_exists.return_value = True

        # Mock CertUtil methods (simulate CA check failure)
        mock_cert_util = mock_cert_util.return_value
        mock_cert_util.validate_ca_certs.return_value = False

        _check_ca_cert(ssl_options, plain_text)


if __name__ == 'mindieclient.python.httpclient.utils':
    unittest.main()
