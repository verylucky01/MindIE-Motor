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

import argparse
import json
import os
import stat
import importlib.util
import re
import multiprocessing

from mindiebenchmark.common.datasetloaders import dataset_loader_map
from mindiebenchmark.common.datasetloaders.base_loader import extract_dataset_type_info
from mindiebenchmark.common.logging import Logger

from .file_utils import FileOperator, PathCheck

MAX_REQUEST_RATE = 10000
MAX_FILE_SIZE = (10 * 1024 * 1024)
BENCHMARK_PATH = importlib.util.find_spec("mindiebenchmark").origin
BASE_DIRECTORY = os.path.dirname(BENCHMARK_PATH)
WARMUP_SIZE_KEY = "WarmupSize"
CONCURRENCY_KEY = "Concurrency"
MAX_INPUT_LEN_KEY = "MaxInputLen"
MAX_OUTPUT_LEN_KEY = "MaxOutputLen"
REQUEST_TIME_OUT_KEY = "RequestTimeOut"
WOKR_NUM_KEY = "WorkersNum"
REQUEST_NUM_KEY = "RequestNum"
REQUEST_COUNT_KEY = "RequestCount"


class Config:
    config = {}
    command_line_args = {}
    default_config_path = os.path.join(BASE_DIRECTORY, "config", "config.json")
    default_synthetic_config_path = os.path.join(BASE_DIRECTORY, "config", "synthetic_config.json")
    logger = None

    @staticmethod
    def load_mies_config_path():
        # 获取 MIES 环境变量的值
        mies_install_path = os.path.realpath(os.getenv('MIES_INSTALL_PATH'))
        if not mies_install_path:
            raise ValueError("MIES environment variable is not set.")
        return os.path.join(mies_install_path, 'conf', 'config.json')

    @staticmethod
    def validate_master_ip():
        # 获取 RANK_TABLE_FILE 环境变量的值
        rank_table_file = os.getenv('RANK_TABLE_FILE')
        current_ip = os.getenv('MIES_CONTAINER_IP')
        master_ip = None
        ret, _ = PathCheck.check_path_full(rank_table_file)
        if not ret:
            raise ValueError("multi machine is running, but not set valid RANK_TABLE_FILE")

        file_size = os.path.getsize(rank_table_file)
        if file_size > MAX_FILE_SIZE:
            raise ValueError(f"File size of RANK_TABLE_FILE is {file_size/1024/1024:.4f} M, " \
                             f"which exceeds MAX_FILE_SIZE(10 M), see environment variable RANK_TABLE_FILE.")
        with open(rank_table_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            for server in data.get('server_list', []):
                for device in server.get('device', []):
                    if device.get('rank_id') == '0':
                        master_ip = server.get('server_id')
                        break
                if master_ip is not None:
                    break
        return master_ip == current_ip

    @staticmethod
    def _check_file_permission(config_path, expected_permissions, path_name):
        ret, infos = PathCheck.check_path_full(config_path)
        if not ret:
            raise ValueError(infos)
        real_path = os.path.realpath(config_path)
        try:
            # 获取指定文件的属主
            file_stat = os.stat(real_path)
            file_permissions = file_stat.st_mode & 0xFFF
            # 将权限位转换为人类可读的格式
            file_permissions_str = stat.filemode(file_permissions)
            expected_permissions_str = stat.filemode(expected_permissions)
            # 检查文件的权限位是否小于等于期望的权限位
            if file_permissions & (~expected_permissions & 0o777) != 0:
                raise ValueError(f"The file {real_path} does not meet the permission "
                                 f"requirement {expected_permissions_str}, but got {file_permissions_str}")
        except os.error as e:
            raise ValueError(f"Failed to check permission: `{path_name}`") from e

    @classmethod
    def parse_command_line_args(cls):
        def str_to_bool(v):
            if isinstance(v, bool):
                return v
            if v == "True":
                return True
            elif v == "False":
                return False
            else:
                raise argparse.ArgumentTypeError('Boolean value expected.')

        def check_str_len(input_str):
            if not isinstance(input_str, str):
                input_str = str(input_str)
            if len(input_str) > 1024:
                raise ValueError(f"The length of input string should be less than 1024, but got {len(input_str)}")
            return input_str

        # 解析命令行参数
        parser = argparse.ArgumentParser()
        parser.add_argument("--TestType", type=check_str_len,
                            choices=["engine", "client", "tgi_client", "vllm_client", "triton_client", "openai"],
                            help="TestType", default="client")
        parser.add_argument("--Http", type=check_str_len, help="Http URL", default="https://127.0.0.1:1025")
        parser.add_argument("--ModelPath", type=check_str_len, help="ModelPath", required=True)
        parser.add_argument("--ModelVersion", type=check_str_len, help="ModelVersion", default="")
        parser.add_argument("--ConfigPath", type=check_str_len, help="ConfigPath", default=cls.default_config_path)
        parser.add_argument("--DatasetPath", type=check_str_len, help="DatasetPath")
        parser.add_argument("--DatasetType", type=check_str_len, help="DatasetType", required=True)
        parser.add_argument("--Concurrency", type=int, help="Concurrency", default=128)
        parser.add_argument("--MaxInputLen", type=int, help="MaxInputLen", default=0)
        parser.add_argument("--MaxOutputLen", type=int, help="MaxOutputLen", default=20)
        parser.add_argument("--WorkersNum", type=int, help="WorkersNum", default=1)
        parser.add_argument("--ModelName", type=check_str_len, help="ModelName", required=True)
        parser.add_argument("--Tokenizer", type=str_to_bool, help="Tokenizer", default=True)
        parser.add_argument("--DoSampling", type=str_to_bool, help="DoSampling", default=False)
        parser.add_argument("--TestAccuracy", type=str_to_bool, help="TestAccuracy", default=False)
        parser.add_argument("--LoraDataMappingFile", type=check_str_len, help="LoraDataMappingFile", default="")
        parser.add_argument("--SavePath", type=check_str_len, help="SavePath", default="")
        parser.add_argument("--SaveTokensPath", type=check_str_len, help="SaveTokensPath", default="")
        parser.add_argument("--SamplingParams", type=check_str_len, help="SamplingParams", default="{}")
        parser.add_argument("--SaveHumanEvalOutputFile", type=check_str_len, help="SaveHumanEvalOutputFile",
                            default="humaneval_output.jsonl")
        parser.add_argument("--TaskKind", type=check_str_len,
                            help="task_kind", choices=["stream", "text", "stream_token"],
                            default="stream")
        parser.add_argument("--RequestRate", type=check_str_len, help="set request seed rate", default="")
        parser.add_argument("--Distribution", type=check_str_len,
                            choices=["uniform", "poisson"], help="set type by RequestRate",
                            default="uniform")
        parser.add_argument("--WarmupSize", type=int, help="Warm up size", default=10)
        parser.add_argument("--RequestTimeOut", type=check_str_len, help="RequestTimeOut", default="600")
        parser.add_argument("--ManagementHttp", type=check_str_len,
                            help="Management Http URL", default="https://127.0.0.2:1026")
        parser.add_argument("--SyntheticConfigPath", type=check_str_len, help="SyntheticConfigPath",
                            default=cls.default_synthetic_config_path)
        parser.add_argument("--TrustRemoteCode", type=str_to_bool, help="TrustRemoteCode", default=False)
        parser.add_argument("--RequestNum", type=int, default=1)
        parser.add_argument("--RequestCount", type=int, default=-1)

        try:
            args = parser.parse_args()
            cls.command_line_args = vars(args)
        except TypeError as type_error:
            raise TypeError("Input args format error") from type_error
        except Exception as e:
            parser.print_help()
            raise ValueError("Failed to parser command line ") from e

    @classmethod
    def load_config(cls, config_file):
        real_path = os.path.realpath(config_file)
        ret, infos = PathCheck.check_path_full(real_path)
        if not ret:
            raise ValueError(infos)
        expected_permissions = 0o640  # 定义配置文件权限位常量
        cls._check_file_permission(real_path, expected_permissions, 'ConfigPath')
        try:
            with open(real_path, mode="r", encoding="utf-8") as file:
                config_list = ["LogConfig", "CertConfig", "OutputConfig", "ServerConfig"]
                loaded_config = json.load(file)
                for config_type in config_list:
                    cls.config.update(loaded_config[config_type])
        except (FileNotFoundError, json.JSONDecodeError) as e:
            raise ValueError(f"Failed to load JSON config from `ConfigPath`") from e

    @classmethod
    def update_config(cls, config_file: str, key_value_pairs: dict):
        real_path = os.path.realpath(config_file)
        ret, infos = PathCheck.check_path_full(real_path)
        if not ret:
            raise ValueError(infos)
        expected_permissions = 0o640  # 定义配置文件权限位常量
        cls._check_file_permission(real_path, expected_permissions, 'ConfigPath')
        try:
            with open(real_path, mode="r+", encoding='utf-8') as file:
                data = json.load(file)
                for k, v in key_value_pairs.items():
                    data['LogConfig'][k] = v
                file.seek(0)
                file.truncate()
                json.dump(data, file, indent=4)
        except (FileNotFoundError, json.JSONDecodeError) as e:
            raise ValueError(f"Failed to load or write JSON config from config path of benchmark, \
                             please check File permissions and group affiliation") from e

    @classmethod
    def validate_config(cls):
        # 校验配置项
        # 在这里根据自己的需求实现校验逻辑
        # 如果配置项不符合要求，可以抛出异常或者给出警告信息
        # JSON配置文件校验（这里只是示例，你可以根据需要添加更多校验逻辑）
        log_config_list = [
            "LOG_LEVEL",
            "LOG_PATH",
            "LOG_TO_FILE",
            "LOG_TO_STDOUT",
            "LOG_VERBOSE",
            "LOG_ROTATE",
        ]
        for log_config_type in log_config_list:
            if log_config_type not in cls.config:
                raise ValueError(f'JSON config is missing {log_config_type}, please check')
        config_list = [
            "INSTANCE_PATH",
            "ENABLE_MANAGEMENT",
            "CA_CERT",
            "KEY_FILE",
            "CERT_FILE",
            "CRL_FILE",
        ]
        for config_type in config_list:
            if config_type not in cls.config or cls.config.get(config_type) == "":
                raise ValueError(f'JSON config is missing {config_type} or {config_type} is empty, please check')

    def add_logger(self):
        self.logger = Logger().logger

    def validate_multi_nodes_infer(self):
        mies_config_path = self.load_mies_config_path()
        PathCheck.check_path_full(mies_config_path)
        with open(mies_config_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
            return data['BackendConfig']['multiNodesInferEnabled']

    def validate_dataset_args(self):
        dataset_type = self.command_line_args.get("DatasetType")
        dataset_name, _ = extract_dataset_type_info(dataset_type)
        if dataset_name not in dataset_loader_map.keys():
            raise ValueError(f"argument --DatasetType: invalid choice: {dataset_name} choose from "
                             f"{list(dataset_loader_map.keys())}")
        dataset_loader_map.get(dataset_name).validate_dataset_args(dataset_type, self.command_line_args)

    def validate_sampling_parameters(self):
        # 转换sampling参数
        sampling_str = "SamplingParams"
        try:
            sampling_params = self.command_line_args.get(sampling_str)
            self.command_line_args[sampling_str] = json.loads(sampling_params)
        except json.JSONDecodeError as e:
            raise ValueError("The value of `SamplingParams` is not a valid json string.") from e

    def validate_conflict_args(self):
        dataset_type = self.command_line_args.get("DatasetType")
        test_type = self.command_line_args.get("TestType")

        if dataset_type == "synthetic" and test_type not in ("vllm_client", "engine", "openai"):
            raise ValueError("WARNING: When DatasetType is synthetic, TestType only support 'vllm_client', "
                             "'openai' or 'engine'.")

    def validate_command_line_args(self):
        # 校验命令行参数
        # 在这里根据自己的需求实现校验逻辑
        # 如果参数不符合要求，可以抛出异常或者给出警告信息
        # 整数范围校验
        if not 1 <= len(self.command_line_args) <= 100:
            raise ValueError(f"Integer param1 should be between 1 and 100, but got {len(self.command_line_args)}")

        # 路径校验（仅支持Unix路径）
        self._check_args_path()

        # 参数区间范围值校验
        self._check_args_range()

        # 参数校验, 规定输入为token id的文件时，不支持精度测试
        if not self.command_line_args.get("Tokenizer") and self.command_line_args.get("TestAccuracy"):
            raise ValueError("Could not test accuracy when reading token_id file!")

        # 参数校验，在TaskKind为stream_token时，不支持Tokenizer为True
        if self.command_line_args.get("TaskKind") == "stream_token" and self.command_line_args.get("Tokenizer"):
            raise ValueError("When TaskKind is 'stream_token', only support set Tokenizer as False.")

        # 防止csv注入攻击
        model_name = self.command_line_args.get("ModelName")

        pattern = r"^[a-zA-Z_0-9.\-\\/]+$"
        if not isinstance(model_name, str) or not re.match(pattern, model_name):
            raise ValueError(f"Invalid model name: {model_name}. "
                             "Please use only letters, numbers, '_', '.', '-', '\\', or '/'!")

        # 校验采样参数
        self.validate_sampling_parameters()

        self.validate_dataset_args()

        # 校验不同参数是否冲突
        self.validate_conflict_args()

    def get_config(self, key):
        # 获取配置项
        illegal_chars = {'\n', '\f', '\r', '\b', '\t', '\v', '\u007F'}
        config = self.config.get(key)
        if isinstance(config, int) or (config is None):
            return config
        elif any(char in illegal_chars for char in config):
            raise ValueError("JSON config contains illegal characters.")
        return config

    def get_command_line_arg(self, key):
        # 获取命令行参数
        return self.command_line_args.get(key)

    def _check_args_path(self):
        # ConfigPath
        config_path = self.command_line_args.get("ConfigPath")
        ret, infos = PathCheck.check_path_full(config_path)
        if not ret:
            raise ValueError(f"Invalid parameter `ConfigPath`: {infos}")

        # DatasetPath
        if self.command_line_args.get("DatasetType") != "synthetic":
            # Skip check synthetic dataset path because it has no path.
            dataset_path = self.command_line_args.get("DatasetPath")
            ret, infos = PathCheck.check_path_full(dataset_path, max_file_size=1 * 1024 * 1024 * 1024)
            if not ret:
                raise ValueError(f"Invalid parameter `DatasetPath`: {infos}")

        # ModelPath
        model_path = self.command_line_args.get("ModelPath")
        ret, infos = PathCheck.check_path_full(model_path, is_size_check=False)
        if not ret:
            raise ValueError(f"Invalid parameter `ModelPath`: {infos}")

        # SaveTokensPath
        save_tokens_path = self.command_line_args.get("SaveTokensPath")
        ret, infos = PathCheck.check_path_link(save_tokens_path)
        if not ret:
            raise ValueError(f"Invalid parameter `SaveTokensPath`: {infos}")
        if save_tokens_path:
            ret = FileOperator.create_file(save_tokens_path)
            if not ret:
                raise ValueError(f"Invalid parameter `SaveTokensPath`, cannot create this file. {infos}")

        # SaveHumanEvalOutputFile
        save_human_eval_path = self.command_line_args.get("SaveHumanEvalOutputFile")
        if os.path.exists(save_human_eval_path):
            ret, infos = PathCheck.check_path_full(save_human_eval_path)
            if not ret:
                err_msg = f"Invalid parameter `SaveHumanEvalOutputFile`: {infos}"
                self.logger.error(err_msg)
                raise ValueError(err_msg)

        # SavePath should be a directory
        save_path_str = "SavePath"
        if not self.command_line_args.get(save_path_str):
            self.command_line_args[save_path_str] = self.get_config("INSTANCE_PATH")
        save_path = self.command_line_args.get(save_path_str)
        if os.path.exists(save_path):
            ret, infos = PathCheck.check_directory_path(save_path)
            if not ret:
                err_msg = f"Invalid parameter `SavePath`: {infos}"
                self.logger.error(err_msg)
                raise ValueError(err_msg)
        else:
            ret = FileOperator.create_dir(save_path, mode=0o750)
            if not ret:
                err_msg = f"Create directory of `SavePath` failed and the reason: {infos}"
                self.logger.error(err_msg)
                raise ValueError(err_msg)

        # SyntheticConfigPath
        if self.command_line_args.get("DatasetType") == "synthetic":
            synthetic_config_path = self.command_line_args.get("SyntheticConfigPath")
            expected_permission = 0o640
            ret, infos = PathCheck.check_path_full(synthetic_config_path)
            if not ret:
                raise ValueError(f"{infos} Got path: {synthetic_config_path}")
            self._check_file_permission(synthetic_config_path, expected_permission, 'SyntheticConfigPath')

    def _check_args_range(self):
        warm_up_size = self.command_line_args.get(WARMUP_SIZE_KEY)
        request_num = self.command_line_args.get(REQUEST_NUM_KEY)
        concurrency = self.command_line_args.get(CONCURRENCY_KEY)
        max_input_len = self.command_line_args.get(MAX_INPUT_LEN_KEY)
        max_output_len = self.command_line_args.get(MAX_OUTPUT_LEN_KEY)
        request_time_out = self.command_line_args.get(REQUEST_TIME_OUT_KEY)
        work_nums = self.command_line_args.get(WOKR_NUM_KEY)
        request_count = self.command_line_args.get(REQUEST_COUNT_KEY)
        max_work_num = multiprocessing.cpu_count()
        if work_nums > max_work_num or work_nums <= 0:
            raise ValueError(f'WorkersNum must be in [1, {max_work_num}], but get {work_nums}')
        if concurrency < 1 or concurrency > 60000:
            error_info = f"Concurrency must be in [1, 60000], but get {concurrency}"
            raise ValueError(error_info)
        if max_input_len < 0 or max_input_len > 2**20:
            error_info = f"MaxInputLen must be in [0, {2**20}], but get {max_input_len}"
            raise ValueError(error_info)
        if max_output_len < 1 or max_output_len > 2**20:
            error_info = f"MaxOutputLen must be in [1, {2**20}], but get {max_output_len}"
            raise ValueError(error_info)
        if warm_up_size < 0 or warm_up_size > 1000:
            error_info = f"WarmupSize must be in [0, 1000], but get {warm_up_size}"
            raise ValueError(error_info)

        #RequestNum Validation
        if request_num <= 0 or request_num > 1000:
            error_info = f"RequestNum must be in [1, 1000], but get {request_num}"
            raise ValueError(error_info)

        #RequestCount Validation
        if request_count == 0 or request_count < -1:
            error_info = f"RequestCount must be a positive integer or -1, but get {request_count}"
            raise ValueError(error_info)

        if request_time_out == "None":
            self.command_line_args.update({'RequestTimeOut': None})
        else:
            if not request_time_out.isdigit() or int(request_time_out) <= 0:
                error_info = f"RequestTimeOut must be a positive integer or 'None', but get {request_time_out}"
                raise ValueError(error_info)
            else:
                self.command_line_args.update({'RequestTimeOut': int(request_time_out)})

        # 发送频率校验
        request_rate = 'RequestRate'
        if self.command_line_args.get(request_rate) != "":
            request_send_rate = []
            request_rates = [rate.strip() for rate in self.command_line_args.get(request_rate).split(',')]
            for rate in request_rates:
                if len(str(rate)) >= 100:
                    error_info = f"RequestRate length too long. RequestRate length must less than 100."
                    raise ValueError(error_info)
                request_send_rate.append(float(rate))
            if not (all(0 < x <= MAX_REQUEST_RATE for x in request_send_rate)):
                error_info = f"RequestRate must be in (0, 10000], but get {request_send_rate}"
                raise ValueError(error_info)
            self.command_line_args.update({request_rate: request_send_rate})
