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

import time
import uuid
from collections import defaultdict
from dataclasses import dataclass, field
import threading
import queue
from functools import partial
from concurrent.futures import ThreadPoolExecutor, wait, ALL_COMPLETED
from urllib.parse import urlparse

import numpy
import tritonclient.grpc as grpcclient
import transformers
from transformers import AutoTokenizer

from mindiebenchmark.common.datasetloaders.base_loader import BaseLoader
from mindiebenchmark.common.results import ResultData
from mindiebenchmark.common.config import Config
from mindiebenchmark.common.logging import Logger
from mindiebenchmark.common.tokenizer import BenchmarkTokenizer


class UserData:
    def __init__(self):
        self.completed_requests = queue.Queue()


def callback(user_data, result, error):
    user_data.completed_requests.put((result, error))


class InferRequest:
    """Encapsulate input data into `ModelClient` requests.
    """
    def __init__(self, request_id, question, tokenizer, sampling_params, max_iter_times=4096,
                 model_name='ibis_benchmark'):
        """Initialize.

        Args:
            request_id: Id of a specific request.
            question (str): Inputted text for inference.
            tokenizer: Tokenizer that convert text to token-ids or in reverse.
            sampling_params (dict): Sampling parameters.
            max_iter_times (int, optional): Maximum iteration times, also means the maximum output
                length. Defaults to 4096.
            model_name (str, optional): The inference model name. Defaults to 'ibis_benchmark'.
        """
        self.submit_time = 0
        self.request_id = request_id
        self.question = question
        self.input_lens = len(question)

        token = tokenizer([question], return_tensors="np")
        token = token["input_ids"].astype(numpy.int64)
        self.input_token_lens = token.size
        input_tensor = token.reshape(1, -1)
        temperature = numpy.array([[sampling_params['temperature']]], numpy.float32)
        top_k = numpy.array([[sampling_params['top_k']]], numpy.int32)
        top_p = numpy.array([[sampling_params['top_p']]], numpy.float32)
        typical_p = numpy.array([[sampling_params['typical_p']]], numpy.float32)
        do_sample = numpy.array([[sampling_params['do_sample']]], bool)
        random_seed = numpy.array([[sampling_params['seed']]], numpy.uint64)
        repetition_penalty = numpy.array([[sampling_params['repetition_penalty']]], numpy.float32)
        frequency_penalty = numpy.array([[sampling_params['frequency_penalty']]], numpy.float32)
        presence_penalty = numpy.array([[sampling_params['presence_penalty']]], numpy.float32)
        watermark = numpy.array([[sampling_params['watermark']]], bool)

        typefp32 = "FP32"
        self.inputs = []
        self.inputs.append(grpcclient.InferInput('INPUT_IDS', list(input_tensor.shape), "INT64"))
        self.inputs.append(grpcclient.InferInput('TEMPERATURE', list(temperature.shape), typefp32))
        self.inputs.append(grpcclient.InferInput('TOP_K', list(top_k.shape), "INT32"))
        self.inputs.append(grpcclient.InferInput('TOP_P', list(top_p.shape), typefp32))
        self.inputs.append(grpcclient.InferInput('TYPICAL_P', list(typical_p.shape), typefp32))
        self.inputs.append(grpcclient.InferInput('DO_SAMPLE', list(do_sample.shape), "BOOL"))
        self.inputs.append(grpcclient.InferInput('SEED', list(random_seed.shape), "UINT64"))
        self.inputs.append(grpcclient.InferInput('REPETITION_PENALTY', list(repetition_penalty.shape), typefp32))
        self.inputs.append(grpcclient.InferInput('FREQUENCY_PENALTY', list(frequency_penalty.shape), typefp32))
        self.inputs.append(grpcclient.InferInput('PRESENCE_PENALTY', list(presence_penalty.shape), typefp32))
        self.inputs.append(grpcclient.InferInput('WATERMARK', list(watermark.shape), "BOOL"))

        self.inputs[0].set_data_from_numpy(input_tensor)
        self.inputs[1].set_data_from_numpy(temperature)
        self.inputs[2].set_data_from_numpy(top_k)
        self.inputs[3].set_data_from_numpy(top_p)
        self.inputs[4].set_data_from_numpy(typical_p)
        self.inputs[5].set_data_from_numpy(do_sample)
        self.inputs[6].set_data_from_numpy(random_seed)
        self.inputs[7].set_data_from_numpy(repetition_penalty)
        self.inputs[8].set_data_from_numpy(frequency_penalty)
        self.inputs[9].set_data_from_numpy(presence_penalty)
        self.inputs[10].set_data_from_numpy(watermark)

        self.outputs = []
        self.outputs.append(grpcclient.InferRequestedOutput('OUTPUT_IDS'))
        self.outputs.append(grpcclient.InferRequestedOutput('IBIS_EOS_ATTR'))

        self.max_iter_times = max_iter_times
        self.model_name = model_name


