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
import queue
import time
import signal
import multiprocessing

import tqdm

from mindiebenchmark.common.tokenizer import BenchmarkTokenizer
from mindiebenchmark.common.datasetloaders import DatasetLoaderFactory
from mindiebenchmark.common.logging import Logger
from mindiebenchmark.common.results import ResultData
from mindiebenchmark.common.config import Config
from mindiebenchmark.inference.api import ClientFactory
from mindiebenchmark.inference.triton_client import TritonClient


WAITING_MASTER_NODE_FINISH = 10
MAX_DATA_QUEUE_SIZE = 1000
MAX_RESULT_QUEUE_SIZE = multiprocessing.cpu_count() + 1

    
def multi_client_run(data_que, res_queue, config: Config, concurrency: int):
    results = ResultData(config.command_line_args)
    temp_queue = queue.Queue(-1)
    loader = DatasetLoaderFactory.make_dataset_loader_instance(
            config.command_line_args, temp_queue, results)
    results.set_loader(loader)
    client = ClientFactory.make_client_instance(config, results, concurrency, loader)
    try:
        client.run_sync_task(data_que)
    except KeyboardInterrupt:
        client.graceful_exit()
    finally:
        if results.total_req > 0:
            res_dict = dict(total_time_elapsed=results.total_time_elapsed,
                            total_throughput=results.total_throughput,
                            total_tokens=results.total_tokens,
                            total_characters=results.total_characters,
                            success_req=results.success_req,
                            returned_req=results.returned_req,
                            empty_req=results.empty_req,
                            total_req=results.total_req,
                            correct_num=results.correct_num,
                            tokenizer_time=results.tokenizer_time,
                            detokenizer_time=results.detokenizer_time,
                            all_results=client.all_results,
                            req_result_data=client.req_result_data
                            )
            res_queue.put(res_dict)


class BaseBenchmark:
    def __init__(self, config: Config):
        self.logger = Logger().logger
        self.config = config
        self.test_type = self.config.get_command_line_arg("TestType")
        self.generate_stream = self.config.get_command_line_arg("GenerateStream")
        self.model_name = self.config.get_command_line_arg("ModelName")
        self.tokenizer = self.config.get_command_line_arg("Tokenizer")
        self.test_accuracy = self.config.get_command_line_arg("TestAccuracy")
        self.dataset_type = self.config.get_command_line_arg("DatasetType")
        self.concurrency = self.config.get_command_line_arg("Concurrency")
        self.task_kind = self.config.get_command_line_arg("TaskKind")
        self.results = ResultData(config.command_line_args)
        BenchmarkTokenizer.init(
            model_path=self.config.get_command_line_arg("ModelPath"),
            trust_remote_code=self.config.get_command_line_arg("TrustRemoteCode")
        )
        self.manager = multiprocessing.Manager()
        self.res_queue = self.manager.Queue(MAX_RESULT_QUEUE_SIZE)
        self.queue = queue.Queue(MAX_DATA_QUEUE_SIZE) if self.test_type == "engine"\
            else self.manager.Queue(MAX_DATA_QUEUE_SIZE)
        self.loader = DatasetLoaderFactory.make_dataset_loader_instance(
            self.config.command_line_args, self.queue, self.results)
        self.results.set_loader(self.loader)
        if self.test_type == "engine":
            self.loader.start()

    def run(self) -> bool:
        self.logger.info("Starting benchmark...")
        if self.test_type == "engine":
            self.run_engine()
            return True
        else:
            self.run_client()
            return True

    def run_engine(self):
        pass

    def run_client(self):
        pass


