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

import json
import os

import pandas as pd

from mindiebenchmark.common.logging import Logger
from .base_loader import BaseLoader, check_file, extract_dataset_type_info


class MmluLoader(BaseLoader):
    SHOT_NUM = 5

    @classmethod
    def validate_dataset_args(cls, dataset_type: str, command_line_args: dict):
        dataset_name, dataset_args = extract_dataset_type_info(dataset_type)
        if len(dataset_args) > 1:
            raise ValueError(f"argument --DatasetType: {dataset_name} supports no more than one dataset arg")
        max_shot_num = 30
        if dataset_args:
            if not str.isdigit(dataset_args[0]):
                raise ValueError(f"argument --DatasetType: {dataset_name}'s dataset arg should be a positive integer")
            cls.SHOT_NUM = int(dataset_args[0])
            if cls.SHOT_NUM > max_shot_num:
                raise ValueError(f"argument --DatasetType: {dataset_name}'s shot should be no more than {max_shot_num}")
        Logger().logger.info(f"Shot number of mmlu is {cls.SHOT_NUM}")

    def format_example_mmlu(self, val_data: dict, sub_name: str, dev_df: pd.DataFrame):
        train_prompt = "The following are multiple choice questions (with answers) about  "
        sub_name_rm_ = sub_name.replace("_", " ")
        train_prompt += sub_name_rm_ + ".\n\n"
        for _, row in dev_df.iterrows():
            line = json.dumps(row.to_dict())
            dev_ques_map = json.loads(line)
            train_prompt += self.splice_ques_options(dev_ques_map, True) + "\n"
        prompt_end = self.splice_ques_options(val_data) + "Answer:"
        return train_prompt + prompt_end

    def read_file(self, file_path: str):
        answer = "answer"
        col_names = ['question', 'A', 'B', 'C', 'D', answer]
        val_df = pd.read_csv(file_path, names=col_names).astype(str)
        file_name = os.path.basename(file_path)
        sub_name = file_name[: -len("_test.csv")]
        dev_path = os.path.join(self.dataset_path, "dev", sub_name + "_dev.csv")
        check_file(dev_path)
        dev_df = pd.read_csv(dev_path, names=col_names).astype(str)[:self.SHOT_NUM]
        count = 0
        for _, row in val_df.iterrows():
            if self.stop:
                return
            line = json.dumps(row.to_dict())
            question_map = json.loads(line)
            options = [
                question_map["A"],
                question_map["B"],
                question_map["C"],
                question_map["D"],
            ]
            prompt = self.format_example_mmlu(
                question_map, sub_name, dev_df
            )

            key = sub_name + "_" + str(count)
            count += 1
            self.gt_answer[key] = question_map[answer]
            self.put_data_dict({"id": key, "data": prompt, "options": options})

    def get_file_posix(self) -> str:
        return self.CSV_POSIX

    def get_sub_dir_name(self) -> str:
        return "test"
