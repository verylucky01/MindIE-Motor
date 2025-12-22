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
import copy
import queue
import time
import threading
from typing import List, Optional, Tuple, Dict
from dataclasses import dataclass, field
from collections import defaultdict
import numpy as np

from mindiebenchmark.common.tokenizer import BenchmarkTokenizer
from mindiebenchmark.common.datasetloaders.base_loader import BaseLoader
from mindiebenchmark.common.logging import Logger
from mindiebenchmark.common.config import Config
from mindiebenchmark.common.results import request_result_template, ResultData
from mindiebenchmark.common.thread_utils import register_signal
from mindiebenchmark.common.file_utils import PathCheck
from mindiebenchmark.common.params_check import check_type_and_range

from llm_manager_python_api_demo.engine import Engine
from llm_manager_python_api_demo.request import Request
from llm_manager_python_api_demo.request_id import RequestId
from llm_manager_python_api_demo.data import Data
from llm_manager_python_api_demo.dtype import DType
from llm_manager_python_api_demo.sampling import SamplingParams


WARMUP_DATA_ID_PREFIX = "_warm_up_"
MAX_BATCH_SIZE = 5000      # max batch size allowed in LLM
g_infer_result = {}        # Store the inference result in format: {"data_id": engine_result_template}
engine_result_template = {
    "input_token_ids": [],
    "start_time": 0.0,
    "generate_token_ids": [],
    "response_time": [],
    "gen_seq_ids": [],
    "gen_logprobs": [],
    "num_expect_generated_tokens": None   # int or None, used only when dataset type is synthetic.
}

g_responses = []
g_use_beam_search = False
g_beam_search_width = 1
TOP_LOGPROBS_STR = "top_logprobs"


@dataclass
class BeamSearchResponse:
    cache_seq: defaultdict = field(default_factory=lambda: defaultdict(list))
    cache_seq_tokens: defaultdict = field(default_factory=lambda: defaultdict(list))
    finished_seq: defaultdict = field(default_factory=lambda: defaultdict(list))
    finished_seq_tokens: defaultdict = field(default_factory=lambda: defaultdict(list))
    finished_seq_id: int = 0

g_beam_search_response: defaultdict = defaultdict(BeamSearchResponse)


def update_beam_search_cache(req_id, current_cache_id, parent_id):
    global g_beam_search_width
    global g_beam_search_response

    for key in current_cache_id:
        if key not in parent_id:
            g_beam_search_response[req_id].cache_seq.pop(key)

    while len(g_beam_search_response[req_id].finished_seq) > g_beam_search_width:
        finished_seq = g_beam_search_response[req_id].finished_seq
        min_finished_seq = min(finished_seq, key=finished_seq.get)
        g_beam_search_response[req_id].finished_seq.pop(min_finished_seq)
        g_beam_search_response[req_id].finished_seq_tokens.pop(min_finished_seq)


