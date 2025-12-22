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

import ctypes
import getpass
import json
import os
import sys
import time
import urllib.parse
import re
import uuid
from abc import abstractmethod, ABC
from ssl import create_default_context, Purpose

import gevent
import gevent.pool
import urllib3
from urllib3.util.retry import Retry
from geventhttpclient.url import URL

# append mindie service path
python_path = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
mindieservice_path = os.path.dirname(os.path.dirname(python_path))
sys.path.append(mindieservice_path)
from mindieclient.python.common.logging import Log
from mindieclient.python.httpclient.utils import (raise_error, check_password, check_sampling_params, \
    check_response, validate_model_string, CRL_FILE, SSL_MUST_KEYS, _check_variable_type, _check_invalid_url, \
    _check_invalid_ssl_path, _check_invalid_ssl_filesize, _check_ca_cert)
from mindieclient.python.httpclient.thread_utils import ThreadSafeFactory


def _stream_data_split(stream_data_line):
    stream_data_line = stream_data_line.lstrip("data:")
    stream_data_line = re.sub(r'[\n\0]+$', '', stream_data_line)
    if "data:{" in stream_data_line:
        stream_data_line = re.sub(r'\}\s*data:{', '} ,{', stream_data_line)
    else:
        stream_data_line = re.sub(r'\}\x00*\s*{', '} ,{', stream_data_line)
    stream_data_line = '[' + stream_data_line + ']'
    json_obj_arr = json.loads(stream_data_line)
    return json_obj_arr


