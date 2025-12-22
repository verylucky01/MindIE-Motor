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

from mindiebenchmark.common.config import Config
from mindiebenchmark.common.datasetloaders.base_loader import BaseLoader
from mindiebenchmark.common.results import ResultData
from mindiebenchmark.common.logging import Logger

from mindieclient.python.httpclient.clients import (
    OpenAIChatTextClient, OpenAIChatStreamClient, OpenAIStreamClient, TritonTextClient, TritonStreamClient,
    SelfTokenStreamClient, TGIClient, VLLMClient
)

from .openai_api import OpenAIChatTextAPI, OpenAIChatStreamAPI, OpenAIStreamAPI
from .mindie_api import MindIETokenAPI
from .triton_api import TritonTextAPI, TritonStreamAPI
from .tgi_api import TgiAPI
from .vllm_api import VllmAPI


api_map = {
    "vllm_client": {
        "stream": {
            True: [VllmAPI, VLLMClient],
            False: [OpenAIStreamAPI, OpenAIStreamClient],
        },
    },
    "tgi_client": {
        "stream": {
            True: [TgiAPI, TGIClient],
            False: [TgiAPI, TGIClient],
        },
    },
    "openai": {
        "text": {
            True: [OpenAIChatTextAPI, OpenAIChatTextClient],
            False: [OpenAIChatTextAPI, OpenAIChatTextClient],
        },
        "stream": {
            True: [OpenAIChatStreamAPI, OpenAIChatStreamClient],
            False: [OpenAIChatStreamAPI, OpenAIChatStreamClient],
        },
    },
    "client": {
        "text": {
            True: [TritonTextAPI, TritonTextClient],
            False: [TritonTextAPI, TritonTextClient],
        },
        "stream": {
            True: [TritonStreamAPI, TritonStreamClient],
            False: [TritonStreamAPI, TritonStreamClient],
        },
        "stream_token": {
            True: [MindIETokenAPI, SelfTokenStreamClient],
        },
    }
}


class ClientFactory:
    @staticmethod
    def make_client_instance(client_config: Config, result_container: ResultData, gevent_pool_size: int = 16,
                             dataloader: BaseLoader | None = None):
        test_type = client_config.get_command_line_arg("TestType")
        task_kind = client_config.get_command_line_arg("TaskKind").lower()

        # Parameter 'stream' in --SamplingParams has higher priority and will overwrite the value
        # given by other command line parameters.
        is_stream = client_config.get_command_line_arg("SamplingParams").get("stream")
        if is_stream and test_type not in ("vllm_client", "openai", "client"):
            Logger().logger.warning("Parameter 'stream' only support --TestType 'vllm_client', 'openai' or 'client'")
        elif is_stream is not None:
            task_kind = "stream" if is_stream else task_kind

        if task_kind not in api_map.get(test_type):
            raise ValueError(f"{test_type} doesn't support {task_kind}")
        health_api, health_client = api_map.get(test_type).get(task_kind).get(True)
        health_client = health_api(
            client_config,
            result_container,
            TritonTextClient,
            gevent_pool_size,
            dataloader
        ).client_instance
        try:
            is_mindie_backend = health_client.is_server_ready()
        except Exception as e:
            is_mindie_backend = False
        if is_mindie_backend not in api_map.get(test_type).get(task_kind):
            backend_str = "mindie" if is_mindie_backend else "third party server"
            raise ValueError(f"{backend_str} doesn't support {test_type} {task_kind}")
        benchmark_api, mindie_client = api_map.get(test_type).get(task_kind).get(is_mindie_backend)
        return benchmark_api(client_config, result_container, mindie_client, gevent_pool_size, dataloader)