class Metrics:
    def __init__(self):
        self.first_token_time = 0
        self.avg_decode_time = 0
        self.max_decode_time = 0
        self.full_time = 0
        self.prompt_lens = 0
        self.prompt_token_lens = 0
        self.generated_tokens = 0
        self.decode_time_list = []
        self.client_id = -1
        self.output_str = ""
        self.generated_chars = 0


class ModelClient:
    """This class uses triton asynchronous api to finish inference task.
    """
    def __init__(self, tokenizer, client_id=0, server_url="localhost:8111"):
        """Initialize.

        Args:
            tokenizer (Any): Tokenizer that convert text to token-ids or in reverse.
            client_id (int, optional): The identifier of client. Defaults to 0.
            server_url (str, optional): Server url that provide service. Defaults to "localhost:8111".
        """
        self.server_url = server_url
        self.client_id = client_id
        self.tokenizer = tokenizer
        self.triton_client = grpcclient.InferenceServerClient(url=server_url, verbose=False)
        self.user_data = UserData()
        self.logger = Logger().logger
        self.triton_client.start_stream(callback=partial(callback, self.user_data))

    def infer(self, request: InferRequest):
        metrics = Metrics()
        metrics.client_id = self.client_id
        metrics.prompt_lens = request.input_lens
        metrics.prompt_token_lens = request.input_token_lens
        begin_time = time.perf_counter()
        try:
            self.logger.info("Send a request, request id : {}".format(request.request_id))
            self.triton_client.async_stream_infer(model_name=request.model_name,
                                                  inputs=request.inputs,
                                                  request_id=request.request_id,
                                                  outputs=request.outputs,
                                                  priority=1)
        except RuntimeError as run_error:
            raise RuntimeError("Run async_stream_infer error") from run_error
        except Exception as e:
            raise e
        tokens = []
        output = ""
        last_token_time = time.perf_counter()
        decode_full_time = 0
        for _ in range(request.max_iter_times):
            (output_tensor, error) = self.user_data.completed_requests.get()
            if error is not None:
                self.logger.error(f'[MIE01E000409] Triton call back failed! Please check the grpc address. {error}')
                break
            gen_tokens = output_tensor.as_numpy('OUTPUT_IDS')
            for token in gen_tokens:
                tokens.append(token)
            if metrics.first_token_time == 0:
                metrics.first_token_time = time.perf_counter() - last_token_time
            else:
                decode_time = time.perf_counter() - last_token_time
                metrics.decode_time_list.append(decode_time)
                gen_token_num = output_tensor.as_numpy('IBIS_EOS_ATTR')[1]
                single_decode_time = decode_time / gen_token_num
                decode_full_time += decode_time
                if decode_time > metrics.max_decode_time:
                    metrics.max_decode_time = single_decode_time

            last_token_time = time.perf_counter()

            inferparam = output_tensor.get_response().parameters['triton_final_response']
            if inferparam.bool_param:
                self.logger.debug("recieve a eos token, request id : {}".format(request.request_id))
                output = self.tokenizer.decode(tokens)
                output += "<EOS>"
                break
        metrics.full_time = time.perf_counter() - begin_time
        metrics.generated_tokens = len(tokens)
        metrics.output_str = output
        metrics.generated_chars = len(output)
        if metrics.generated_tokens > 1:
            metrics.avg_decode_time = decode_full_time / (metrics.generated_tokens - 1)
        self.logger.info("return from client infer, request id : {}".format(request.request_id))
        return True, metrics