class InferenceEngine:
    """A class to run mindie inference in engine model.

    This class uses the sdk api provided mindie-llm directly.
    """

    def __init__(self, config: Config, data_queue: queue.Queue, loader: BaseLoader, results: ResultData):
        """Initialize inference engine.

        Args:
            config (Config): Configuration from command line args and configuration file.
            data_queue (queue.Queue): A queue that contains the dataset, each item in queue will be
                pop left and convert to request.
            loader (BaseLoader): The dataloader, it's used to stop loading data and get some
                properties of dataloader, instead of getting data from dataloader.
            results (ResultData): It stores the generated information during inference to calculate
                the metrics.
        """
        register_signal()
        self.logger = Logger().logger
        self.data_queue = data_queue
        self.loader = loader
        self.results = results
        self.stop = False
        self.test_type = config.get_command_line_arg("TestType")
        self.sample_params = config.get_command_line_arg("SamplingParams")
        self.validate_sample_parameters()
        self.do_sampling = self.sample_params.get("do_sample", config.get_command_line_arg("DoSampling"))
        self.max_output_len = self.sample_params.get("max_new_tokens", config.get_command_line_arg("MaxOutputLen"))
        self.max_input_len = config.get_command_line_arg("MaxInputLen")
        self.tokenizer = config.get_command_line_arg("Tokenizer")
        self.warmup_size = config.get_command_line_arg("WarmupSize")
        self.warmup_returned = 0
        self.failed_req = 0
        self.first_request_time = None
        self.last_response_time = None
        self.response_timeout_threshold = 120     # Timeout to wait LLM response (unit: second)

        self.data_mutex = threading.Lock()
        self.metrics_mutex = threading.Lock()
        self.count_mutex = threading.Lock()

        self.engine_config_path = self.get_engine_config_path()
        self.engine = Engine()
        self.init_engine()
        self.max_batch_size = self.get_max_batch_size()
        self.ignore_eos = self.sample_params.get("ignore_eos", None)
        if self.ignore_eos is None:
            self.ignore_eos = (self.loader.dataset_type == "synthetic")
        
        if "n" in self.sample_params.keys():    # best_of >= n
            self.sample_params['best_of'] = self.sample_params['n']
        
        logprobs_false = "logprobs" in self.sample_params.keys() and not self.sample_params['logprobs']
        top_logprobs_set = TOP_LOGPROBS_STR in self.sample_params.keys() and self.sample_params[TOP_LOGPROBS_STR] > 0
        if logprobs_false and top_logprobs_set:
            self.logger.warning("Warning: top_logprobs must be 0 when logprobs is false!")
            self.sample_params[TOP_LOGPROBS_STR] = 0

    @staticmethod
    def token_ids_to_data(data_id: str, token_ids: List[int]) -> Data:
        """Convert token-id list to `Data` instance."""
        data = Data(data_id)
        shape = np.array([1, len(token_ids)])
        data.set_token_id(DType.TYPE_INT64, shape, np.array(token_ids, dtype=np.int64))
        return data

    @staticmethod
    def get_engine_config_path():
        mies_install_path = os.getenv("MIES_INSTALL_PATH")
        if not mies_install_path:
            raise ValueError("Environment variable 'MIES_INSTALL_PATH' is not set, " \
                             "make sure source the set_env.sh file.")
        ret, infos = PathCheck.check_path_full(mies_install_path)
        if not ret:
            raise ValueError(f"Check path of env MIES_INSTALL_PATH failed: {infos}")
        engine_config_path = os.path.join(mies_install_path, "conf/config.json")
        ret, infos = PathCheck.check_path_full(engine_config_path)
        if not ret:
            raise ValueError(f"Check config.json of MindIE-Service failed: {infos}")
        return engine_config_path

    def get_max_batch_size(self):
        if self.engine_config_path:
            self.get_engine_config_path()
        schuduler_config = self.engine.get_scheduler_config(self.engine_config_path)
        max_batch_size = schuduler_config.get("maxBatchSize", 200)   # default maxBatchSize is 200
        if max_batch_size < 1 or max_batch_size > MAX_BATCH_SIZE:
            err_msg = f"maxBatchSize in config.json of MindIE-Service should be in " \
                      f"range [1, {MAX_BATCH_SIZE}], but got {max_batch_size}."
            self.logger.error(f'[MIE01E000502] {err_msg}')
            raise ValueError(err_msg)
        return max_batch_size

    def response_callback(self, req_id, output, is_final: bool, err_msg: str):
        """The callback function that will be called after generating one token.

        To release the NPU resources in time, this callback should be as simple as possible.

        Args:
            req_id (llm_manager_python.InferRequestId): A unique descriptor of request.
            output (llm_manager_python.TensorMap): The generated data, it contains generated token.
            is_final (bool): If the token is the last token for one request.
            err_msg (str): Error messages.
        """
        global g_responses

        cur_time = time.time()
        g_responses.append((req_id, output, is_final, err_msg, cur_time))
        self.last_response_time = cur_time

        if is_final:
            req_id = self.engine.convert_request_id(req_id)
            if req_id.startswith(WARMUP_DATA_ID_PREFIX):
                self.logger.info(f"Warmup data returned, the data id is {req_id}")
            else:
                with self.count_mutex:
                    self.results.add_returned_req()
                if self.results.returned_req % 20 == 0 or self.results.returned_req == self.results.total_req:
                    self.logger.info(f"Request returned/total = {self.results.returned_req}/{self.results.total_req}")
                if self.results.returned_req == self.results.total_req:
                    self.logger.info("All request returned, it may take a long time to handle it.")

    def process_response(self):
        """Process the response cached in `g_responses`.

        This function will be called when all inference requests have finished.

        Raises:
            RuntimeError: If update the metrics failed, raise runtime error.
        """
        global g_responses
        global g_use_beam_search
        global g_beam_search_width
        global g_beam_search_responses
        g_use_beam_search = "use_beam_search" in self.sample_params.keys() and self.sample_params["use_beam_search"]
        g_beam_search_width = self.sample_params["n"] if "n" in self.sample_params.keys() else 1

        for i, (req_id, output, is_final, err_msg, t) in enumerate(g_responses):
            if (i + 1) % 10000 == 0 or (i + 1) == len(g_responses):
                self.logger.info(f"Finished tokens/all tokens: {i + 1}/{len(g_responses)}")
            req_id = self.engine.convert_request_id(req_id)
            resp = self.engine.construct_response_by_tensor_map(req_id, output, is_final, err_msg)
            data_id = resp.get_request_id()                     # resp_id == req_id == data_id
            if data_id.startswith(WARMUP_DATA_ID_PREFIX):
                continue
            if g_use_beam_search:
                cur_time = time.time()
                g_infer_result[data_id]['response_time'].append(cur_time)
                cumulative_logprobs = resp.get_cumulative_logprobs().tolist()
                seq_ids = resp.get_seq_id().tolist()
                output_ids = resp.get_output_id().tolist()
                parent_seq_ids = resp.get_parent_seq_id().tolist()
                current_cache_id = copy.deepcopy(g_beam_search_response[req_id].cache_seq)
                current_cache_tokens = copy.deepcopy(g_beam_search_response[req_id].cache_seq_tokens)
                for index, seq_id in enumerate(seq_ids):
                    parent_seq_id = parent_seq_ids[index]
                    parent_tokens = current_cache_tokens[parent_seq_id]
                    output_id = output_ids[index]
                    if seq_id == -1:
                        resp_finished_seq_id = g_beam_search_response[req_id].finished_seq_id
                        g_beam_search_response[req_id].finished_seq[resp_finished_seq_id] = cumulative_logprobs[index]
                        g_beam_search_response[req_id].finished_seq_tokens[resp_finished_seq_id].extend(parent_tokens)
                        g_beam_search_response[req_id].finished_seq_tokens[resp_finished_seq_id].extend(output_id)
                        g_beam_search_response[req_id].finished_seq_id += 1
                    else:
                        if seq_id not in current_cache_id.keys():
                        #if seq_id not in g_beam_search_response[req_id].cache_seq_tokens:
                            g_beam_search_response[req_id].cache_seq_tokens[seq_id].extend(parent_tokens)
                        g_beam_search_response[req_id].cache_seq_tokens[seq_id].extend(output_id)
                        g_beam_search_response[req_id].cache_seq[seq_ids[index]] = cumulative_logprobs[index]
            else:
                gen_token_ids = resp.get_output_id().tolist()
                gen_seq_ids = resp.get_seq_id().tolist()
                gen_logprobs = resp.get_top_logprobs().tolist()
                try:
                    self.update_metrics(data_id, gen_token_ids, action="response",
                                        gen_seq_res=[gen_seq_ids, gen_logprobs], cur_time=t)
                except Exception as e:
                    raise RuntimeError("Error in response callback") from e
        #select beam search top N
        for i in g_beam_search_response.keys():
            beam_search_response = g_beam_search_response[i]
            all_finished_seqs = beam_search_response.finished_seq
            selected_seqs = sorted(all_finished_seqs.items(),
                                    key=lambda x: x[1], reverse=True)[:g_beam_search_width]
            selected_seq_tokens = []
            for _, (selected_seq, _) in enumerate(selected_seqs):
                selected_seq_tokens.append(beam_search_response.finished_seq_tokens[selected_seq])
            g_beam_search_response[i] = selected_seq_tokens
            if g_beam_search_response[i]:
                self.results.add_success_req()
            else:
                self.results.add_empty_req()

        for _, result in g_infer_result.items():
            if result.get("generate_token_ids"):
                self.results.add_success_req()
            else:
                self.results.add_empty_req()

    def validate_sample_parameters(self):
        """Validate the sampling parameters input by user, which given by --SamplingParams."""
        int32_range = '(-2147483647, 2147483647]'
        int64_range = '(-18446744073709551615, 18446744073709551615]'
        support_sample_params = {
            "top_k": ((int, ), int32_range),
            "top_p": ((float, int), ''),
            "typical_p": ((float, int), ''),
            "temperature": ((float, int), ''),
            "seed": ((int, ), int64_range),
            "presence_penalty": ((float, int), ''),
            "frequency_penalty": ((float, int), ''),
            "repetition_penalty": ((float, int), ''),
            "ignore_eos": ((bool, ), ''),
            "do_sample": ((bool, ), ''),
            "watermark": ((bool, ), ''),
            "max_new_tokens": ((int, ), int32_range),
            "best_of": ((int, ), int32_range),
            "n": ((int, ), int32_range),
            "use_beam_search": ((bool, ), ''),
            "logprobs": ((bool, ), ''),
            "top_logprobs": ((int, ), int32_range),
        }
        for key, value in self.sample_params.items():
            if key not in support_sample_params:
                self.logger.warning(f"Parameter '{key}' is not support in --SamplingParams.")
            else:
                types, value_range = support_sample_params.get(key)
                check_type_and_range(key, value, types, value_range)
            if key == "do_sample":
                self.logger.warning("Parameter 'do_sample' in --SamplingParams will overwrite "
                                    "--DoSampling.")
            if key == "max_new_tokens":
                self.logger.warning("Parameter 'max_new_tokens' in --SamplingParams will overwrite "
                                    "--MaxOutputLen.")


    def get_sampling_params(self) -> SamplingParams:
        return SamplingParams(
            in_temperature=self.sample_params.get("temperature", 1.0),
            in_top_k=self.sample_params.get("top_k", 0),
            in_top_p=self.sample_params.get("top_p", 1.0),
            in_typical_p=self.sample_params.get("typical_p", 1.0),
            in_do_sample=self.do_sampling,
            in_seed=self.sample_params.get("seed", 1),
            in_repetition_penalty=self.sample_params.get("repetition_penalty", 1.0),
            in_watermark=self.sample_params.get("watermark", False),
            in_frequency_penalty=self.sample_params.get("frequency_penalty", 0.0),
            in_presence_penalty=self.sample_params.get("presence_penalty", 0.0),
            best_of=self.sample_params.get("best_of", 1),
            n=self.sample_params.get("n", 1),
            use_beam_search=self.sample_params.get("use_beam_search", False),
            logprobs=self.sample_params.get("logprobs", False),
            top_logprobs=self.sample_params.get("top_logprobs", 0),
        )

    def init_engine(self):
        """Initialize inference engine.

        It should be called once at the beginning of inference.
        """
        status = self.engine.init(self.engine_config_path, self.response_callback)
        if not status.is_ok():
            self.logger.error("[MIE01E000509] Infer engine init model failed, please check the service logs.")
            self.loader.stop_load()

    def prepare_input(self, raw_data: Dict) -> Data:
        """ Convert data reading from data queue to `Data` instance. """
        data_id = raw_data.get("id")
        sentence = raw_data.get("data")
        if data_id is None or sentence is None:
            raise ValueError(f"Wrong format data: {raw_data}")
        if self.loader.dataset_type == "synthetic":
            num_expect_generated_tokens = raw_data.get("num_expect_generated_tokens", self.max_output_len)
            self.update_metrics(
                data_id,
                token_ids=[],     # not used
                action="add_num_expect_generated_tokens",
                num_expect_generated_tokens=num_expect_generated_tokens
            )
            # For synthetic dataset, the ignore_eos is True in default.
            self.ignore_eos = self.sample_params.get("ignore_eos", True)

        token_ids = BenchmarkTokenizer.encode(sentence, self.results.tokenizer_time) if self.tokenizer else sentence
        if 0 < self.max_input_len < len(token_ids):
            token_ids = token_ids[0:self.max_input_len]
        data = self.token_ids_to_data(data_id, token_ids)
        return data

    def data_to_request(self, data: Data) -> Request:
        """Convert `Data` instance to request instance."""
        data_id = data.get_id()
        request = Request(RequestId(data_id))
        status = request.set_data_to_request(data)
        if not status.is_ok():
            raise ValueError(f"engine set data error : {status.get_msg()}")

        max_output_len = self.max_output_len
        if self.loader.dataset_type == "synthetic" and not data_id.startswith(WARMUP_DATA_ID_PREFIX):
            max_output_len = g_infer_result[data_id]["num_expect_generated_tokens"]
        request.set_max_output_len(max_output_len)

        request.set_sampling_params(self.get_sampling_params())
        request.set_ignore_eos(self.ignore_eos)
        return request

    def get_quotas(self) -> Tuple:
        """Get the quotas that can used as inference.

        The interface will called api provided by llm to get the quotas.

        Returns:
            remain_prefill_slots (int): The remain prefill slots in machine, it should be greater
                than 0 if a new request can be accepted.
            remain_prefill_tokens (int): The remain prefill tokens in machine, it should be greater
                than 0 if a new request can be accepted.
            slot_num (int): The number of slot that can be used in machine, it should be greater
                than 0 if a new request can be accepted.
        """
        _remain_blocks, remain_prefill_slots, remain_prefill_tokens = self.engine.get_request_block_quotas()
        processing_num = self.engine.get_processing_request()
        slot_num = self.max_batch_size - processing_num
        return (remain_prefill_slots, remain_prefill_tokens, slot_num)

    def get_input_data_by_quotas(self) -> List[Optional[Data]]:
        """Get input data list from data queue by quotas, make sure the number of data is reasonable."""
        remain_prefill_slots, remain_prefill_tokens, slot_num = self.get_quotas()
        data_list: List[Data] = []
        with self.data_mutex:
            while not self.data_queue.empty():
                raw_data = self.data_queue.queue[0]
                if raw_data is None:
                    self.data_queue.get()
                    break
                tmp_data = self.prepare_input(raw_data)
                demand_token_num = tmp_data.get_data_size()
                if remain_prefill_slots > 0 and remain_prefill_tokens >= demand_token_num and slot_num > 0:
                    data_list.append(tmp_data)
                    self.data_queue.get()
                    remain_prefill_slots -= 1
                    remain_prefill_tokens -= demand_token_num
                    slot_num -= 1
                else:
                    break
        return data_list

    def update_metrics(self, data_id: str, token_ids: List, action: str = "",
                       gen_seq_res=None, cur_time: Optional[float] = None,
                       num_expect_generated_tokens: Optional[int] = None):
        """Update the statistics information."""
        cur_time = cur_time if cur_time else time.time()
        if data_id not in g_infer_result:
            g_infer_result[data_id] = copy.deepcopy(engine_result_template)
        with self.metrics_mutex:
            if action == "remove":
                g_infer_result.pop(data_id, None)
            elif action == "request":
                g_infer_result[data_id]["input_token_ids"].extend(token_ids)
                g_infer_result[data_id]["start_time"] = cur_time
            elif action == "response":
                gen_seq_ids, gen_logprobs = gen_seq_res
                g_infer_result[data_id]["generate_token_ids"].append(token_ids)
                g_infer_result[data_id]["response_time"].append(cur_time)
                g_infer_result[data_id]["gen_seq_ids"].append(gen_seq_ids)
                g_infer_result[data_id]["gen_logprobs"].append(gen_logprobs)
            elif action == "add_num_expect_generated_tokens":
                g_infer_result[data_id]["num_expect_generated_tokens"] = num_expect_generated_tokens
            else:
                raise ValueError(f"action should be one of {{'remove', 'request', 'response', but got: {action}}}")

    def forward_one(self, request: Request):
        """Call the llm to infer once by given data.

        Args:
            request (Request): One request that contains prompt and sampling params etc.
        """
        status = self.engine.async_forward(request)
        if not status.is_ok():
            self.failed_req += 1
            req_id = request.get_request_id().get_id_value()
            self.update_metrics(req_id, token_ids=[], action="remove")
            self.logger.warning(f"[MIE01W000509] LLM forward failed one item, request id: {req_id}")

    def async_forward(self):
        """Infer asynchronously.

        This interface is the entrance of inference. It will finish the whole process of inference.
        """
        if self.warmup_size > 0:
            self.warmup()
        self.first_request_time = time.time()
        while not self.data_queue.empty():
            data_list = self.get_input_data_by_quotas()
            threads = []
            for data in data_list:
                request = self.data_to_request(data)
                self.update_metrics(data.get_id(), data.get_data(), action="request")
                t = threading.Thread(target=self.forward_one, args=(request,))
                t.start()
                threads.append(t)
                self.results.add_total_req()
                if self.results.total_req % 100 == 0:
                    self.logger.info(f"Send request num: {self.results.total_req}")
            for t in threads:
                t.join()
            time.sleep(0.01)
        self.logger.info(f"Send all request num: {self.results.total_req}")
        while (not self.is_all_request_finished()) and (not self.is_response_timeout()):
            time.sleep(0.01)
        self.process_response()
        self.process()

    def is_all_request_finished(self):
        return self.results.returned_req + self.failed_req >= self.results.total_req

    def is_response_timeout(self) -> bool:
        if self.first_request_time is None:
            return False
        now = time.time()
        if self.last_response_time is None:
            time_elapsed = now - self.first_request_time
        else:
            time_elapsed = now - self.last_response_time
        is_timeout = time_elapsed > self.response_timeout_threshold
        if is_timeout:
            self.logger.warning("[MIE01W000509] Wait LLM response timeout, stop inference in advance.")
        return is_timeout

    def process(self) -> Dict:
        """Handle the data returned from llm.
        """
        if self.results.engine_infer_time is None or self.last_response_time is None:
            return

        self.logger.info("Begin to calculate the metrics.")
        self.results.engine_infer_time = self.last_response_time - self.first_request_time  # second
        for data_id, infer_res in g_infer_result.items():
            gen_seq_token_ids = defaultdict(list)
            res = copy.deepcopy(request_result_template)
            try:
                input_token_ids = infer_res["input_token_ids"]
                if g_use_beam_search:
                    output_token_id = g_beam_search_response[data_id]
                    gen_token_ids = output_token_id[0]
                else:
                    gen_token_ids = infer_res["generate_token_ids"]
                    gen_seq_ids = infer_res['gen_seq_ids']
                    for tokens, seq_ids in zip(gen_token_ids, gen_seq_ids):
                        for token_id, seq_id in zip(tokens, seq_ids):
                            gen_seq_token_ids[str(seq_id)].append(token_id[0])
                    output_token_id = []
                    for _, value in gen_seq_token_ids.items():
                        output_token_id.append(value)
                gen_chars = BenchmarkTokenizer.decode(output_token_id[0])
                res["id"] = data_id
                res["input_data"] = input_token_ids
                res["input_tokens_len"] = len(input_token_ids)
                res["output"] = [BenchmarkTokenizer.decode(i) for i in output_token_id]
                res["output_token_id"] = output_token_id
                res['logprobs'] = infer_res['gen_logprobs']
                res["generate_tokens_len"] = len(gen_token_ids)

                start_time = infer_res["start_time"] * 1000                     # second -> millisecond
                response_time = np.array(infer_res["response_time"]) * 1000     # second -> millisecond
                res["seq_latencies"] = response_time[-1] - start_time
                res["prefill_latencies"] = response_time[0] - start_time
                if len(gen_token_ids) <= 1:
                    decode_token_latencies = []
                    decode_max_token_latencies = 0
                else:
                    decode_token_latencies = np.diff(response_time).tolist()
                    decode_max_token_latencies = max(decode_token_latencies)
                res["decode_token_latencies"] = decode_token_latencies
                res["decode_max_token_latencies"] = decode_max_token_latencies

                res["generate_characters_len"] = len(gen_chars)
                res["characters_per_token"] = len(gen_chars) / len(gen_token_ids)
                self.results.add_req_res([res], False)
            except ZeroDivisionError:
                self.logger.warning(f"[MIE01W000505] Generated empty content for data: {data_id}.")
            except Exception as e:
                self.logger.warning(f"[MIE01W000505] Failed to deal data: {data_id}, ignore it. Reason: {e}")

    def finalize(self):
        """Finalize the inference engine.

        It usually be called after all task finished and want to release the resources of NPU.
        """
        self.loader.stop_load()
        self.engine.finalize()

    def stop_infer(self):
        """Stop infer.

        The dataloader will be stopped, but the llm-backend is still alive and the NPU resources is
        occupied by current process.
        """
        self.stop = True
        self.loader.stop_load()

    def warmup(self):
        """Warmup before send data in data queue.

        It infers several times before truly send data in data queue, usually to get better
        inference performance.
        """
        for i in range(self.warmup_size):
            input_len = 32
            prompt = " ".join(["A"] * input_len)
            input_token_ids = BenchmarkTokenizer.encode(prompt)
            data_id = f"{WARMUP_DATA_ID_PREFIX}{i}"
            tmp_data = self.token_ids_to_data(data_id, input_token_ids)
            demand_token_num = tmp_data.get_data_size()
            while True:
                remain_prefill_slots, remain_prefill_tokens, slot_num = self.get_quotas()
                if remain_prefill_slots > 0 and remain_prefill_tokens >= demand_token_num and slot_num > 0:
                    request = self.data_to_request(tmp_data)
                    t = threading.Thread(target=self.forward_one, args=(request,))
                    t.start()
                    t.join()
                    break
                else:
                    time.sleep(0.02)
            if (i + 1) % 10 == 0 or (i + 1) == self.warmup_size:
                self.logger.info(f"Send warmup request num: {i + 1}")
