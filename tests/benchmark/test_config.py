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
from unittest.mock import patch, MagicMock
from parameterized import parameterized
from benchmark.module import Config, BENCHMARK_HOME

TEST_CLASS_PATH = "mindiebenchmark.common.config.Config."
TEST_MOD_PATH = "mindiebenchmark.common.config."


class TestConfig(unittest.TestCase):
    def setUp(self):
        from mindiebenchmark.common.file_utils import PathCheck
        patch.multiple(
            PathCheck,
            check_path_valid=MagicMock(return_value=(True, "")),
            check_path_link=MagicMock(return_value=(True, "")),
            check_path_owner_group_valid=MagicMock(return_value=(True, "")),
            check_path_mode=MagicMock(return_value=(True, "")),
            check_system_path=MagicMock(return_value=(True, "")),
            check_file_size=MagicMock(return_value=(True, ""))
        ).start()
        patch("mindieclient.python.common.config.Config._check_file_permission",
            return_value=(True, None)).start()
        self.config = Config()
        self.config.config.clear()
        self.config.command_line_args.clear()

    def test_validate_config(self):
        self.config.config.update({
            "INSTANCE_PATH": "./instance",
            "LOG_PATH": "~/mindie/log/debug",
            "LOG_TO_FILE": "true",
            "LOG_TO_STDOUT": "false",
            "LOG_LEVEL": "INFO",
            "LOG_VERBOSE": "false",
            "LOG_ROTATE": "-fs 20 -r 1",
            "CA_CERT": "/path/to/cacert.pem",
            "KEY_FILE": "/path/to/client.pem.key",
            "CERT_FILE": "/path/to/client.pem",
            "CRL_FILE": "/path/to/crl.pem",
            "ENABLE_MANAGEMENT": False,
            "MAX_LINK_NUM": 1000
        })
        self.assertIsNone(self.config.validate_config())

    @patch('os.getenv')
    @patch('os.path.realpath', side_effect=lambda path: path)
    def test_load_mies_config_path(self, mock_realpath, mock_getenv):
        # 模拟 MIES_INSTALL_PATH 环境变量
        current_dir = os.path.dirname(os.path.abspath(__file__))
        mock_getenv.return_value = current_dir

        # 期望的配置文件路径
        expected_config_path = os.path.join(current_dir, 'conf', 'config.json')

        # 调用静态方法 load_mies_config_path
        result = Config.load_mies_config_path()

        # 验证结果是否与期望路径匹配
        self.assertEqual(result, expected_config_path)

    @patch('os.getenv')
    def test_load_mies_config_path_env_not_set(self, mock_getenv):
        # 模拟 MIES_INSTALL_PATH 环境变量未设置
        mock_getenv.return_value = None

        # 验证当环境变量未设置时会抛出 ValueError, 需要对路径为空是增加校验
        with self.assertRaises(TypeError):
            Config.load_mies_config_path()

    def test_validate_command_line_args(self):
        with self.assertRaises(ValueError) as context:
            self.config.validate_command_line_args()
        self.assertEqual(str(context.exception), 'Integer param1 should be between 1 and 100, but got 0')

        config_path = os.path.join(BENCHMARK_HOME, "mindiebenchmark/config", "config.json")
        mock_path = "./instance"
        dataset_type = "ceval"
        mock_save_tokens_path = "./mock_save_tokens.csv"

        self.config.command_line_args.update({
            'ConfigPath': config_path,
            'DatasetPath': mock_path,
            'DatasetType': dataset_type,
            'ModelPath': mock_path,
            'SaveTokensPath': mock_save_tokens_path,
            'SavePath': mock_path,
            'TestType': "client",
            'Concurrency': 1,
            'MaxInputLen': 4095,
            'MaxOutputLen': 4095,
            "Distribution": "uniform",
            "SaveHumanEvalOutputFile": "",
            "RequestRate": "1",
            "WarmupSize": 10,
            "ManagementPort": 1026,
            "SamplingParams": "{}"
        })

        self.assertIsNone(self.config.validate_command_line_args())

    def test_get_command_line_arg(self):
        self.config.command_line_args = {'Concurrency': 128}
        self.assertEqual(self.config.get_command_line_arg("Concurrency"), 128)

    def test_get_config(self):
        self.assertIsNone(self.config.get_config("INSTANCE_PATH"))

    def test_load_config(self):
        config_path = os.path.join(BENCHMARK_HOME, "mindiebenchmark/config", "config.json")
        os.chmod(config_path, 0o640)
        self.config.load_config(config_path)
        self.assertIsNotNone(self.config.get_config("INSTANCE_PATH"))

    def test_update_config(self):
        config_path = os.path.join(BENCHMARK_HOME, "mindiebenchmark/config", "config.json")
        os.chmod(config_path, 0o640)
        self.config.update_config(config_path, {"LOG_VERBOSE": "true"})
        self.config.load_config(config_path)
        self.assertEqual(self.config.get_config("LOG_VERBOSE"), "true")

    @parameterized.expand([
        # 测试各种异常情况
        ({"Concurrency": 0, "MaxInputLen": 100, "MaxOutputLen": 100, "WarmupSize": 100}, ValueError,
         "Concurrency must be in [1, 1000]"),
        ({"Concurrency": 1001, "MaxInputLen": 100, "MaxOutputLen": 100, "WarmupSize": 100}, ValueError,
         "Concurrency must be in [1, 1000]"),
        ({"Concurrency": 10, "MaxInputLen": -1, "MaxOutputLen": 100, "WarmupSize": 100}, ValueError,
         "MaxInputLen must be in [0, 1048576]"),
        ({"Concurrency": 10, "MaxInputLen": 1048577, "MaxOutputLen": 100, "WarmupSize": 100}, ValueError,
         "MaxInputLen must be in [0, 1048576]"),
        ({"Concurrency": 10, "MaxInputLen": 100, "MaxOutputLen": 0, "WarmupSize": 100}, ValueError,
         "MaxOutputLen must be in [1, 1048576]"),
        ({"Concurrency": 10, "MaxInputLen": 100, "MaxOutputLen": 1048577, "WarmupSize": 100}, ValueError,
         "MaxOutputLen must be in [1, 1048576]"),
        ({"Concurrency": 10, "MaxInputLen": 100, "MaxOutputLen": 100, "WarmupSize": -1}, ValueError,
         "WarmupSize must be in [0, 1000]"),
        ({"Concurrency": 10, "MaxInputLen": 100, "MaxOutputLen": 100, "WarmupSize": 1001}, ValueError,
         "WarmupSize must be in [0, 1000]"),
    ])
    def test_check_args_range_invalid(self, args, expected_exception, expected_message):
        # 模拟设置参数
        self.config.command_line_args = args
        self.config.command_line_args['TestType'] = "client"
        self.config.command_line_args["RequestRate"] = "1"
        with patch(TEST_CLASS_PATH + "_check_args_path"):
            with self.assertRaises(expected_exception) as context:
                self.config.validate_command_line_args()
            self.assertIn(expected_message, str(context.exception))

    def test_validate_sampling_parameters_pass(self):
        sampling_params_str = "SamplingParams"
        self.config.command_line_args = {"DoSampling": False, sampling_params_str: '{"repetition_penalty": 1.0}'}
        self.assertIsNone(self.config.validate_sampling_parameters())
        params = self.config.command_line_args[sampling_params_str]
        self.assertAlmostEqual(params.get("repetition_penalty"), 1.0, places=9)

    def test_validate_sampling_parameters_not_a_json(self):
        self.config.command_line_args = {"DoSampling": False, "SamplingParams": '[not a json)}'}
        with self.assertRaises(ValueError) as context:
            self.config.validate_sampling_parameters()
        self.assertEqual(str(context.exception), "The value of `SamplingParams` is not a valid json string.")

    @patch(TEST_MOD_PATH + 'os.getenv')
    @patch(TEST_MOD_PATH + 'PathCheck.check_path_full')
    @patch(TEST_MOD_PATH + 'os.path.getsize')
    @patch(TEST_MOD_PATH + 'json.load')
    @patch('builtins.open', create=True)
    def test_validate_master_ip_success(self, mock_open, mock_json_load, mock_getsize, mock_check_path, mock_getenv):
        # Test case where the master IP matches the current IP
        def mock_getenv_side_effect(key):
            return {
                'RANK_TABLE_FILE': '/path/to/ranktable.json',
                'MIES_CONTAINER_IP': '192.168.1.100'
            }.get(key)

        mock_getenv.side_effect = mock_getenv_side_effect

        # Mock PathCheck to return a valid path
        mock_check_path.return_value = (True, "")

        # Mock the file size to be under the MAX_FILE_SIZE
        mock_getsize.return_value = 5 * 1024 * 1024  # 5 MB

        # Mock the content of the RANK_TABLE_FILE
        mock_json_load.return_value = {
            'server_list': [
                {
                    'server_id': '192.168.1.100',  # This matches the MIES_CONTAINER_IP
                    'device': [
                        {'rank_id': '0'}
                    ]
                }
            ]
        }

        # Call the method
        result = self.config.validate_master_ip()
        # Assert that the result is True (master IP matches the current IP)
        self.assertTrue(result)

    @patch("sys.argv", new=["script_name", "--TestType", "client",
                            "--ModelPath", "/path/to/model",
                            "--ModelName", "MyModel",
                            "--DatasetType", "text",
                            "--Concurrency", "64",
                            "--TestAccuracy", "True"])
    def test_parse_command_line_args(self):
        self.config.parse_command_line_args()

        self.assertEqual(self.config.get_command_line_arg("TestType"), "client")
        self.assertEqual(self.config.get_command_line_arg("ModelPath"), "/path/to/model")
        self.assertEqual(self.config.get_command_line_arg("ModelName"), "MyModel")
        self.assertEqual(self.config.get_command_line_arg("DatasetType"), "text")
        self.assertEqual(self.config.get_command_line_arg("Concurrency"), 64)
        self.assertTrue(self.config.get_command_line_arg("TestAccuracy"))

    def tearDown(self):
        del self.config
        patch.stopall()

if __name__ == "__main__":
    unittest.main()