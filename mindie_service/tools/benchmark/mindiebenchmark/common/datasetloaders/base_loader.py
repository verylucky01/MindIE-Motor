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
import glob
import json
import os
import queue
import threading
from abc import abstractmethod, ABC
from pathlib import Path

from jsonlines import jsonlines

from mindiebenchmark.common.logging import Logger
from mindiebenchmark.common.results import ResultData
from mindiebenchmark.common.file_utils import PathCheck

DATASET_TYPE_SEP = "_"
DATA_FILE_SIZE_LIMITATION = 1 * 1024 * 1024 * 1024  # Limit the size of the dataset file to 1 GB
DATA_FILE_NUM_SIZE_LIMITATION = 30000  # 限制读取dataset文件数量为1000


def check_file_num(file_list: list):
    if len(file_list) >= DATA_FILE_NUM_SIZE_LIMITATION:
        raise ValueError(f"The nums of dataset file must less than {DATA_FILE_NUM_SIZE_LIMITATION}.")


def check_file(dataset_path: str):
    ret, infos = PathCheck.check_path_full(
        path=dataset_path, max_file_size=DATA_FILE_SIZE_LIMITATION, is_size_check=True
    )
    if not ret:
        raise ValueError(infos)


def extract_dataset_type_info(dataset_type: str):
    arr = dataset_type.split(DATASET_TYPE_SEP)
    return arr[0], arr[1:]


