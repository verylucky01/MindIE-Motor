#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
# MindIE is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#         http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

import os
import sys
import gevent
import gevent.pool

# append mindie service path
python_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
mindieservice_path = os.path.dirname(os.path.dirname(python_path))
sys.path.append(mindieservice_path)
from mindieclient.python.common.logging import Log
from .result import Result
from .utils import (
    check_response,
    raise_error,
    _check_variable_type,
    _check_invalid_url
)
from .clients import (OpenAIChatTextClient, OpenAIChatStreamClient, SelfTokenStreamClient, TritonTextClient,
                      TritonStreamClient, TritonTokenInferTextClient, TritonTokenAsyncInferTextClient,
                      OpenAITextClient, OpenAIStreamClient)


class AsyncRequest:
    """Implement a class that process asynchronous responses in triton token inference mode
    """
    def __init__(self, greenlet, verbose=False):
        """
        Args:
            _res_verbose: show processing information in detail
            _greenlet: the size of gevent pool
        """
        self._res_verbose = verbose
        self._greenlet = greenlet

    def get_result(self, timeout=60):
        """Process the asynchronous responses

        Args:
            timeout: time out
        """
        if timeout is None:
            raise_error("The timeout should be more than 0!")
        _check_variable_type({'timeout': timeout}, (int, float))
        if timeout <= 0:
            raise_error("The timeout should be more than 0!")
        try:
            response = self._greenlet.get(timeout=timeout)
        except gevent.Timeout:
            raise_error("Failed to obtain inference response!")
        check_response(response)
        return Result(response.data, self._res_verbose)


DEFAULT_CLIENT_KEY = "triton_text"


