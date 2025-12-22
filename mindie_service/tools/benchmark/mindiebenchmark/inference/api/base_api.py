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
import signal
import time
import uuid
import threading
from abc import abstractmethod, ABC
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor

import gevent
import numpy
import urllib3
from gevent.pool import Pool

from mindiebenchmark.common.exceptions import BenchmarkServerException
from mindiebenchmark.common.tokenizer import BenchmarkTokenizer
from mindiebenchmark.common.config import Config
from mindiebenchmark.common.datasetloaders.base_loader import BaseLoader, extract_dataset_type_info
from mindiebenchmark.common.logging import Logger
from mindiebenchmark.common.results import ResultData
from mindiebenchmark.inference.middle_result import MiddleResult

from mindieclient.python.httpclient.clients import MindIEBaseClient
from mindieclient.python.httpclient.utils import MindIEClientException


class BaseAPI(ABC):
    """Base client of benchmark.

    This class is the abstract of different clients. Its duty includes reading data from dataloader,
    sending request to server, receiving response from server, recording some information to
    calculate performance metrics and saving the inference results etc.
    """
    def __init__(self, client_config: Config, result_container: ResultData, client_cls: MindIEBaseClient,
                 gevent_pool_size: int = 16, dataloader: BaseLoader | None = None):
        """Initialize the instance of benchmarker that in client mode.

        Args:
            client_config (Config): Configuration for client instance.
            result_container (ResultData): It stores the generated information during inference to
                calculate the metrics.
            gevent_pool_size(int): Maximum coroutines at the same time during pressure test. It
                should be less than maximum connections allowed.
            dataloader (BaseLoader | None): The dataloader load data and convert to specific format.
        """

        def new_middle():
            return MiddleResult()

        self.enable_management = client_config.get_config("ENABLE_MANAGEMENT")
        self.max_conn = client_config.get_config("MAX_LINK_NUM")
        self.logger = Logger().logger
        self._config = client_config
        self.results = result_container
        self.model_name = client_config.get_command_line_arg("ModelName")
        self.model_path = client_config.get_command_line_arg("ModelPath")
        model_version = client_config.get_command_line_arg("ModelVersion")
        self.model_version = model_version if model_version else ""
        self.max_new_token = client_config.get_command_line_arg("MaxOutputLen")
        self.server_url = client_config.get_command_line_arg("Http")
        self.management_url = client_config.get_command_line_arg("ManagementHttp")
        self.timeout = client_config.get_command_line_arg("RequestTimeOut")
        self.workersnum = client_config.get_command_line_arg("WorkersNum")
        self.is_stream = client_config.get_command_line_arg("TaskKind") != "text"
        self.client_instance = self.init_client(client_cls)
        self.pool_size = min(gevent_pool_size, self.max_conn)
        if self.pool_size <= 0:
            self.logger.warning(f"Invalid pool size {self.pool_size}, has been reset to 128")
            self.pool_size = 128
        self.lock = threading.Lock()
        
        self.dataloader = dataloader
        self.lora_models = self.dataloader.lora_models
        self.all_results = []
        if self.dataloader.dataset_type == "synthetic":
            self.logger.warning("The number of output tokens is set by file --SyntheticConfigPath, "
                                "since the --DatasetType is 'synthetic'. The parameter in command "
                                "line (e.g. --MaxOutputLen, 'max_new_tokens' and 'max_tokens') "
                                "will be ignored.")
        self.do_sampling = client_config.get_command_line_arg("DoSampling")
        if self.do_sampling:
            self.logger.info("DoSampling enable.")
        else:
            self.logger.info("DoSampling is disabled")
        self.sample_params = client_config.get_command_line_arg("SamplingParams")
        self.validate_sampling_parameters()
        # 'stream' is used to choose client, it should not send to client directly, though it's in --SamplingParams
        self.sample_params.pop("stream", None)
        self.filter_sampling_parameters()
        self.get_inference_parameters()
        self.logger.info("Sampling params is %s.", self.sample_params)
        self.result_caching = defaultdict(new_middle)
        self.dataset_type = client_config.get_command_line_arg("DatasetType")
        self.dataset_path = client_config.get_command_line_arg("DatasetPath")
        self.req_result_data = {}

        self.test_acc = client_config.get_command_line_arg("TestAccuracy")
        self.logger.info("Client init finished. Gevent pool size is %d, max connection for mindie-client is %d.",
                         self.pool_size, self.max_conn)
        self.reset_states()
        self.check_if_ready()

    @abstractmethod
    def request(self, req_id: str, data):
        """Send request to server based on given data.

        Args:
            req_id (str): Unique request id.
            data (any): The data will send to server to execute reference.
        """
        pass

    def validate_sampling_parameters(self):
        pass

    def filter_sampling_parameters(self):
        pass

    def get_inference_parameters(self):
        params = {
            "details": True,
            "max_new_tokens": self.max_new_token,
            "do_sample": self.do_sampling,
        }
        params.update(self.sample_params)
        return params

    def init_client(self, client_cls):
        try:
            return client_cls(
                url=self.server_url,
                ssl_options=self.get_ssl_options(),
                enable_ssl=self.server_url.startswith("https"),
                concurrency=self.max_conn,
                enable_management=self.enable_management,
                management_url=self.management_url,
                network_timeout=self.timeout,
                connection_timeout=self.timeout,
            )
        except MindIEClientException as e:
            self.client_instance = None
            raise RuntimeError(f"Client instance initialization error with message "
                               f"{e.get_message()} Please check your config!") from e
        except Exception as e:
            self.client_instance = None
            raise RuntimeError(f"Client instance initialization error with message "
                               f"{e} Please check your config!") from e

    def get_ssl_options(self):
        def check_single(fn: str) -> bool:
            return fn is not None and os.path.exists(fn) and os.path.isfile(fn)

        ssl_options = {}
        enable_ssl = self.server_url.startswith("https")
        if not enable_ssl:
            return ssl_options

        ssl_config_key_map = {
            "ca_certs": "CA_CERT",
            "keyfile": "KEY_FILE",
            "certfile": "CERT_FILE",
            "crlfile": "CRL_FILE"
        }
        for ssl_key, config_key in ssl_config_key_map.items():
            config_val = self._config.get_config(config_key)
            if check_single(config_val):
                ssl_options[ssl_key] = config_val
        if all(ssl_key in ssl_options.keys() for ssl_key in list(ssl_config_key_map.keys())[:3]):
            return ssl_options
        raise RuntimeError("Invalid ssl options when using https protocol.")

    def reset_states(self):
        self.logger.info("Reset states...")
        self.results.total_req = 0
        self.results.success_req = 0
        self.results.empty_req = 0
        self.results.returned_req = 0
        self.result_caching.clear()

    def check_if_ready(self):
        if not self.enable_management:
            self.logger.warning("[MIE01W000102] Do not check the status of server because management port is disabled")
            return
        if not self.client_instance.is_server_ready():
            raise BenchmarkServerException(f"Server {self.server_url} is not ready !!!")
        if not self.client_instance.is_server_live():
            raise BenchmarkServerException(f"Server {self.server_url} is not alive !!!")
        if not self.client_instance.is_model_ready(self.model_name, self.model_version):
            raise BenchmarkServerException(f"Model {self.model_name} ver. {self.model_version} is not ready on "
                                           f"server {self.server_url} !!!")
        if self.lora_models:
            for lora_model in self.lora_models:
                if not self.client_instance.is_model_ready(lora_model, self.model_version):
                    raise BenchmarkServerException(f"Lora Model {lora_model} ver. {self.model_version} is not ready on "
                                           f"server {self.server_url} !!!")

    def graceful_exit(self):
        self.logger.info("Detected Ctrl+C. Begin to graceful exit.")
        urllib3.PoolManager().clear()
        for requests in self.client_instance.get_requests_list():
            if requests not in self.result_caching.keys():
                continue
            if not self.result_caching[requests].success:
                self.result_caching.pop(requests)
        self.save_results()
        # to sync results to Results
        self.report_results()

    def inc_total_req(self):
        if self.results.total_req % 10 == 0:
            self.logger.info("Total %d requests are sent to server, %d returned, %d empty.",
                             self.results.total_req,
                             self.results.returned_req, self.results.empty_req)
        self.results.add_total_req()

    def process_input_data(self, data, rrid: str):
        if not isinstance(data, str):
            return data
        if self.dataloader:
            self.result_caching[rrid].num_input_tokens = len(
                BenchmarkTokenizer.encode(data, self.results.tokenizer_time))
        else:
            self.result_caching[rrid].num_input_tokens = len(data)
        self.result_caching[rrid].num_input_chars = len(data)
        return data

    def prepare_input(self, data_dict: dict):
        """Prepare data according to input data dict, ready for connection with endpoint.

        Args:
            data_dict (dict): It should have keys `data` and `id`.

        Returns:
            rrid (str): Unique identifier for each request.
            data (ndarray or string): The input text ro token-ids that will input to server.
        """
        if not all(key in data_dict for key in ("data", "id")):
            raise RuntimeError(f"Input data must have `id` and `data` key but got {data_dict.keys()}")
        data = data_dict.get("data")
        rrid = uuid.uuid4().hex
        self.result_caching[rrid].request_id = rrid
        self.result_caching[rrid].data_id = data_dict.get("id")
        self.result_caching[rrid].input_data = data
        self.result_caching[rrid].data_option = data_dict.get("options", [])
        self.result_caching[rrid].num_expect_generated_tokens = data_dict.get("num_expect_generated_tokens", None)
        self.result_caching[rrid].model_id = data_dict.get("model_id")
        return rrid, self.process_input_data(data, rrid)

    def report_results(self):
        """After finishing the task, convert the cached content to final result.
        """
        if self.results.total_req <= 0:
            return
        if not self.result_caching:
            self.logger.error("[MIE01E000005] All requests failed, please check dataset path or other settings.")
            return
        total_start = min([item.start_time for _, item in self.result_caching.items()])
        total_end = max([item.end_time for _, item in self.result_caching.items()])
        self.logger.info(f"Converting middle results to final results "
                         f"and exit the inference process: {os.getpid()}...")
        self.results.total_time_elapsed = total_end - total_start
        self.all_results = [result.convert_to_standard() for _, result in self.result_caching.items()]
        self.results.add_req_res(self.all_results)

    def update_max_new_token(self, data_dict):
        if self.dataset_type == "synthetic":
            self.max_new_token = data_dict.get("max_new_tokens", self.max_new_token)

    def data_handler(self, data_dict: dict):
        with self.lock:
            self.inc_total_req()
            req_id, data = self.prepare_input(data_dict)
            self.update_max_new_token(data_dict)
        try:
            self.result_caching[req_id].start_time = time.perf_counter()
            self.request(req_id, data)
            self.result_caching[req_id].end_time = time.perf_counter()
            self.result_caching[req_id].req_latency = \
                (self.result_caching[req_id].end_time - self.result_caching[req_id].start_time) * 1000
            with self.lock:
                self.results.add_returned_req()
                if self.results.returned_req % 10 == 0:
                    self.logger.info("Total %d requests are sent to server, %d returned, %d empty.",
                                    self.results.total_req,
                                    self.results.returned_req, self.results.empty_req)
                if not self.result_caching[req_id].num_generated_tokens:
                    self.logger.error("[MIE01E000009] Request with data id %s encounters empty response!",
                                    data_dict.get("id", "<Unknown-id>"))
                    self.result_caching.pop(req_id)
                    self.results.add_empty_req()
                    return ""
                self.results.add_success_req()
                self.save_req_result_data(req_id)
        except ValueError as value_error:
            raise ValueError("Value error") from value_error 
        except Exception as e:
            self.logger.error("[MIE01E000008] Request %s occurs error with message %s. Invalid response format, "
                              "please check the message in mindie-client.", req_id, e)
            with self.lock:
                self.result_caching.pop(req_id, None)
            return ""
        self.result_caching[req_id].success = True
        return self.result_caching[req_id].output

    def run_sync_task(self, data_queue):
        """Run inference task synchronously.

        This is the entrance of run inference task, it will choose specific handler according to
        the client type.

        Args:
            data_queue (Queue): Data queue, a `get` method will be used to pop the front data in queue.
        """
        data_handler = self.data_handler
        self._delay_request(data_handler, data_queue)
        self.logger.info("Handling %d requests, failed %d",
                         self.results.total_req, (self.results.total_req - self.results.success_req))
        self.save_results()
        # to sync results to Results
        self.report_results()

    def save_results(self):
        pass

    def save_req_result_data(self, req_id: str):
        self.req_result_data[self.result_caching[req_id].data_id] = {
            "input_len": self.result_caching[req_id].num_input_tokens,
            "output_len": self.result_caching[req_id].num_generated_tokens,
            "prefill_bsz": self.result_caching[req_id].prefill_batch_size,
            "decode_bsz": self.result_caching[req_id].decode_batch_size,
            "req_latency": self.result_caching[req_id].req_latency,
            "latency": [self.result_caching[req_id].prefill_latency] + self.result_caching[req_id].decode_cost,
            "queue_latency": self.result_caching[req_id].queue_wait_time,
            "input_data": self.result_caching[req_id].input_data,
            "output": self.result_caching[req_id].output
        }

    def warm_up(self, warm_up_size: int):
        self.logger.info("Warm up start...")
        question = ("Claire makes a 3 egg omelet every morning for breakfast.  "
                    "How many dozens of eggs will she eat in 4 weeks?")
        if not self._config.get_command_line_arg("Tokenizer"):
            question = numpy.array(BenchmarkTokenizer.encode(question), dtype=numpy.uint32)
            if question.ndim == 1:
                question = numpy.expand_dims(question, axis=0)
        data_dict = {"id": "warm_up", "data": question, "options": []}
        count = 0
        warm_start = time.perf_counter()
        with ThreadPoolExecutor(max_workers=warm_up_size) as executor:
            for _ in range(warm_up_size):
                new_data_dict = data_dict.copy()
                new_data_dict["id"] = f"warm_up_{count}_{uuid.uuid4().hex}"
                executor.submit(self._warm_up_func, new_data_dict)
                count += 1
        time_elasped = time.perf_counter() - warm_start
        self.logger.info("Warm up finished. Handling %d requests, cost warm up time %.3f seconds",
                         warm_up_size, time_elasped)

    def _warm_up_func(self, data_dict):
        req_id, prompt = data_dict["id"], data_dict["data"]
        try:
            if self.is_stream:
                for _ in self.client_instance.request(
                    model_name=self.model_name,
                    inputs=prompt,
                    parameters=self.get_inference_parameters(),
                    request_id=req_id,
                ):
                    continue
            else:
                self.client_instance.request(
                    model_name=self.model_name,
                    inputs=prompt,
                    parameters=self.get_inference_parameters(),
                    request_id=req_id,
                )

        except MindIEClientException as e:
            self.logger.error("[MIE01E000108] Warm up request failed, request %s occurs error with message %s.",
                              req_id, e.get_message())
            return
        except Exception as e:
            self.logger.error("[MIE01E000108] Warm up request failed, request %s occurs error with message %s. "
                              "Please check the response in mindieclient",
                              req_id, str(e))
            return

    def _delay_request(self, data_handler, data_queue):
        """Send request to server using a thread pool to limit the number of concurrent threads.

        It will delay only when `--RequestRate` from command line args is given.

        Args:
            data_handler (function): A function to handle data.
            data_queue (Queue): A queue that contains request data; use `get` to pop the front element.
        """
        def get_request_type_and_rate():
            request_send_rate = self._config.command_line_args.get("RequestRate")
            if not request_send_rate:
                return None, None
            rate = request_send_rate[self.results.result_id]
            if not rate:
                raise ValueError("Request Rate must be > 0, failed to send request.")
            request_send_type = self._config.command_line_args.get("Distribution")
            if request_send_type not in ['uniform', 'poisson']:
                raise ValueError("Distribution must be uniform or poisson, failed to send request.")
            return request_send_type, rate

        def multi_rounds_handler(data_handler, data_dict: dict):
            hist_arr = []
            data_str = 'data'
            id_str = 'id'
            for i in range(len(data_dict[data_str])):
                turn_id = str(data_dict[id_str]) + '_' + str(i)
                hist_arr.append(data_dict[data_str][i])
                input_data = {
                    'id': turn_id,
                    'data': ''.join(hist_arr),
                    'options': []
                }
                hist_arr.append(data_handler(input_data))

        try:
            request_send_type, global_rate = get_request_type_and_rate()
            rate = None if not global_rate else global_rate / self.workersnum
        except ValueError as e:
            self.logger.error(e)
            return
        except Exception as exception:
            self.logger.error(exception)
            return
        dataset_name, _ = extract_dataset_type_info(self.dataset_type)
        request_num = int(self._config.command_line_args.get("RequestNum"))
        request_count = 0
        
        with ThreadPoolExecutor(max_workers=self.pool_size) as executor:
            data_dict = data_queue.get()
            while data_dict is not None:
                if dataset_name == 'mtbench':
                    executor.submit(multi_rounds_handler, data_handler, data_dict)
                else:
                    executor.submit(data_handler, data_dict)
                data_dict = data_queue.get()
                request_count += 1
                if request_count != request_num:
                    continue
                else:
                    request_count = 0
                if request_send_type == 'uniform':
                    time.sleep(numpy.random.uniform(0, 1.0 / rate * 2 * request_num))
                elif request_send_type == 'poisson':
                    time.sleep(numpy.random.exponential(1.0 / rate * request_num))
            data_queue.put(None)