class BaseLoader(threading.Thread):
    JSONL_POSIX = "jsonl"
    JSON_POSIX = "json"
    CSV_POSIX = "csv"
    PARQUET_POSIX = "parquet"
    JPG_POSIX = "jpg"
    WAV_POSIX = "wav"

    def __init__(self, command_line_args: dict, data_queue: queue.Queue, result: ResultData):
        super().__init__()
        self.data_queue = copy.copy(data_queue)
        self.dataset_path = command_line_args.get("DatasetPath")
        self.dataset_type = command_line_args.get("DatasetType")
        self.synthetic_config_path = command_line_args.get("SyntheticConfigPath")
        self.model_path = command_line_args.get("ModelPath")
        self.model_name = command_line_args.get("ModelName")
        self.test_accuracy = command_line_args.get("TestAccuracy")
        self.test_type = command_line_args.get("TestType")
        self.lora_mapping_file = command_line_args.get("LoraDataMappingFile")
        self.request_count = command_line_args.get("RequestCount")
        self.logger = Logger().logger
        self.lora_support_type = ("tgi_client", "vllm_client", "openai")
        self.lora_adapter_dict = {}
        if self.lora_mapping_file:
            self.lora_adapter_dict = self.load_lora_adapter(self.test_type, self.lora_mapping_file)
        self.lora_models = set(self.lora_adapter_dict.values())

        self.gt_answer = result.gt_answer
        self.tokenizer_time = result.tokenizer_time
        self.detokenizer_time = result.detokenizer_time
        self.stop = False
        self.choices = ["A", "B", "C", "D"]
        self.id_count = 0

    @classmethod
    def validate_dataset_args(cls, dataset_type: str, command_line_args: dict):
        if DATASET_TYPE_SEP in dataset_type:
            raise ValueError(f"argument --DatasetType: {dataset_type} should not contain {DATASET_TYPE_SEP}")

    @abstractmethod
    def read_file(self, file_path: str):
        pass

    @abstractmethod
    def get_file_posix(self) -> str:
        pass

    def get_sub_dir_name(self) -> str:
        return ""
    
    def load_lora_adapter(self, test_type, lora_data_mapping_path):
        lora_adapter_dict = {}
        if test_type not in self.lora_support_type:
            self.logger.warning(f"Multi lora don't support TestType:{test_type},\
                                only {self.lora_support_type} support")
            self.logger.info(f"All prompts will infer use {self.model_name}")
            return lora_adapter_dict

        ret, infos = PathCheck.check_path_full(lora_data_mapping_path)
        if not ret:
            raise ValueError(f"Invalid lora mapping data in `LoraDataMappingFile`. {infos}")

        with os.fdopen(os.open(lora_data_mapping_path, os.O_RDONLY, 0o640), "r", encoding='utf-8') as f:
            for line in f:
                try:
                    data = json.loads(line)
                    lora_adapter_dict.update(data)
                except json.JSONDecodeError as e:
                    self.logger.warning(f"Skip invalid JSON line:{line}, error:{e}")
                except Exception as exception:
                    self.logger.warning("Invalid lora data mapping path")
        return lora_adapter_dict

    def get_file_list(self) -> list:
        if str("." + self.get_file_posix()) in self.dataset_path:
            return [self.dataset_path]
        dir_path = self.dataset_path
        sub_dir_name = self.get_sub_dir_name()
        if sub_dir_name:
            dir_path = os.path.join(dir_path, sub_dir_name)
        return glob.glob((Path(dir_path) / ("*." + self.get_file_posix())).as_posix())

    def link_mg_dict(self, mg_dict):
        self.gt_answer = mg_dict
        
    def stop_load(self):
        self.logger.info("Stopping loading dataset...")
        self.stop = True
        while not self.data_queue.empty():
            self.data_queue.get()
        self.data_queue.put(None)
        self.logger.info("Finished loading dataset")

    def run(self):
        try:
            self.logger.info("Reading dataset...")
            file_list = self.get_file_list()
            check_file_num(file_list)
            for file_path in file_list:
                check_file(file_path)
                if self.stop:
                    return
                self.read_file(file_path)
            self.data_queue.put(None)
            self.logger.info("Finished loading dataset...")
        except RuntimeError as run_error:
            raise RuntimeError("Check file error") from run_error
        except Exception as err:
            self.logger.error(f"Loading dataset failed, {err}")
            self.stop_load()
            raise {"DatasetLoader process failed"} from err

    def splice_ques_options(self, data_dict, with_ans=False):
        ques_key = "question"
        ques = data_dict[ques_key]
        options = [data_dict["A"], data_dict["B"], data_dict["C"], data_dict["D"]]
        comp_choice = ""
        for choice, op in zip(self.choices, options):
            comp_choice += f"{choice}. {op}\n"

        complete_ques = ques + "\n" + comp_choice

        if with_ans:
            complete_ques += "Answer: " + data_dict["answer"] + "\n"
        return complete_ques

    def put_data_dict(self, data):
        """将处理好的数据保存到data_queue

        :param data:如果是data_dict格式的字典，则取其id、data和options字段存储；
                    如果是单独的data，则id默认取self.id_count，options默认取[]
        """
        data_dict = {
            "id": str(self.id_count),
            "data": data,
            "options": [],
            "num_expect_generated_tokens": 20,  # [int] Only used in synthetic dataset.
            "model_id": self.lora_adapter_dict.get(str(self.id_count), 
                                                   self.model_name) # Only used in multi-lora evaluate
        }
        if isinstance(data, dict):
            for key, val in data_dict.items():
                data_dict[key] = data.get(key, val)
        self.data_queue.put(data_dict)
        self.id_count += 1
        if self.id_count >= self.request_count and self.request_count != -1:
            self.data_queue.put(None)
            self.stop = True


class JsonlLoader(BaseLoader, ABC):
    @abstractmethod
    def read_line(self, line):
        pass

    def get_file_posix(self) -> str:
        return self.JSONL_POSIX

    def read_file(self, file_path: str):
        with jsonlines.open(file_path) as reader:
            for line in reader:
                if self.stop:
                    return
                self.put_data_dict(self.read_line(line))


class MultiJsonLoader(BaseLoader, ABC):
    @abstractmethod
    def read_line(self, line):
        pass

    def get_file_posix(self) -> str:
        return self.JSON_POSIX

    def read_file(self, file_path: str):
        with open(file_path, mode="r", encoding="utf-8") as file:
            data = json.load(file)
            for line in data.items():
                if self.stop:
                    return
                self.put_data_dict(self.read_line(line))