class MindIEHTTPClient:
    """Implement a class that Send requests and receive responses in different mode
    """
    def __init__(
        self,
        url,
        greenlet_size=None,
        enable_ssl=True,
        ssl_options=None,
        network_timeout=600.0,
        connection_timeout=600.0,
        concurrency=1,
        verbose=False,
        enable_management=False,
        management_url="https://127.0.0.2:1026",
    ):
        """
        Args:
            url: business address of server
            greenlet_size: the size of gevent pool
            enable_ssl: the symbol indicates whether the communication uses https protocal or not
            ssl_options: ssl options while communication uses https protocal
            network_timeout: network time out /s
            connection_timeout: connect time out /s
            concurrency: request count
            verbose: show processing information in detail
            enable_management: use managment address to query server status
            management_url: managment address of server
        """
        client_cls_map = {
            "openai_chat_text": OpenAIChatTextClient,
            "openai_chat_stream": OpenAIChatStreamClient,
            "openai_completions_text": OpenAITextClient,
            "openai_completions_stream": OpenAIStreamClient,
            "self_token_stream": SelfTokenStreamClient,
            "triton_text": TritonTextClient,
            "triton_stream": TritonStreamClient,
            "infer_text": TritonTokenInferTextClient,
            "async_infer_text": TritonTokenAsyncInferTextClient,
        }
        _check_variable_type({
            "enable_ssl": enable_ssl,
            "verbose": verbose,
            "enable_management": enable_management,
        }, bool)
        if enable_management:
            _check_invalid_url(url, enable_ssl)
        self.logger = Log(__name__).getlog()
        self.client_instance_map = dict()
        for key, cls in client_cls_map.items():
            self.client_instance_map[key] = \
                cls(url, greenlet_size, enable_ssl, ssl_options, network_timeout, connection_timeout, concurrency,
                    verbose, enable_management, management_url)

    def __del__(self):
        self.close()

    def is_server_live(self):
        """check if server is still alive
        """
        return self.client_instance_map[DEFAULT_CLIENT_KEY].is_server_live()

    def is_server_ready(self):
        """check if server is ready to process request
        """
        return self.client_instance_map[DEFAULT_CLIENT_KEY].is_server_ready()

    def get_server_metadata(self):
        """check metadata in server
        """
        return self.client_instance_map[DEFAULT_CLIENT_KEY].get_server_metadata()

    def is_model_ready(self, model_name, model_version=""):
        """check if the specific model is supported by server

        Args:
            model_name: the name of model
            model_version: the version of model
        """
        return self.client_instance_map[DEFAULT_CLIENT_KEY].is_model_ready(model_name, model_version)

    def get_model_metadata(self, model_name, model_version=""):
        """Get model metadata from server

        Args:
            model_name: the name of model
            model_version: the version of model
        """
        return self.client_instance_map[DEFAULT_CLIENT_KEY].get_model_metadata(model_name, model_version)

    def get_model_config(self, model_name, model_version=""):
        """Get model config message from server

        Args:
            model_name: the name of model
            model_version: the version of model
        """
        return self.client_instance_map[DEFAULT_CLIENT_KEY].get_model_config(model_name, model_version)

    def close(self):
        """close the gevent pool and close all connection in the pool
        """
        for _, client_instance in self.client_instance_map.items():
            client_instance.close()

    def infer(
        self,
        model_name,
        inputs,
        model_version="",
        outputs=None,
        request_id="",
        parameters=None,
    ):
        """triton token inference mode

        Args:
            model_name: the name of model
            inputs: input tokens
            model_version: the version of model
            outputs: output tokens
            request_id: id of current request
            parameters: inference parameters
        """
        return self.client_instance_map["infer_text"].request(
            inputs=inputs,
            model_name=model_name,
            is_stream=False,
            request_id=request_id,
            parameters=parameters,
            outputs=outputs
        )

    def async_infer(
        self,
        model_name,
        inputs,
        model_version="",
        outputs=None,
        request_id="",
        parameters=None,
    ):
        """triton token asynchronous inference

        Args:
            model_name: the name of model
            inputs: input tokens
            model_version: the version of model
            outputs: output tokens
            request_id: id of current request
            parameters: inference parameters
        """
        return self.client_instance_map["async_infer_text"].request(
            inputs=inputs,
            model_name=model_name,
            is_stream=False,
            request_id=request_id,
            parameters=parameters,
            outputs=outputs,
        )

    def generate(
        self,
        model_name,
        prompt,
        model_version="",
        request_id="",
        parameters=None,
    ):
        """triton text inference

        Args:
            model_name: the name of model
            prompt: input text
            model_version: the version of model
            request_id: id of current request
            parameters: inference parameters
        """
        return self.client_instance_map["triton_text"].request(
            model_name=model_name,
            inputs=prompt,
            is_stream=False,
            request_id=request_id,
            parameters=parameters,
        )

    def generate_stream(
        self,
        model_name,
        prompt,
        model_version="",
        request_id="",
        parameters=None,
    ):
        """triton text inference in stream mode

        Args:
            model_name: the name of model
            prompt: input text
            model_version: the version of model
            request_id: id of current request
            parameters: inference parameters
        """
        for res_ in self.client_instance_map["triton_stream"].request(
            model_name=model_name,
            inputs=prompt,
            request_id=request_id,
            parameters=parameters,
        ):
            yield res_["text_output"]
        self.client_instance_map["triton_stream"].remove_request_id(request_id)

    def generate_stream_self(
        self,
        token,
        request_id,
        parameters,
    ):
        """mindie token inference in stream mode

        Args:
            token: input tokens
            request_id: id of current request
            parameters: inference parameters
        """
        return self.client_instance_map["self_token_stream"].request(
            inputs=token,
            model_name="token_model",
            request_id=request_id,
            parameters=parameters,
        )

    def generate_stream_with_details(
        self,
        model_name,
        prompt,
        model_version="",
        request_id="",
        parameters=None,
    ):
        """triton text inference in stream mode with detailed message

        Args:
            model_name: the name of model
            prompt: input text
            model_version: the version of model
            request_id: id of current request
            parameters: inference parameters
        """
        return self.client_instance_map["triton_stream"].request(
            model_name=model_name,
            inputs=prompt,
            request_id=request_id,
            parameters=parameters,
        )

    def generate_chat(
        self,
        model_name,
        prompt,
        request_id="",
        parameters=None
    ):
        """openai text inference

        Args:
            model_name: the name of model
            prompt: input text
            request_id: id of current request
            parameters: inference parameters
        """
        return self.client_instance_map["openai_chat_text"].request(
            model_name=model_name,
            inputs=prompt,
            request_id=request_id,
            parameters=parameters,
        )

    def generate_chat_stream(
        self,
        model_name,
        prompt,
        request_id="",
        parameters=None
    ):
        """openai text inference in stream mode

        Args:
            model_name: the name of model
            prompt: input text
            request_id: id of current request
            parameters: inference parameters
        """
        return self.client_instance_map["openai_chat_stream"].request(
            model_name=model_name,
            inputs=prompt,
            request_id=request_id,
            parameters=parameters,
        )

    def generate_completions(
        self,
        model_name,
        prompt,
        request_id="",
        parameters=None
    ):
        return self.client_instance_map["openai_completions_text"].request(
            model_name=model_name,
            inputs=prompt,
            request_id=request_id,
            parameters=parameters,
        )

    def generate_completions_stream(
        self,
        model_name,
        prompt,
        request_id="",
        parameters=None
    ):
        return self.client_instance_map["openai_completions_stream"].request(
            model_name=model_name,
            inputs=prompt,
            request_id=request_id,
            parameters=parameters,
        )

    def cancel(
        self,
        model_name,
        request_id,
        model_version="",
    ):
        """cancel all inference in server

        Args:
            model_name: the name of model
            request_id: id of current request
            model_version: the version of model
        """
        def instance_cancel(instance) -> int:
            if request_id not in instance.get_requests_list():
                return not_found_status
            return int(instance.cancel(model_name, request_id, model_version))

        not_found_status = -1
        supported_keys = ["triton_text", "triton_stream"]
        for key in supported_keys:
            result = instance_cancel(self.client_instance_map[key])
            if result != not_found_status:
                return bool(result)
        self.logger.warning(f"[MIE02E000407] Don't support to cancel {request_id}")
        return False


    def get_slot_count(self, model_name, model_version=""):
        """query the slot count of server

        Args:
            model_name: the name of model
            model_version: the version of model
        """
        return self.client_instance_map[DEFAULT_CLIENT_KEY].get_slot_count(model_name, model_version)