class MindIEBaseClient(ABC):
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
        self.logger = Log(__name__).getlog()
        _check_variable_type({
            "enable_ssl": enable_ssl,
            "verbose": verbose,
            "enable_management": enable_management,
        }, bool)
        if enable_management:
            _check_invalid_url(management_url, enable_ssl)
        # 1000 is the maximum number of connections the server supports
        if greenlet_size is not None:
            _check_variable_type({"greenlet_size": greenlet_size}, int)
            if greenlet_size <= 0 or greenlet_size > 1e9:
                raise_error(
                    "[MIE02E000005] The value of greenlet_size should be in range [1, 1000000000]!"
                )
        if network_timeout:
            _check_variable_type({
                "network_timeout": network_timeout,
            }, (int, float))
        if connection_timeout:
            _check_variable_type({
                "connection_timeout": connection_timeout,
            }, (int, float))
        if concurrency <= 0 or concurrency > 4096:
            raise_error(
                "The number of connections create for client should be  in range [1, 4096]!"
            )
        connection_timeout_invalid = connection_timeout and connection_timeout <= 0
        network_timeout_invalid = network_timeout and network_timeout <= 0
        if (connection_timeout_invalid) or (network_timeout_invalid):
            raise_error("Connection timeout and network timeout should be more than 0!")

        self.valid_url = URL(url)
        self.manage_url = management_url
        self._common_url = url
        self._context = None
        self.validate_ssl(enable_ssl, ssl_options)
        retries = Retry(
            total=3,
            status_forcelist=[104], # To solve the problem of Connection aborted
            allowed_methods=["POST"]
        )
        if enable_ssl:
            self._http_pool_manager = urllib3.PoolManager(ssl_context=self._context, retries=retries)
        else:
            self._http_pool_manager = urllib3.PoolManager(retries=retries)
        self._pool = gevent.pool.Pool(greenlet_size)
        self._verbose = verbose
        self._requests_list = ThreadSafeFactory.make_threadsafe_instance(set)

        self._timeout = min(network_timeout, connection_timeout) \
            if connection_timeout and network_timeout else None

    def __del__(self):
        self.close()

    @staticmethod
    def validate_input(inputs):
        if not isinstance(inputs, str) and not isinstance(inputs, list):
            raise_error("[MIE02E000002] The prompt should be a string or a list!")
        if len(inputs) < 1 or len(inputs) > 4294967295:
            raise_error(" The length of prompt should be in range [1, 4294967295], but got {}!".format(len(inputs)))

    @staticmethod
    def validate_output(outputs):
        if outputs is not None and len(outputs) != 1:
            raise_error("The size of requested outputs only supports 1")

    @abstractmethod
    def construct_request_body(
        self,
        model_name: str,
        inputs,
        request_id: str,
        parameters: dict = None,
        outputs=None
    ) -> dict:
        pass

    @abstractmethod
    def process_response(self, response, last_time_point):
        pass

    def request_uri(self, model_name: str) -> str:
        return ""

    def validate_ssl(self, enable_ssl: bool, ssl_options: dict):
        if not enable_ssl:
            self.logger.warning("[MIE02W000007] SSL authentication is disabled!")
            return
        if CRL_FILE in ssl_options.keys():
            SSL_MUST_KEYS.append(CRL_FILE)
        _check_invalid_ssl_path(ssl_options)
        _check_invalid_ssl_filesize(ssl_options)
        password = getpass.getpass("Please enter the password: ")
        check_password(password)
        _check_ca_cert(ssl_options, bytes(password, 'utf-8'))
        self._context = create_default_context(Purpose.SERVER_AUTH)
        self._context.set_ciphers('DEFAULT:!aNULL:!eNULL:!LOW:!EXP:!RC4:!MD5:!CBC')
        self._context.load_verify_locations(cafile=ssl_options["ca_certs"])
        self._context.load_cert_chain(
            certfile=ssl_options["certfile"],
            keyfile=ssl_options["keyfile"],
            password=password
        )
        password_len = len(password)
        password_offset = sys.getsizeof(password) - password_len - 1
        ctypes.memset(id(password) + password_offset, 0, password_len)

    def is_server_live(self):
        return self._get(urllib.parse.urljoin(self.manage_url, "/v2/health/live")).status == 200

    def is_server_ready(self):
        return self._get(urllib.parse.urljoin(self.manage_url, "/v2/health/ready")).status == 200

    def get_server_metadata(self):
        response = self._get(urllib.parse.urljoin(self._common_url, "/v2"))
        check_response(response)
        json_content = response.data.decode()
        return json.loads(json_content)

    def is_model_ready(self, model_name, model_version=""):
        return self._get(urllib.parse.urljoin(self.manage_url, f"/v2/models/{model_name}/ready")).status == 200

    def get_model_metadata(self, model_name, model_version=""):
        response = self._get(urllib.parse.urljoin(str(self.valid_url), f"/v2/models/{model_name}"))
        check_response(response)
        return json.loads(response.data.decode())

    def get_model_config(self, model_name, model_version=""):
        response = self._get(urllib.parse.urljoin(str(self.valid_url), f"/v2/models/{model_name}/config"))
        check_response(response)
        return json.loads(response.data.decode())

    def get_slot_count(self, model_name, model_version=""):
        response = self._get(urllib.parse.urljoin(self.manage_url, f"/v2/models/{model_name}/getSlotCount"))
        check_response(response)
        from mindieclient.python.httpclient import Result
        return Result(response.data, self._verbose)

    def close(self):
        self._http_pool_manager.clear()

    def do_request(
        self,
        request_uri: str,
        request_body: dict,
        request_method: str = "POST",
        is_stream: bool = True
    ):
        return self._http_pool_manager.request(
            request_method,
            urllib.parse.urljoin(str(self.valid_url), request_uri),
            headers={"Content-Type": "application/json"},
            body=json.dumps(request_body).encode(),
            timeout=self._timeout,
            preload_content=not is_stream
        )

    def remove_request_id(self, request_id):
        self._requests_list.remove(request_id)

    def request(
        self,
        inputs,
        model_name: str = "",
        is_stream: bool = True,
        method: str = "POST",
        uri: str = "",
        request_id: str = "",
        parameters: dict = None,
        outputs=None
    ):
        if parameters is None:
            parameters = {}
        self.validate_input(inputs)
        self.validate_output(outputs)
        check_sampling_params(parameters)
        validate_model_string(model_name)
        request_id = self._check_request_id(request_id)
        if not uri:
            uri = self.request_uri(model_name)
        response = None
        try:
            request_body = self.construct_request_body(
                model_name,
                inputs,
                request_id,
                parameters=parameters,
                outputs=outputs
            )
            last_time_point = time.perf_counter()
            response_raw = self.do_request(uri, request_body, method, is_stream)
            response = self.process_response(response_raw, last_time_point)
            if not is_stream and hasattr(response_raw, 'data') and callable(getattr(response_raw.data, 'decode', None)):
                response_obj = json.loads(response_raw.data.decode())
                err_msg = response_obj.get("error", None)
                if err_msg:
                    raise_error("Request failed, response from server is {}.".format(err_msg))
            return response
        except urllib3.exceptions.TimeoutError:
            raise_error("The http request timeout.")
        except urllib3.exceptions.RequestError as err:
            raise_error("Request failed and the reason is {}.".format(err))
        except json.JSONDecodeError as err:
            raise_error("Request failed and the reason is {}.".format(err))
        finally:
            if not is_stream:
                self.remove_request_id(request_id)
        return response

    def get_requests_list(self):
        return self._requests_list

    def cancel(
        self,
        model_name,
        request_id,
        model_version="",
    ):
        if request_id not in self._requests_list:
            raise_error("Request id should belong to one of the running requests!")

        response = self._post(
            request_uri=urllib.parse.urljoin(str(self.valid_url), f"/v2/models/{model_name}/stopInfer"),
            request_body={"id": request_id},
        )
        check_response(response)
        result = json.loads(response.data.decode("utf-8"))
        if response.status == 200 and result["id"] == request_id:
            self.logger.info("Cancel %s succeeded!", request_id)
            return True
        else:
            self.logger.error("[MIE02E000407] Cancel %s failed! Please check the status of server.", request_id)
            return False

    def _check_request_id(self, request_id):
        _check_variable_type({"request_id": request_id}, str)
        if len(request_id) > 1000:
            raise_error("[MIE02E000002]The length of request_id should be no more than 1000, "
                        "but got {}".format(len(request_id)))
        if request_id in self._requests_list:
            raise_error("Duplicate request_id {}, please change!".format(request_id))
        if not request_id:
            request_id = uuid.uuid4().hex
            self.logger.info("Request_id %s is generated for this request", request_id)
        self._requests_list.add(request_id)
        return request_id

    def _get(self, request_uri):
        if self._verbose:
            self.logger.info("GET %s", request_uri)
        try:
            response = self._http_pool_manager.request(
                "GET",
                request_uri,
                timeout=self._timeout,
            )
        except urllib3.exceptions.TimeoutError:
            raise_error("The get http request timeout.")
        except urllib3.exceptions.RequestError as err:
            raise_error("Request failed and the reason is {}.".format(err))
        if self._verbose:
            self.logger.info("Get response: %s", response.data.decode())
        return response

    def _post(self, request_uri, request_body):
        if self._verbose:
            self.logger.info("POST %s \n %s", request_uri, request_body)
        try:
            response = self._http_pool_manager.request("POST",
                                                       request_uri, body=json.dumps(request_body).encode(),
                                                       timeout=self._timeout,
                                                       )
        except urllib3.exceptions.TimeoutError:
            raise_error("POST request timeout occurred.")
        except urllib3.exceptions.RequestError as err:
            raise_error("Request failed and the reason is {}.".format(err))
        if self._verbose:
            self.logger.info("Post response: %s", response.data.decode())
        return response


class StreamClientMindIE(MindIEBaseClient, ABC):
    @staticmethod
    def preprocess_cur_line(cur_line: str) -> str:
        return cur_line

    @abstractmethod
    def process_stream_line(self, json_content: dict) -> dict:
        pass

    def process_response(self, response, last_time_point):
        time_name = "prefill_time"

        for byte_line in response.stream():
            if byte_line == b"\n":
                continue
            cur_line = self.preprocess_cur_line(byte_line.decode())
            try:
                for json_content in _stream_data_split(cur_line):
                    cur_time_point = time.perf_counter()
                    response_dict = self.process_stream_line(json_content)
                    response_dict[time_name] = (cur_time_point - last_time_point) * 1000
                    yield response_dict
                    time_name = "decode_time"
                    last_time_point = time.perf_counter()
            except ValueError as value_error:
                raise_error(f"Request failed and the reason is abnormal value: {value_error}, "
                            f"response from server is: {cur_line}")
            except KeyError as key_error:
                raise_error(f"Request failed and the reason is abnormal key: {key_error}, "
                            f"response from server is: {cur_line}")
            except Exception as error:
                raise_error(f"Request failed and the reason is {error}, response from server is: {cur_line}")