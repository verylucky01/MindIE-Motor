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

from .base_loader import BaseLoader, check_file


class CEvalLoader(BaseLoader):
    def read_file(self, file_path: str):
        check_file(file_path)
        val_df = pd.read_csv(file_path).astype(str)
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
            complete_ques, sub_name = self._format_example_ceval(
                question_map, file_path
            )

            key = sub_name + '_' + question_map["id"]
            self.gt_answer[key] = question_map["answer"]
            self.put_data_dict({"id": key, "data": complete_ques, "options": options})

    def get_file_posix(self) -> str:
        return self.CSV_POSIX

    def get_sub_dir_name(self) -> str:
        return "val"

    def _format_example_ceval(self, val_data: dict, file_path: str):
        file_name = file_path.split(os.sep)[-1]
        sub_name = file_name[: -len("_val.csv")]

        if not self.test_accuracy:
            return self.splice_ques_options(val_data), sub_name
        shot = 5
        question_info = "The following are multiple choice questions (with answers) about  "
        sub_name_rm_ = sub_name.replace("_", " ")
        question_info += sub_name_rm_ + ".\n\n"

        dev_path = os.path.join(self.dataset_path, "dev", sub_name + "_dev.csv")
        check_file(dev_path)
        dev_df = pd.read_csv(dev_path).astype(str)[:shot]
        shot_info = ""
        for _, row in dev_df.iterrows():
            line = json.dumps(row.to_dict())
            dev_ques_map = json.loads(line)
            shot_info += self.splice_ques_options(dev_ques_map, True) + "\n"

        complete_ques = (
            question_info
            + shot_info
            + self.splice_ques_options(val_data)
            + "Answer:"
        )
        return complete_ques, sub_name