class TritonClient():
    """Triton client.

    Triton client sends inference requests to triton server through the inference interface of
    `ModelClient` in the process pool. Then obtain inference results and performance data.
    """
    @dataclass
    class MiddleResult:
        data_id: str = ''
        input_data: str = ''
        num_input_tokens: int = 0
        num_input_chars: int = 0
        num_generated_tokens: int = 0
        num_generated_chars: int = 0
        prefill_latency: float = .0
        decode_cost: list[float] = field(default_factory=list)
        req_latency: float = .0
        decode_batch_size: list[int] = field(default_factory=list)
        prefill_batch_size: int = 0
        post_process_time: float = .0
        tokenized_time: float = .0
        detokenized_time: float = .0
        queue_wait_time: list[float] = field(default_factory=list)
        data_option: list = field(default_factory=list)
        output: str = ""
        start_time: float = .0
        end_time: float = .0
        request_id: str = ''

        def is_valid(self):
            """To ensure valid results"""
            return all([self.num_input_tokens, self.num_input_chars,
                        self.num_generated_tokens, self.num_generated_chars, self.decode_cost])

        def convert_to_standard(self) -> dict:
            try:
                chars = self.num_generated_chars / self.num_generated_tokens
            except ZeroDivisionError:
                chars = 0.0
            except Exception as exception:
                raise Exception("Div failed") from exception
            return {
                "id": self.data_id,
                "input_data": self.input_data,
                'prefill_latencies': self.prefill_latency,
                'decode_token_latencies': self.decode_cost[:],
                'decode_max_token_latencies': max(self.decode_cost) if self.decode_cost else .0,
                'seq_latencies': self.req_latency,
                'input_tokens_len': self.num_input_tokens,
                'generate_tokens_len': self.num_generated_tokens,
                'generate_characters_len': self.num_generated_chars,
                'prefill_batch_size': self.prefill_batch_size,
                'decode_batch_size': self.decode_batch_size[:],
                'queue_wait_time': self.queue_wait_time[:],
                'post_processing_time': self.post_process_time,
                'tokenizer_time': self.tokenized_time,
                'detokenizer_time': self.detokenized_time,
                'output': self.output,
                'characters_per_token': chars,
                'request_id': self.request_id,
            }

    def __init__(self, client_config: Config, result_container: ResultData, dataloader: BaseLoader):
        """Initialize triton client.

        Args:
            client_config (Config): Configuration for client instance.
            result_container (ResultData): It stores the generated information during inference to
                calculate the metrics.
            dataloader (BaseLoader): The dataloader of benchmark.

        Raises:
            RecursionError: If `AutoTokenizer` load tokenizer failed, raise recursion error.
        """
        self.logger = Logger().logger
        self.config = client_config
        self.concurrency = self.config.command_line_args.get("Concurrency")
        self.model_name = self.config.command_line_args.get("ModelName")
        self.model_path = self.config.command_line_args.get("ModelPath")
        httplocate = self.config.command_line_args.get("Http")
        trust_remote_code = self.config.command_line_args.get("TrustRemoteCode")
        self.url = str(urlparse(httplocate).netloc)
        try:
            self.tokenizer = AutoTokenizer.from_pretrained(
                self.model_path, trust_remote_code=trust_remote_code, use_fast=True, local_files_only=True
            )
        except RecursionError as e:
            raise RecursionError(
                f"Failed to load the Tokenizer using AutoTokenizer. The current transformers "
                f"version is {transformers.__version__}. Please visit the HuggingFace official "
                "website to update to the latest weights and Tokenizer."
            ) from e
        except Exception as exception:
            raise Exception("Please check your tokenizer") from exception

        def new_middle():
            return TritonClient.MiddleResult()

        self.result_caching = defaultdict(new_middle)
        self.sample_params = self.get_sampling_params()
        # benchmark统计结果存储结构
        self.results = result_container
        self.dataloader = dataloader
        # InferRequest队列
        self.requests = queue.Queue()
        self.lock = threading.Lock()
        self.exception_queue = queue.Queue()


    def prepare_input(self, data_queue):
        # prepare input data
        default_sampling_params = {
            'temperature': 1.0,
            'top_k': 0,
            'top_p': 1.0,
            'typical_p': 1.0,
            'do_sample': 0,
            'seed': 0,
            'repetition_penalty': 1.0,
            'frequency_penalty': 0,
            'presence_penalty': 0,
            'watermark': 0
        }
        default_sampling_params.update(self.sample_params)
        data_dict = data_queue.get()
        while data_dict is not None:

            if all(key in data_dict for key in ("data", "id")):
                data = data_dict.get("data")
                data_id = data_dict.get("id")
            else:
                raise RuntimeError(f"Input data must have `id` and `data` key but got {data_dict.keys()}")

            request_idx = uuid.uuid4().hex
            self.result_caching[request_idx].request_id = request_idx

            self.requests.put(InferRequest(str(request_idx), data, sampling_params=default_sampling_params,
                                           tokenizer=self.tokenizer, model_name=self.model_name))

            self.result_caching[request_idx].data_id = data_id
            self.result_caching[request_idx].input_data = data

            if isinstance(data, list):
                data = numpy.array(data, dtype=numpy.uint32)
                if data.ndim == 1:
                    data = numpy.expand_dims(data, axis=0)
                num_tokens = data.shape[-1]
                self.result_caching[request_idx].num_input_tokens = num_tokens
                self.result_caching[request_idx].num_input_chars = num_tokens
            elif isinstance(data, str):
                if self.dataloader is None:
                    self.result_caching[request_idx].num_input_tokens = len(data)
                else:
                    encode_out = BenchmarkTokenizer.encode(data)
                    self.result_caching[request_idx].num_input_tokens = len(encode_out)
                self.result_caching[request_idx].num_input_chars = len(data)
            elif isinstance(data, numpy.ndarray):
                num_tokens = data.shape[-1]
                self.result_caching[request_idx].num_input_tokens = num_tokens
                self.result_caching[request_idx].num_input_chars = num_tokens

            try:
                data_dict = data_queue.get(timeout=2)
            except ValueError as value_error:
                raise ValueError("Please check timeout") from value_error
            except queue.Empty:
                break


    def get_sampling_params(self):
        do_sample = self.config.get_command_line_arg("DoSampling")
        param_cfg = self.config.get_command_line_arg("SamplingParams")
        if not param_cfg:
            param_cfg = {}
        if do_sample:
            self.logger.info("DoSampling is enabled.")
            param_cfg["do_sample"] = True
        return param_cfg


    def run_triton_sync_task(self, data_queue):
        """Run inference task synchronously.

        This is the entrance of run inference task, it will choose specific handler according to
        the client type.

        Args:
            data_queue (Queue): Data queue, a `get` method will be used to pop the front data in queue.
        """
        self.prepare_input(data_queue)
        # start threads
        all_task = []
        init = queue.Queue(self.concurrency)
        with ThreadPoolExecutor(self.concurrency, thread_name_prefix='submit') as p:
            for i in range(self.concurrency):
                all_task.append(p.submit(self.submit_requests, i, init))
            wait(all_task, return_when=ALL_COMPLETED)
        if not self.exception_queue.empty():
            raise self.exception_queue.get()

        # to sync results to Results
        self.report_results()


    def submit_requests(self, client_id, init):
        try:
            client = ModelClient(tokenizer=self.tokenizer,
                                 client_id=client_id, server_url=self.url)
            init.put(1)
            # wait all grpc client init ready
            while not init.full():
                time.sleep(0.1)
            while not self.requests.empty():
                with self.lock:
                    if not self.requests.empty():
                        request = self.requests.get()
                        self.requests.task_done()
                # add the count of requests that have been sent to server
                self.inc_total_req()
                self.result_caching[request.request_id].start_time = time.perf_counter()
                success, metrics = client.infer(request)
                self.result_caching[request.request_id].end_time = time.perf_counter()
                self.results.add_returned_req()

                if self.results.returned_req % 10 == 0:
                    self.logger.info("Total %d requests are sent to server, %d returned, %d empty.",
                                    self.results.total_req,
                                    self.results.returned_req, self.results.empty_req)
                if not success:
                    self.logger.error("[MIE01E000409] request {} submit failed! triton url: {}, "
                                      "model name: {}, inputs: {}".format(
                        request.request_id, client.server_url, request.model_name,
                        request.inputs))

                if metrics.first_token_time is None == 0:
                    self.result_caching.pop(request.request_id)
                    self.results.add_empty_req()
                    return

                self.result_caching[request.request_id].prefill_latency = metrics.first_token_time * 1000
                self.result_caching[request.request_id].req_latency = metrics.full_time * 1000
                self.result_caching[request.request_id].num_generated_tokens = metrics.generated_tokens
                self.result_caching[request.request_id].num_generated_chars = metrics.generated_chars
                self.result_caching[request.request_id].decode_cost = \
                                                [cost * 1000 for cost in metrics.decode_time_list]
                self.result_caching[request.request_id].output = metrics.output_str
                self.results.add_success_req()

            client.triton_client.stop_stream()
        except ValueError as value_error:
            raise ValueError("Value error") from value_error 
        except Exception as e:
            self.exception_queue.put(e)


    def report_results(self):
        """After finishing the task, convert the cached content to final result."""
        total_start = min([item.start_time for _, item in self.result_caching.items()])
        total_end = max([item.end_time for _, item in self.result_caching.items()])
        self.logger.info("Converting middle results to final results...")
        self.results.total_time_elapsed = total_end - total_start
        all_results = [result.convert_to_standard() for _, result in self.result_caching.items()]
        self.results.add_req_res(all_results)


    def inc_total_req(self):
        if self.results.total_req % 10 == 0:
            self.logger.info("Total %d requests are sent to server, %d returned, %d empty.",
                             self.results.total_req,
                             self.results.returned_req, self.results.empty_req)
        self.results.add_total_req()
