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

import copy


class MockBenchmarkConfig:
    def __init__(self):
        self._config = {
            "TestType": "client",
            "Http": "http://127.0.0.1:1077",
            "ManagementHttp": "http://127.0.0.2:1078",
            "ModelName": "llama2_7b",
            "ConfigPath": "mock_config_path",
            "ModelPath": "tests/benchmark/config/model_config",
            "DatasetType": "gsm8k",
            "DatasetPath": "tests/benchmark/mock_dataset",
            "Concurrency": 1,
            "MaxInputLen": 512,
            "MaxOutputLen": 1024,
            "Tokenizer": True,
            "DoSampling": False,
            "TestAccuracy": False,
            "SavePath": "/path/to/save",
            "SaveHumanEvalOutputFile": "output.jsonl",
            "TaskKind": "stream",
            "RequestRate": "",
            "Distribution": "uniform",
            "WarmupSize": 10,
            "SamplingParams": "{}",
            "SyntheticConfigPath": "tests/benchmark/config/synthetic_config"
        }

    def set_config_value(self, key, value):
        self._config[key] = value

    def get_config(self):
        """Returns a deep copy of the default command config."""
        return copy.deepcopy(self._config)

    def update(self, key, value):
        """
        update key-value and return a deepcopy of MockBenchmarkConfig object
        """
        # Create a deep copy of the current object
        new_config_obj = copy.deepcopy(self)

        # Use the protected method to update the config in the copied object
        new_config_obj.set_config_value(key, value)

        return new_config_obj
