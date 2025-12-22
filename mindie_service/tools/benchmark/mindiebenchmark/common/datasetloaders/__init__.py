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

from queue import Queue

from mindiebenchmark.common.results import ResultData

from .base_loader import extract_dataset_type_info
from .boolq_loader import BoolqLoader
from .ceval_loader import CEvalLoader
from .cocotest_loader import CocotestLoader
from .gsm8k_loader import Gsm8kLoader
from .humaneval_loader import HumanevalLoader
from .mmlu_loader import MmluLoader
from .mtbench_loader import MtbenchLoader
from .oasst1_loader import Oasst1Loader
from .tokenid_loader import TokenidLoader
from .truthfulqa_loader import TruthfulqaLoader
from .synthetic_loader import SyntheticLoader
from .textvqa_loader import TextVQALoader
from .videobench_loader import VideoBenchLoader
from .vocalsound_loader import VocalsoundLoader


dataset_loader_map = {
    "ceval": CEvalLoader,
    "gsm8k": Gsm8kLoader,
    "oasst1": Oasst1Loader,
    "humaneval": HumanevalLoader,
    "boolq": BoolqLoader,
    "mmlu": MmluLoader,
    "truthfulqa": TruthfulqaLoader,
    "mtbench": MtbenchLoader,
    "cocotest": CocotestLoader,
    "synthetic": SyntheticLoader,
    "textvqa": TextVQALoader,
    "videobench": VideoBenchLoader,
    "vocalsound": VocalsoundLoader,
}


class DatasetLoaderFactory:
    @staticmethod
    def make_dataset_loader_instance(command_line_args: dict, queue: Queue, result: ResultData):
        if not command_line_args.get("Tokenizer"):
            return TokenidLoader(command_line_args, queue, result)

        dataset_name, _ = extract_dataset_type_info(command_line_args.get("DatasetType"))
        return dataset_loader_map.get(dataset_name)(command_line_args, queue, result)
