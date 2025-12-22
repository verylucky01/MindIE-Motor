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

import os.path

import pandas as pd

import mindiebenchmark.datasets.truthfulqa_eval as tqa

from .base_loader import BaseLoader


class TruthfulqaLoader(BaseLoader):
    def read_file(self, file_path: str):
        file_name = os.path.basename(file_path).split(".csv")[0]
        df = pd.read_csv(file_path)
        df.dropna(axis=1, how='all', inplace=True)
        count = 0
        for _, row in df.iterrows():
            if self.stop:
                return
            prompt = tqa.format_prompt(row)
            key = file_name + "_" + str(count)
            count += 1
            self.gt_answer[key] = tqa.parse_answers(row)
            self.put_data_dict({"id": key, "data": prompt})

    def get_file_posix(self) -> str:
        return self.CSV_POSIX
