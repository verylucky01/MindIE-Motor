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

from mindiebenchmark.common.config import Config
from mindiebenchmark.common.results import ResultData
from mindiebenchmark.common.output import MetricsCalculator
from mindiebenchmark.common.logging import Logger, _complete_relative_path, _split_env_value
from mindiebenchmark.inference.api import ClientFactory
from mindiebenchmark.inference.benchmarker import GPTBenchmark
from mindiebenchmark.common.exceptions import BenchmarkServerException
from mindiebenchmark.common.datasetloaders import DatasetLoaderFactory, dataset_loader_map
from mindiebenchmark.common.datasetloaders import BoolqLoader, Gsm8kLoader, TokenidLoader
from mindiebenchmark.benchmark_run import BenchmarkRun, main
from mindiebenchmark.inference.triton_client import TritonClient, ModelClient, InferRequest
from mock_object import TokenizerModel
from mock_server import server_mocker

# Check for required environment variables
missing_vars = []
ROOT_HOME = os.getenv('DT_ROOT_HOME')
if ROOT_HOME is None:
    missing_vars.append('DT_ROOT_HOME')

BENCHMARK_HOME = os.getenv('DT_BENCHMARK_SRC_HOME')
if BENCHMARK_HOME is None:
    missing_vars.append('DT_BENCHMARK_SRC_HOME')

TEST_HOME = os.getenv('DT_BENCHMARK_TEST_HOME')
if TEST_HOME is None:
    missing_vars.append('DT_BENCHMARK_TEST_HOME')

MOCK_HOME = os.getenv('DT_BENCHMARK_MOCK_HOME')
if MOCK_HOME is None:
    missing_vars.append('DT_BENCHMARK_MOCK_HOME')

# Raise an exception if any variables are missing
if missing_vars:
    raise EnvironmentError(f"Missing environment variables: {', '.join(missing_vars)}")