class GPTBenchmark(BaseBenchmark):
    def __init__(self, config: Config):
        super().__init__(config)
        self.infer_engine = None
        self.is_multi_nodes_infer = False
        self.is_master_ip = False
        self.work_num = self.config.get_command_line_arg("WorkersNum")
        self.warm_up_size = self.config.get_command_line_arg("WarmupSize")
        average_concurrency = self.concurrency // self.work_num
        final_concurrency = self.concurrency - average_concurrency * (self.work_num - 1)
        self.concurrencys = [average_concurrency for _ in range(self.work_num - 1)]
        self.concurrencys.append(final_concurrency)

        try:
            if self.config.get_command_line_arg("TestType") in ("engine", "client"):
                self.is_multi_nodes_infer = self.config.validate_multi_nodes_infer()

            if self.is_multi_nodes_infer:
                self.is_master_ip = self.config.validate_master_ip()
                self.logger.info("MULTI MACHINE is running")
                if self.is_master_ip:
                    self.logger.info("Multi machine is running in [master] machine node")
                else:
                    self.logger.info("Multi machine is running in [slave] machine node")
            else:
                self.logger.info("SINGLE MACHINE is running")
        except Exception as error:
            self.logger.error(f'[MIE01E000002] {str(error)}')
            self.loader.stop_load()
            self.loader.join()
            raise RuntimeError("Failed to initialize benchmark, please check benchmark settings") from error

    def run(self) -> bool:
        is_ok = super().run()
        self.results.save_eval_results()
        return is_ok

    def init_infer_engine(self):
        from mindiebenchmark.inference.async_inference_engine import InferenceEngine
        self.logger.info("Starting initializing benchmark[engine mode]")
        self.infer_engine = InferenceEngine(self.config, self.queue, self.loader, self.results)

    def infer_engine_finalize(self):
        self.logger.info("Starting finalizing benchmark[engine mode]")
        self.infer_engine.finalize()

    def run_engine(self):
        self.logger.info("Starting run engine inference")
        try:
            if self.is_master_ip or not self.is_multi_nodes_infer:
                self.infer_engine.async_forward()
            else:
                while True:
                    self.logger.info("Waiting for master machine to finish infer...")
                    time.sleep(WAITING_MASTER_NODE_FINISH)
        except RuntimeError as run_error:
            raise RuntimeError("Run async_forward error") from run_error
        except Exception as error:
            self.logger.error(f'[MIE01E000009] {str(error)}')
            self.loader.stop_load()
            self.loader.join()
            if self.infer_engine:
                self.infer_engine.stop_infer()

    def run_client(self):
        try:
            if self.test_type == "triton_client":
                self.logger.info("Starting run triton inference")
                client = TritonClient(self.config, result_container=self.results, dataloader=self.loader)
                client.run_triton_sync_task(self.queue)
            else:
                main_client = ClientFactory.make_client_instance(self.config,
                                                                 result_container=self.results,
                                                                 dataloader=self.loader
                                                                 )
                if self.warm_up_size > 0:
                    main_client.warm_up(self.warm_up_size)
                mg_dict = self.manager.dict()
                self.loader.link_mg_dict(mg_dict)
                data_process = multiprocessing.Process(target=self.loader.start, args=())
                data_process.start()
                self.logger.info("Starting run mindie-client inference")
                
                pool = multiprocessing.Pool(processes=self.work_num)  
                try:
                    for i in range(self.work_num):
                        pool.apply_async(
                            multi_client_run,
                            (self.queue, self.res_queue, self.config, self.concurrencys[i]),
                            error_callback=lambda x: self.logger.error(x)
                        )
                    pool.close()
                    pool.join()
                except KeyboardInterrupt:
                    self.logger.info("Main process catches KeyboardInterrupt and terminates the process Pool.")
                    pool.close()
                    pool.join()
                self.logger.info("Finish mindie-client inference")
                data_process.join()
                self.res_queue.put(None)
                self.results.update_gt_answer(dict(mg_dict))
                pbar = tqdm.tqdm(total=self.res_queue.qsize() - 1, 
                                 desc="Merging data from different processes", unit="item")
                while True:
                    res_data = self.res_queue.get()
                    if not res_data:
                        break
                    self.results.update(res_data)
                    pbar.update(1)
                pbar.close()
        except Exception as error:
            self.logger.error(f'[MIE01E000009] {str(error)}')
            self.loader.stop_load()
            self.loader.join()
            raise RuntimeError("Failed to run client") from error
        finally:
            self.manager.shutdown()